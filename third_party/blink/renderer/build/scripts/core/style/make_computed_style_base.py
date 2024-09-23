#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math

import json5_generator
import template_expander
import keyword_utils
import bisect

from blinkbuild.name_style_converter import NameStyleConverter
from core.css import css_properties
from core.style.computed_style_fields import Enum, Group, Field

from itertools import chain

# Heuristic ordering of types from largest to smallest, used to sort fields by
# their alignment sizes.
# Specifying the exact alignment sizes for each type is impossible because it's
# platform specific, so we define an ordering instead.
# The ordering comes from the data obtained in:
# https://codereview.chromium.org/2841413002
# FIXME: Put alignment sizes into code form, rather than linking to a CL
# which may disappear.
ALIGNMENT_ORDER = [
    # Aligns like double
    'ScaleTransformOperation',
    'RotateTransformOperation',
    'TranslateTransformOperation',
    'NGGridTrackList',
    'StyleHighlightData',
    'FilterOperations',
    'DynamicRangeLimit',
    'ComputedGridTrackList',
    'std::optional<gfx::Size>',
    'double',
    'StyleViewTransitionGroup',
    # Aligns like a pointer (can be 32 or 64 bits)
    'NamedGridLinesMap',
    'NamedGridAreaMap',
    'TransformOperations',
    'Vector<CSSPropertyID>',
    'Vector<AtomicString>',
    'Vector<TimelineAttachment>',
    'Vector<TimelineAxis>',
    'Vector<TimelineInset>',
    'GridPosition',
    'ScrollStartData',
    'AtomicString',
    'scoped_refptr',
    'std::unique_ptr',
    'Vector<String>',
    'Font',
    'FillLayer',
    'NinePieceImage',
    'SVGPaint',
    'IntrinsicLength',
    'TextBoxEdge',
    'TextDecorationThickness',
    'StyleAnchorScope',
    'StyleAspectRatio',
    'StyleIntrinsicLength',
    'std::optional<StyleOverflowClipMargin>',
    'std::optional<blink::PositionAreaOffsets>',
    'std::optional<PhysicalOffset>',
    # Compressed builds a Member can be 32 bits, vs. a pointer will be 64.
    'Member',
    # Aligns like float
    'std::optional<Length>',
    'StyleInitialLetter',
    'StyleOffsetRotation',
    'TransformOrigin',
    'ScrollPadding',
    'ScrollMargin',
    'LengthBox',
    'LengthSize',
    'gfx::SizeF',
    'LengthPoint',
    'Length',
    'UnzoomedLength',
    'TextSizeAdjust',
    'TabSize',
    'float',
    # Aligns like int
    'cc::ScrollSnapType',
    'cc::ScrollSnapAlign',
    'BorderValue',
    'StyleColor',
    'StyleAutoColor',
    'Color',
    'StyleHyphenateLimitChars',
    'LayoutUnit',
    'LineClampValue',
    'OutlineValue',
    'unsigned',
    'size_t',
    'wtf_size_t',
    'int',
    'PositionArea',
    # Aligns like short
    'unsigned short',
    'short',
    # Aligns like char
    'StyleSelfAlignmentData',
    'StyleContentAlignmentData',
    'uint8_t',
    'char',
    # Aligns like bool
    'bool'
]

# FIXME: Improve documentation and add docstrings.


def _flatten_list(x):
    """Flattens a list of lists into a single list."""
    return list(chain.from_iterable(x))


def _get_include_paths(properties):
    """
    Get a list of paths that need to be included for ComputedStyleBase.
    """
    include_paths = set()
    for property_ in properties:
        include_paths.update(property_.include_paths)
    return list(sorted(include_paths))


