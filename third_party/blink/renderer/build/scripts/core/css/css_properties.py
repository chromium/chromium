#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkbuild.name_style_converter import NameStyleConverter
from core.css.field_alias_expander import FieldAliasExpander
import json5_generator
from make_origin_trials import OriginTrialsWriter
from name_utilities import enum_key_for_css_property, id_for_css_property
from name_utilities import enum_key_for_css_property_alias, id_for_css_property_alias
import dataclasses
import typing
import copy

# These values are converted using CSSPrimitiveValue in the setter function,
# if applicable.
PRIMITIVE_TYPES = [
    'short', 'unsigned short', 'int', 'unsigned int', 'unsigned', 'float',
    'LineClampValue'
]


def validate_property(prop, props_by_name):
    """Perform sanity-checks on a property entry.

    prop: The property (or extra field) to perform checks on.
    props_by_name: A dict which maps properties by name. This is useful for
                   cases where 'prop' refer to other properties by name.

    Many combinations of values that are possible to specify in
    css_properties.json5 do not make sense, and/or would produce invalid
    generated code. For example, it does not make sense for longhands to
    implement the ParseShorthand function.
    """
    name = prop.name
    has_method = lambda x: x in prop.property_methods
    assert prop.is_property or prop.is_descriptor, \
        'Entry must be a property, descriptor, or both [%s]' % name
    assert not prop.interpolable or prop.is_longhand, \
        'Only longhands can be interpolable [%s]' % name
    assert not has_method('ParseSingleValue') or prop.is_longhand, \
        'Only longhands can implement ParseSingleValue [%s]' % name
    assert not has_method('ParseShorthand') or prop.is_shorthand, \
        'Only shorthands can implement ParseShorthand [%s]' % name
    assert not prop.field_template or prop.is_longhand, \
        'Only longhands can have a field_template [%s]' % name
    assert not prop.valid_for_first_letter or prop.is_longhand, \
        'Only longhands can be valid_for_first_letter [%s]' % name
    assert not prop.valid_for_first_line or prop.is_longhand, \
        'Only longhands can be valid_for_first_line [%s]' % name
    assert not prop.valid_for_cue or prop.is_longhand, \
        'Only longhands can be valid_for_cue [%s]' % name
    assert not prop.valid_for_marker or prop.is_longhand, \
        'Only longhands can be valid_for_marker [%s]' % name
    assert not prop.valid_for_highlight_legacy or prop.is_longhand, \
        'Only longhands can be valid_for_highlight_legacy [%s]' % name
    assert not prop.valid_for_highlight or prop.is_longhand, \
        'Only longhands can be valid_for_highlight [%s]' % name
    assert not prop.is_internal or prop.computable is None, \
        'Internal properties are always non-computable [%s]' % name
    if prop.supports_incremental_style:
        assert not prop.is_animation_property, \
            'Animation properties can not be applied incrementally [%s]' % name
        assert prop.idempotent, \
            'Incrementally applied properties must be idempotent [%s]' % name
        if prop.is_shorthand:
            for subprop_name in prop.longhands:
                subprop = props_by_name[subprop_name]
                assert subprop.supports_incremental_style, \
                    '%s must be incrementally applicable when its shorthand %s is' % (subprop_name, name)
    if prop.alias_for:
        assert not prop.is_internal, \
            'Internal aliases not supported [%s]' % name
    assert not prop.mutable or \
        (prop.field_template in ['derived_flag', 'monotonic_flag'] ),\
        'mutable requires field_template:derived_flag or monotonic_flag [%s]' % name
    assert not prop.in_origin_trial or prop.runtime_flag,\
        'Property participates in origin trial, but has no runtime flag'
    custom_functions = set(prop.computed_style_custom_functions)
    protected_functions = set(set(prop.computed_style_protected_functions))
    assert not custom_functions.intersection(protected_functions), \
        'Functions must be specified as either protected or custom, not both [%s]' % name
    if prop.field_template == 'derived_flag':
        assert prop.mutable, 'Derived flags must be mutable [%s]' % name
        assert not prop.field_group, 'Derived flags may not have field groups [%s]' % name
        assert prop.reset_on_new_style, 'Derived flags must have reset_on_new_style [%s]' % name
    if prop.is_logical:
        assert not prop.field_group, 'Logical properties can not have fields [%s]' % name