def _create_groups(properties):
    """Create a tree of groups from a list of properties.

    Returns:
        Group: The root group of the tree. The name of the group is set to None.
    """

    # We first convert properties into a dictionary structure. Each dictionary
    # represents a group. The None key corresponds to the fields directly stored
    # on that group. The other keys map from group name to another dictionary.
    # For example:
    # {
    #   None: [field1, field2, ...]
    #   'groupA': { None: [field3] },
    #   'groupB': {
    #      None: [],
    #      'groupC': { None: [field4] },
    #   },
    # }
    #
    # We then recursively convert this dictionary into a tree of Groups.
    # FIXME: Skip the first step by changing Group attributes to methods.
    def _dict_to_group(name, group_dict):
        fields_in_current_group = group_dict.pop(None)
        subgroups = [
            _dict_to_group(subgroup_name, subgroup_dict)
            for subgroup_name, subgroup_dict in group_dict.items()
        ]
        return Group(name, subgroups, _reorder_fields(fields_in_current_group))

    root_group_dict = {None: []}
    for property_ in properties:
        current_group_dict = root_group_dict
        if property_.field_group:
            for group_name in property_.field_group.split('->'):
                current_group_dict[group_name] = current_group_dict.get(
                    group_name, {None: []})
                current_group_dict = current_group_dict[group_name]
        field, flag_field = _create_fields(property_)
        if field is not None:
            current_group_dict[None].append(field)

        # The flag field for this property, if any, should not be part of
        # the same group as the property; since it is not inherited
        # (you cannot inherit the inherit flag), that would always preclude
        # copy-on-write for the group when calling the inheriting constructor.
        if flag_field is not None:
            root_group_dict[None].append(flag_field)

    return _dict_to_group(None, root_group_dict)


def _mark_builder_flags(group):
    """Mark all fields as builder fields."""
    for field in group.fields:
        field.builder = True
    for subgroup in group.subgroups:
        _mark_builder_flags(subgroup)


def _create_builder_groups(properties):
    """Like _create_groups, but all fields returned by this function has
       the builder flag set to True."""
    groups = _create_groups(properties)
    _mark_builder_flags(groups)
    return groups


def _create_enums(properties):
    """Returns a list of Enums to be generated"""
    enums = {}
    for property_ in properties:
        # Only generate enums for keyword properties that do not
        # require includes.
        if (property_.field_template in ('keyword', 'multi_keyword',
                                         'bitset_keyword')
                and len(property_.include_paths) == 0):
            if property_.field_template == 'multi_keyword':
                set_type = 'multi'
            elif property_.field_template == 'bitset_keyword':
                set_type = 'bitset'
            else:
                set_type = None
            enum = Enum(property_.type_name,
                        property_.keywords,
                        set_type=set_type)
            if property_.field_template == 'multi_keyword':
                assert property_.keywords[0] == 'none', \
                    "First keyword in a 'multi_keyword' field must be " \
                    "'none' in '{}'.".format(property_.name)

            if enum.type_name in enums:
                # There's an enum with the same name, check if the enum
                # values are the same
                assert set(enums[enum.type_name].values) == set(enum.values), \
                    "'{}' can't have type_name '{}' because it was used by " \
                    "a previous property, but with a different set of " \
                    "keywords. Either give it a different name or ensure " \
                    "the keywords are the same.".format(
                        property_.name, enum.type_name)
            else:
                enums[enum.type_name] = enum

    # Return the enums sorted by type name
    return list(sorted(enums.values(), key=lambda e: e.type_name))


def _create_property_field(property_):
    """
    Create a property field.
    """
    name_for_methods = property_.name_for_methods

    assert property_.default_value is not None, \
        'MakeComputedStyleBase requires an default value for all fields, ' \
        'none specified for property ' + property_.name

    type_name = property_.type_name
    if property_.field_template == 'keyword':
        assert property_.field_size is None, \
            ("'" + property_.name + "' is a keyword field, "
             "so it should not specify a field_size")
        size = int(math.ceil(math.log(len(property_.keywords), 2)))
    elif property_.field_template == 'multi_keyword':
        size = len(property_.keywords) - 1  # Subtract 1 for 'none' keyword
    elif property_.field_template == 'bitset_keyword':
        size = len(property_.keywords)
    elif property_.field_template == 'external':
        size = None
    elif property_.field_template == 'primitive':
        # pack bools with 1 bit.
        size = 1 if type_name == 'bool' else property_.field_size
    elif property_.field_template == 'pointer':
        size = None
    elif property_.field_template == 'derived_flag':
        size = 2
    else:
        assert property_.field_template == 'monotonic_flag', \
            "Please use a valid value for field_template"
        size = 1

    return Field(
        'property',
        name_for_methods,
        property_name=property_.name.original,
        inherited=property_.inherited,
        independent=property_.independent,
        semi_independent_variable=property_.semi_independent_variable,
        type_name=property_.type_name,
        wrapper_pointer_name=property_.wrapper_pointer_name,
        field_template=property_.field_template,
        size=size,
        default_value=property_.default_value,
        invalidate=property_.invalidate,
        derived_from=property_.derived_from,
        reset_on_new_style=property_.reset_on_new_style,
        custom_compare=property_.custom_compare,
        mutable=property_.mutable,
        getter_method_name=property_.getter,
        setter_method_name=property_.setter,
        initial_method_name=property_.initial,
        computed_style_custom_functions=property_.
        computed_style_custom_functions,
        computed_style_protected_functions=property_.
        computed_style_protected_functions,
    )


def _create_inherited_flag_field(property_):
    """
    Create the field used for an inheritance fast path from an independent CSS
    property, and return the Field object.
    """
    name_for_methods = NameStyleConverter(
        property_.name_for_methods).to_function_name(
            suffix=['is', 'inherited'])
    name_source = NameStyleConverter(name_for_methods)
    return Field(
        'inherited_flag',
        name_for_methods,
        property_name=property_.name.original,
        type_name='bool',
        wrapper_pointer_name=None,
        field_template='primitive',
        size=1,
        default_value='true',
        derived_from=None,
        invalidate=[],
        reset_on_new_style=False,
        custom_compare=False,
        mutable=False,
        getter_method_name=name_source.to_function_name(),
        setter_method_name=name_source.to_function_name(prefix='set'),
        initial_method_name=name_source.to_function_name(prefix='initial'),
        computed_style_custom_functions=property_.
        computed_style_custom_functions,
        computed_style_protected_functions=property_.
        computed_style_protected_functions,
    )


def _create_fields(property_):
    """
    Create ComputedStyle fields from a property and return two Fields
    (of which the last, or both, may be None). The first Field is for
    the property itself. The second Field is a special boolean for
    independent properties that stores whether the property was set
    to the “inherit” value or not; it is returned separately because
    you may want to put it on the top level, not in a group.
    """
    field = None
    flag_field = None
    # Only generate properties that have a field template
    if property_.field_template is not None:
        # If the property is independent, add the single-bit sized isInherited
        # flag to the list of Fields as well.
        if property_.independent:
            flag_field = _create_inherited_flag_field(property_)

        field = _create_property_field(property_)

    return field, flag_field


def _reorder_bit_fields(bit_fields):
    # Since fields cannot cross word boundaries, in order to minimize
    # padding, group fields into buckets so that as many buckets as possible
    # are exactly 32 bits. Although this greedy approach may not always
    # produce the optimal solution, we add a static_assert to the code to
    # ensure ComputedStyleBase results in the expected size. If that
    # static_assert fails, this code is falling into the small number of
    # cases that are suboptimal, and may need to be rethought.
    # For more details on packing bit fields to reduce padding, see:
    # http://www.catb.org/esr/structure-packing/#_bitfields
    field_buckets = []
    # Consider fields in descending order of size to reduce fragmentation
    # when they are selected. Ties broken in alphabetical order by name.
    for field in sorted(bit_fields, key=lambda f: (-f.size, f.name)):
        added_to_bucket = False
        # Go through each bucket and add this field if it will not increase
        # the bucket's size to larger than 32 bits. Otherwise, make a new
        # bucket containing only this field.
        for bucket in field_buckets:
            if sum(f.size for f in bucket) + field.size <= 32:
                bucket.append(field)
                added_to_bucket = True
                break
        if not added_to_bucket:
            field_buckets.append([field])

    return _flatten_list(field_buckets)


def _reorder_non_bit_fields(non_bit_fields):
    # A general rule of thumb is to sort members by their alignment requirement
    # (from biggest aligned to smallest).
    for field in non_bit_fields:
        assert field.alignment_type in ALIGNMENT_ORDER, \
            "Type {} has unknown alignment. Please update ALIGNMENT_ORDER " \
            "to include it.".format(field.name)
    return list(
        sorted(
            non_bit_fields,
            key=lambda f: ALIGNMENT_ORDER.index(f.alignment_type)))


def _reorder_fields(fields):
    """
    Returns a list of fields ordered to minimise padding.
    """
    # Separate out bit fields from non bit fields
    bit_fields = [field for field in fields if field.is_bit_field]
    non_bit_fields = [field for field in fields if not field.is_bit_field]

    # Non bit fields go first, then the bit fields.
    return _reorder_non_bit_fields(non_bit_fields) + _reorder_bit_fields(
        bit_fields)