# Determines whether or not style builders (i.e. Apply functions)
# should be generated for the given property.
def needs_style_builders(property_):
    if not property_.is_property:
        return False
    # Shorthands do not get style builders, because shorthands are
    # expanded to longhands parse-time.
    if property_.longhands:
        return False
    # Surrogates do not get style builders, because they are replaced
    # with another target property cascade-time.
    if property_.surrogate_for:
        return False
    # Logical properties do not get style builders for the same reason
    # as surrogates.
    if property_.is_logical:
        return False
    return True


def verify_file_path(file_paths, index, expected):
    assert len(file_paths) > index and file_paths[index].endswith(expected), \
        'Unexpected file path at index %s (expected path that ends with %s, got .../%s)' \
            % (index, expected, file_paths[index])
    return file_paths[index]


class PropertyBase(object):
    """Base class for the generated 'Property' class.

    This class provides utility functions on top of 'Property', which is
    generated by 'generate_property_class'. PropertyBase is not intended to
    be instantiated directly, and expects all 'parameters' [1] to exist on
    'self'.

    [1] See 'parameters' dictionary in css_properties.json5.
    """

    def __init__(self):
        super(PropertyBase, self).__init__()

    @property
    def namespace(self):
        """The namespace for the generated CSSProperty subclass."""
        if self.is_shorthand:
            return 'css_shorthand'
        # Otherwise, 'self' is a longhand, or a descriptor (which also ends up
        # in the css_longhand namespace).
        return 'css_longhand'

    @property
    def classname(self):
        """The name of the generated CSSProperty subclass."""
        return self.name.to_upper_camel_case()

    @property
    def is_longhand(self):
        return self.is_property and not self.longhands

    @property
    def is_shorthand(self):
        return self.is_property and self.longhands

    @property
    def is_internal(self):
        return self.name.original.startswith('-internal-')

    @property
    def known_exposed(self):
        """True if the property is unconditionally web-exposed."""
        return not self.is_internal \
            and not self.runtime_flag \
            and not self.alternative

    @property
    def ultimate_property(self):
        """Returns the ultimate property, which is the final property
            in the alternative_of chain."""
        if self.alternative_of:
            return self.alternative_of.ultimate_property
        return self

    @property
    def css_sample_id(self):
        """Returns the CSSSampleId to use for this property."""
        # Alternative properties use the same use-counter as the
        # corresponding ultimate main property. In other words, alternative
        # properties are use-counted the same way as their main properties.
        return self.ultimate_property.enum_key


def generate_property_field(default):
    # Must use 'default_factory' rather than 'default' for list/dict.
    # https://docs.python.org/3/library/dataclasses.html#dataclasses.field
    if isinstance(default, list):
        return dataclasses.field(default_factory=list)
    if isinstance(default, dict):
        return dataclasses.field(default_factory=dict)
    return dataclasses.field(default=default)


def generate_property_class(parameters):
    """Generate a Property dataclass based on 'parameters' found in json5.

    See documentation about "parameters" in json5_generator.py.
    """
    # Fields and their default values, as specified in a json5-file:
    fields = [(name, spec.get('default', None))
              for name, spec in parameters.items()]

    # Additional defaults not specified in json5:
    additional = {
        'aliases': [],
        'custom_compare': False,
        'reset_on_new_style': False,
        'mutable': False,
        'name': None,
        'alternative': None,
        'visited_property': None,
    }

    fields += additional.items()

    return dataclasses.make_dataclass('Property', \
        [(name, typing.Any, generate_property_field(default)) for name, default in fields], \
        bases=(PropertyBase,))


class CSSProperties(object):
    def __init__(self, file_paths):
        assert len(
            file_paths) <= 4, 'Superfluous arguments: %s' % file_paths[4:]

        css_properties_path = verify_file_path(file_paths, 0,
                                               'css_properties.json5')
        computed_style_field_aliases_path = verify_file_path(
            file_paths, 1, 'computed_style_field_aliases.json5')
        runtime_enabled_features_path = verify_file_path(
            file_paths, 2, 'runtime_enabled_features.json5')
        # Extra fields are optional:
        computed_style_extra_fields_path = (
            len(file_paths) > 3) and verify_file_path(
                file_paths, 3, 'computed_style_extra_fields.json5')

        # computed_style_field_aliases.json5. Used to expand out parameters used
        # in the various generators for ComputedStyle.
        self._field_alias_expander = FieldAliasExpander(
            computed_style_field_aliases_path)

        # _alias_offset is updated in add_properties().
        self._alias_offset = -1
        # 0: CSSPropertyID::kInvalid
        # 1: CSSPropertyID::kVariable
        self._first_enum_value = 2
        self._last_used_enum_value = self._first_enum_value
        self._last_high_priority_property = None

        self._properties_by_id = {}
        self._aliases = []
        self._longhands = []
        self._shorthands = []
        self._properties_including_aliases = []

        # Add default data in css_properties.json5. This must be consistent
        # across instantiations of this class.
        css_properties_file = json5_generator.Json5File.load_from_files(
            [css_properties_path])
        self._default_parameters = css_properties_file.parameters

        Property = generate_property_class(self._default_parameters)

        # TODO(crbug/1031309): Refactor OriginTrialsWriter to reuse logic here.
        origin_trials_writer = OriginTrialsWriter(
            [runtime_enabled_features_path], "")
        self._origin_trial_features = {
            str(f['name'])
            for f in origin_trials_writer.origin_trial_features
        }

        properties = [
            Property(**x) for x in css_properties_file.name_dictionaries
        ]

        # Process extra fields, if any.
        self._extra_fields = []
        if computed_style_extra_fields_path:
            fields = json5_generator.Json5File.load_from_files(
                [computed_style_extra_fields_path],
                default_parameters=self._default_parameters)
            self._extra_fields = [
                Property(**x) for x in fields.name_dictionaries
            ]

        self._properties_by_name = {p.name.original: p for p in properties}

        for property_ in properties + self._extra_fields:
            self.set_derived_attributes(property_)
            validate_property(property_, self._properties_by_name)

        self.add_properties(properties)

        self._last_unresolved_property_id = max(property_.enum_value
                                                for property_ in self._aliases)

    def add_properties(self, properties):
        self._aliases = [
            property_ for property_ in properties if property_.alias_for
        ]
        self._shorthands = [
            property_ for property_ in properties if property_.longhands
        ]
        self._longhands = [
            property_ for property_ in properties
            if (not property_.alias_for and not property_.longhands)
        ]

        # Sort the properties by priority, then alphabetically. Ensure that
        # the resulting order is deterministic.
        # Sort properties by priority, then alphabetically.
        for property_ in self._longhands + self._shorthands:
            name_without_leading_dash = property_.name.original
            if name_without_leading_dash.startswith('-'):
                name_without_leading_dash = name_without_leading_dash[1:]
            property_.sorting_key = (-property_.priority,
                                     name_without_leading_dash)

        sorting_keys = {}
        for property_ in self._longhands + self._shorthands:
            key = property_.sorting_key
            assert key not in sorting_keys, \
                ('Collision detected - two properties have the same name and '
                 'priority, a potentially non-deterministic ordering can '
                 'occur: {}, {} and {}'.format(
                     key, property_.name.original, sorting_keys[key]))
            sorting_keys[key] = property_.name.original
        self._longhands.sort(key=lambda p: p.sorting_key)
        self._shorthands.sort(key=lambda p: p.sorting_key)

        # The sorted index becomes the CSSPropertyID enum value.
        for property_ in self._longhands + self._shorthands:
            property_.enum_value = self._last_used_enum_value
            self._last_used_enum_value += 1
            # Add the new property into the map of properties.
            assert property_.property_id not in self._properties_by_id, \
                ('property with ID {} appears more than once in the '
                 'properties list'.format(property_.property_id))
            self._properties_by_id[property_.property_id] = property_
            if property_.priority > 0:
                self._last_high_priority_property = property_

        self._alias_offset = self._last_used_enum_value
        self.expand_aliases()
        self._properties_including_aliases = self._longhands + \
            self._shorthands + self._aliases
        self._properties_with_alternatives = list(
            filter(lambda p: p.alternative,
                   self._properties_including_aliases))

    def get_property(self, name):
        assert name in self._properties_by_name, \
            'No property with that name [%s]' % name
        return self._properties_by_name[name]

    def set_derived_visited_attributes(self, property_):
        if not property_.visited_property_for:
            return
        visited_property_for = property_.visited_property_for
        unvisited_property = self._properties_by_name[visited_property_for]
        property_.visited = True
        # The visited property needs a link to the unvisited counterpart.
        property_.unvisited_property = unvisited_property
        # The unvisited property needs a link to the visited counterpart.
        assert not unvisited_property.visited_property, \
            'A property may not have multiple visited properties'
        unvisited_property.visited_property = property_

    def set_derived_surrogate_attributes(self, property_):
        if not property_.surrogate_for:
            return
        assert property_.surrogate_for in self._properties_by_name, \
            'surrogate_for must name a property'
        # Upgrade 'surrogate_for' to property reference.
        property_.surrogate_for = self._properties_by_name[
            property_.surrogate_for]

    def set_derived_alternative_attributes(self, property_):
        if not property_.alternative_of:
            return
        main_property = self.get_property(property_.alternative_of)
        # Upgrade 'alternative_of' to a property reference.
        property_.alternative_of = main_property
        assert not main_property.alternative, \
            'A property may not have multiple alternatives'
        main_property.alternative = property_

    def expand_aliases(self):
        for i, alias in enumerate(self._aliases):
            aliased_property = self._properties_by_id[id_for_css_property(
                alias.alias_for)]
            aliased_property.aliases.append(alias.name.original)
            updated_alias = copy.deepcopy(aliased_property)
            updated_alias.name = alias.name
            updated_alias.alias_for = alias.alias_for
            updated_alias.alternative_of = alias.alternative_of
            updated_alias.alternative = alias.alternative
            updated_alias.aliased_property = aliased_property.name.to_upper_camel_case(
            )
            updated_alias.computable = alias.computable
            updated_alias.property_id = id_for_css_property_alias(alias.name)
            updated_alias.enum_key = enum_key_for_css_property_alias(
                alias.name)
            updated_alias.enum_value = self._alias_offset + i
            updated_alias.aliased_enum_value = aliased_property.enum_value
            updated_alias.superclass = 'CSSUnresolvedProperty'
            updated_alias.namespace_group = \
                'Shorthand' if aliased_property.longhands else 'Longhand'
            self._aliases[i] = updated_alias

        updated_aliases_by_name = {a.name: a for a in self._aliases}

        # The above loop produces an "updated" alias for each (incoming) alias.
        # Any alternative_of/alternative references that point to aliases
        # must be updated to point to the respective "updated" aliases.
        def update_alternatives(properties):
            for _property in properties:
                if _property.alternative_of and _property.alternative_of.alias_for:
                    _property.alternative_of = updated_aliases_by_name[
                        _property.alternative_of.name]
                if _property.alternative and _property.alternative.alias_for:
                    _property.alternative = updated_aliases_by_name[
                        _property.alternative.name]

        update_alternatives(self.longhands)
        update_alternatives(self.shorthands)
        update_alternatives(self.aliases)

    def set_derived_attributes(self, property_):
        """Set new attributes on 'property_', based on existing attribute values
        or defaults.

        The 'Property' class (of which 'property_' is an instance) contains an
        attribute for every "parameter" [1] specified in css_properties.json5.
        However, these attributes are not always sufficient or ergonomic to use
        in template files. For example, the 'field_template' parameter is
        shortcut for specifying a bunch of other parameters, and that expansion
        happens here.

        For trivial derived attributes, such as an 'is_internal' attribute which
        just checks if the name starts with "-internal-", prefer an @property
        on PropertyBase instead.

        [1] See "parameters" dictionary in css_properties.json5.
        """

        def set_if_none(property_, key, value):
            if not getattr(property_, key, None):
                setattr(property_, key, value)

        # Basic info.
        name = property_.name
        property_.property_id = id_for_css_property(name)
        property_.enum_key = enum_key_for_css_property(name)
        method_name = property_.name_for_methods
        if not method_name:
            method_name = name.to_upper_camel_case().replace('Webkit', '')
        set_if_none(property_, 'inherited', False)

        # Initial function, Getters and Setters for ComputedStyle.
        set_if_none(property_, 'initial', 'Initial' + method_name)
        simple_type_name = str(property_.type_name).split('::')[-1]
        set_if_none(property_, 'name_for_methods', method_name)
        set_if_none(property_, 'type_name', 'E' + method_name)
        set_if_none(
            property_, 'getter', method_name
            if simple_type_name != method_name else 'Get' + method_name)
        set_if_none(property_, 'setter', 'Set' + method_name)
        if property_.inherited:
            property_.is_inherited_setter = ('Set' + method_name +
                                             'IsInherited')

        property_.is_logical = False

        if property_.logical_property_group:
            group = property_.logical_property_group
            assert 'name' in group, 'name option is required'
            assert 'resolver' in group, 'resolver option is required'
            logicals = {
                'block', 'inline', 'block-start', 'block-end', 'inline-start',
                'inline-end', 'start-start', 'start-end', 'end-start',
                'end-end'
            }
            physicals = {
                'vertical', 'horizontal', 'top', 'bottom', 'left', 'right',
                'top-left', 'top-right', 'bottom-right', 'bottom-left'
            }
            if group['resolver'] in logicals:
                group['is_logical'] = True
            elif group['resolver'] in physicals:
                group['is_logical'] = False
            else:
                assert 0, 'invalid resolver option'
            group['name'] = NameStyleConverter(group['name'])
            group['resolver_name'] = NameStyleConverter(group['resolver'])
            property_.is_logical = group['is_logical']

        property_.style_builder_declare = needs_style_builders(property_)

        # Figure out whether we should generate style builder implementations.
        for x in ['initial', 'inherit', 'value']:
            suppressed = x in property_.style_builder_custom_functions
            declared = property_.style_builder_declare
            setattr(property_, 'style_builder_generate_%s' % x,
                    (declared and not suppressed))

        # Expand StyleBuilderConverter params where necessary.
        if property_.type_name in PRIMITIVE_TYPES:
            set_if_none(property_, 'converter', 'CSSPrimitiveValue')
        else:
            set_if_none(property_, 'converter', 'CSSIdentifierValue')

        if property_.anchor_mode:
            property_.anchor_mode = NameStyleConverter(property_.anchor_mode)

        if not property_.longhands:
            property_.superclass = 'Longhand'
            property_.namespace_group = 'Longhand'
        elif property_.longhands:
            property_.superclass = 'Shorthand'
            property_.namespace_group = 'Shorthand'

        # Expand out field templates.
        if property_.field_template:
            self._field_alias_expander.expand_field_alias(property_)

            type_name = property_.type_name
            if (property_.field_template == 'keyword'
                    or property_.field_template == 'multi_keyword'
                    or property_.field_template == 'bitset_keyword'):
                default_value = (type_name + '::' + NameStyleConverter(
                    property_.default_value).to_enum_value())
            elif (property_.field_template == 'external'
                  or property_.field_template == 'primitive'
                  or property_.field_template == 'pointer'):
                default_value = property_.default_value
            elif property_.field_template == 'derived_flag':
                property_.type_name = 'unsigned'
                default_value = '0'
            else:
                assert property_.field_template == 'monotonic_flag', \
                    "Please put a valid value for field_template; got " + \
                    str(property_.field_template)
                property_.type_name = 'bool'
                default_value = 'false'
            property_.default_value = default_value

            property_.unwrapped_type_name = property_.type_name
            if property_.wrapper_pointer_name:
                assert property_.field_template in ['pointer', 'external']
                if property_.field_template == 'external':
                    property_.type_name = '{}<{}>'.format(
                        property_.wrapper_pointer_name, type_name)

        # Default values for extra parameters in computed_style_extra_fields.json5.
        set_if_none(property_, 'reset_on_new_style', False)
        set_if_none(property_, 'custom_compare', False)
        set_if_none(property_, 'mutable', False)

        property_.in_origin_trial = property_.runtime_flag and \
            property_.runtime_flag in self._origin_trial_features

        self.set_derived_visited_attributes(property_)
        self.set_derived_surrogate_attributes(property_)
        self.set_derived_alternative_attributes(property_)

    @property
    def default_parameters(self):
        return self._default_parameters

    @property
    def aliases(self):
        return self._aliases

    @property
    def computable(self):
        # Use the name of the ultimate property as the sorting key,
        # otherwise '-alternative-foo' will sort according to
        # '-alternative-...', when it will really be exposed to
        # parsing/serialization as just 'foo'.
        sorting_name = lambda p: p.ultimate_property.name.original
        is_prefixed = lambda p: sorting_name(p).startswith('-')
        is_not_prefixed = lambda p: not is_prefixed(p)

        prefixed = filter(is_prefixed, self._properties_including_aliases)
        unprefixed = filter(is_not_prefixed,
                            self._properties_including_aliases)

        def is_computable(p):
            if p.is_internal:
                return False
            if p.computable is not None:
                return p.computable
            if p.alias_for:
                return False
            if not p.is_property:
                return False
            if not p.is_longhand:
                return False
            return True

        prefixed = filter(is_computable, prefixed)
        unprefixed = filter(is_computable, unprefixed)

        return sorted(unprefixed, key=sorting_name) + \
            sorted(prefixed, key=sorting_name)

    @property
    def shorthands(self):
        return self._shorthands

    @property
    def shorthands_including_aliases(self):
        return self._shorthands + [x for x in self._aliases if x.longhands]

    @property
    def longhands(self):
        return self._longhands

    @property
    def longhands_including_aliases(self):
        return self._longhands + [x for x in self._aliases if not x.longhands]

    @property
    def properties_by_name(self):
        return self._properties_by_name

    @property
    def properties_by_id(self):
        return self._properties_by_id

    @property
    def properties_including_aliases(self):
        return self._properties_including_aliases

    @property
    def properties_with_alternatives(self):
        """Properties that have another property referencing it with 'alternative_of'."""
        return self._properties_with_alternatives

    @property
    def gperf_properties(self):
        """The CSS properties that should be passed to gperf.

        This excludes properties with 'alternative_of' set, because such properties
        have the same web-facing name as the main property.
        """
        non_alternative = lambda p: not p.alternative_of
        return list(filter(non_alternative,
                           self._properties_including_aliases))

    @property
    def first_property_id(self):
        return self._first_enum_value

    @property
    def last_property_id(self):
        return self._first_enum_value + len(self._properties_by_id) - 1

    @property
    def last_unresolved_property_id(self):
        return self._last_unresolved_property_id

    @property
    def last_high_priority_property_id(self):
        return self._last_high_priority_property.enum_key

    @property
    def property_id_bit_length(self):
        return int.bit_length(self._last_unresolved_property_id)

    @property
    def alias_offset(self):
        return self._alias_offset

    @property
    def extra_fields(self):
        return self._extra_fields

    @property
    def max_shorthand_expansion(self):
        return max(map(lambda s: len(s.longhands), self._shorthands))