def _get_properties_ranking_using_partition_rule(properties_ranking,
                                                 partition_rule):
    """Take the contents of the properties ranking file and produce a dictionary
    of css properties with their group number based on the partition_rule

    Args:
        properties_ranking: rankings map as read from CSSPropertyRanking.json5
        partition_rule: cumulative distribution over properties_ranking

    Returns:
        dictionary with keys are css properties' name values are the group
        that each css properties belong to. Smaller group number is higher
        popularity in the ranking.
    """
    return dict(
        zip(properties_ranking, [
            bisect.bisect_left(partition_rule,
                               float(i) / len(properties_ranking)) + 1
            for i in range(len(properties_ranking))
        ]))


def _best_rank(prop, ranking_map):
    """Return the best ranking value for the specified property.

    This function collects ranking values for not only the property's real name
    but also its aliases, and returns the best (lower is better) value.
    If no ranking values for the property is available, this returns -1.
    """
    worst_rank = max(ranking_map.values()) + 1
    best_rank = ranking_map.get(prop.name.original, worst_rank)

    for alias_name in prop.aliases:
        best_rank = min(best_rank, ranking_map.get(alias_name, worst_rank))
    return best_rank if best_rank != worst_rank else -1


def _evaluate_rare_non_inherited_group(properties,
                                       properties_ranking,
                                       num_layers,
                                       partition_rule=None):
    """Re-evaluate the grouping of RareNonInherited groups based on each
    property's popularity.

    Args:
        properties: list of all css properties
        properties_ranking: map of property rankings
        num_layers: the number of group to split
        partition_rule: cumulative distribution over properties_ranking
                        Ex: [0.3, 0.6, 1]
    """
    if partition_rule is None:
        partition_rule = [
            1.0 * (i + 1) / num_layers for i in range(num_layers)
        ]

    assert num_layers == len(partition_rule), \
        "Length of rule and num_layers mismatch"

    layers_name = [
        "rare-non-inherited-usage-less-than-{}-percent".format(
            int(round(partition_rule[i] * 100))) for i in range(num_layers)
    ]
    properties_ranking = _get_properties_ranking_using_partition_rule(
        properties_ranking, partition_rule)

    for property_ in properties:
        rank = _best_rank(property_, properties_ranking)
        if (property_.field_group is not None and "*" in property_.field_group
                and not property_.inherited and rank >= 0):

            assert property_.field_group == "*", \
                "The property {}  will be automatically assigned a group, " \
                "please put '*' as the field_group".format(property_.name)

            property_.field_group = "->".join(layers_name[0:rank])
        elif (property_.field_group is not None
              and "*" in property_.field_group and not property_.inherited
              and rank < 0):
            group_tree = property_.field_group.split("->")[1:]
            group_tree = [layers_name[0], layers_name[0] + "-sub"] + group_tree
            property_.field_group = "->".join(group_tree)


def _evaluate_rare_inherit_group(properties,
                                 properties_ranking,
                                 num_layers,
                                 partition_rule=None):
    """Re-evaluate the grouping of RareInherited groups based on each property's
    popularity.

    Args:
        properties: list of all css properties
        properties_ranking: map of property rankings
        num_layers: the number of group to split
        partition_rule: cumulative distribution over properties_ranking
                        Ex: [0.4, 1]
    """
    if partition_rule is None:
        partition_rule = [
            1.0 * (i + 1) / num_layers for i in range(num_layers)
        ]

    assert num_layers == len(partition_rule), \
        "Length of rule and num_layers mismatch"

    layers_name = [
        "rare-inherited-usage-less-than-{}-percent".format(
            int(round(partition_rule[i] * 100))) for i in range(num_layers)
    ]

    properties_ranking = _get_properties_ranking_using_partition_rule(
        properties_ranking, partition_rule)

    for property_ in properties:
        rank = _best_rank(property_, properties_ranking)
        if (property_.field_group is not None and "*" in property_.field_group
                and property_.inherited and rank >= 0):
            property_.field_group = "->".join(layers_name[0:rank])
        elif (property_.field_group is not None
              and "*" in property_.field_group and property_.inherited
              and rank < 0):
            group_tree = property_.field_group.split("->")[1:]
            group_tree = [layers_name[0], layers_name[0] + "-sub"] + group_tree
            property_.field_group = "->".join(group_tree)


class ComputedStyleBaseWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(ComputedStyleBaseWriter, self).__init__([], output_dir)

        self._input_files = json5_file_paths

        # Reads css_properties.json5, computed_style_field_aliases.json5,
        # runtime_enabled_features.json5 and computed_style_extra_fields.json5
        self._css_properties = css_properties.CSSProperties(
            json5_file_paths[0:4])

        # We sort the enum values based on each value's position in
        # the keywords as listed in css_properties.json5. This will ensure that
        # if there is a continuous
        # segment in css_properties.json5 matching the segment in this enum then
        # the generated enum will have the same order and continuity as
        # css_properties.json5 and we can get the longest continuous segment.
        # Thereby reduce the switch case statement to the minimum.
        properties = keyword_utils.sort_keyword_properties_by_canonical_order(
            self._css_properties.longhands, json5_file_paths[4],
            self.default_parameters)
        self._properties = properties + self._css_properties.extra_fields
        self._longhands = [p for p in properties if p.is_longhand]

        self._generated_enums = _create_enums(self._properties)
        self._diff_enum = [
            NameStyleConverter(value).to_enum_value()
            for value in self._css_properties.default_parameters["invalidate"]
            ["valid_values"]
        ]

        # Organise fields into a tree structure where the root group
        # is ComputedStyleBase.
        group_parameters = dict([
            (conf["name"], conf["cumulative_distribution"])
            for conf in json5_generator.Json5File.load_from_files(
                [json5_file_paths[6]]).name_dictionaries
        ])

        properties_ranking = [
            x["name"].original
            for x in json5_generator.Json5File.load_from_files(
                [json5_file_paths[5]]).name_dictionaries
        ]
        _evaluate_rare_non_inherited_group(
            self._properties, properties_ranking,
            len(group_parameters["rare_non_inherited_properties_rule"]),
            group_parameters["rare_non_inherited_properties_rule"])
        _evaluate_rare_inherit_group(
            self._properties, properties_ranking,
            len(group_parameters["rare_inherited_properties_rule"]),
            group_parameters["rare_inherited_properties_rule"])
        self._root_group = _create_groups(self._properties)
        # We create separate groups/fields for generating ComputedStyle-
        # BuilderBase. The only difference between these fields and the regular
        # fields, is that the builder fields have the "builder" flag set, which
        # is used to tweak the code generation in the field templates.
        #
        # TODO(crbug.com/1377295): When the builder is fully deployed, we no
        #                          longer need two groups.
        self._root_builder_group = _create_builder_groups(self._properties)

        self._include_paths = _get_include_paths(self._properties)
        self._outputs = {
            'computed_style_base.h':
            self.generate_base_computed_style_h,
            'computed_style_base.cc':
            self.generate_base_computed_style_cpp,
            'computed_style_base_constants.h':
            self.generate_base_computed_style_constants_h,
            'computed_style_base_constants.cc':
            self.generate_base_computed_style_constants_cc,
        }

    @template_expander.use_jinja(
        'core/style/templates/computed_style_base.h.tmpl',
        tests={
            'in': lambda a, b: a in b
        })
    def generate_base_computed_style_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'longhands': self._longhands,
            'enums': self._generated_enums,
            'include_paths': self._include_paths,
            'computed_style': self._root_group,
            'computed_style_builder': self._root_builder_group,
            'diff_enum': self._diff_enum,
        }

    @template_expander.use_jinja(
        'core/style/templates/computed_style_base.cc.tmpl',
        tests={
            'in': lambda a, b: a in b
        })
    def generate_base_computed_style_cpp(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
            'include_paths': self._include_paths,
            'computed_style': self._root_group,
        }

    @template_expander.use_jinja(
        'core/style/templates/computed_style_base_constants.h.tmpl')
    def generate_base_computed_style_constants_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
        }

    @template_expander.use_jinja(
        'core/style/templates/computed_style_base_constants.cc.tmpl')
    def generate_base_computed_style_constants_cc(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'enums': self._generated_enums,
        }


if __name__ == '__main__':
    json5_generator.Maker(ComputedStyleBaseWriter).main()
