#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkbuild.name_style_converter import NameStyleConverter
from core.css.field_alias_expander import FieldAliasExpander
import json5_generator
from make_origin_trials import OriginTrialsWriter
from name_utilities import enum_key_for_css_property, id_for_css_property
from name_utilities import enum_key_for_css_property_alias, id_for_css_property_alias

# These values are converted using CSSPrimitiveValue in the setter function,
# if applicable.
PRIMITIVE_TYPES = [
    'short', 'unsigned short', 'int', 'unsigned int', 'unsigned', 'float',
    'LineClampValue'
]


def validate_property(prop):
    name = prop['name']
    has_method = lambda x: x in prop['property_methods']
    assert prop['is_property'] or prop['is_descriptor'], \
        'Entry must be a property, descriptor, or both [%s]' % name
    assert not prop['interpolable'] or prop['is_longhand'], \
        'Only longhands can be interpolable [%s]' % name
    assert not has_method('ParseSingleValue') or prop['is_longhand'], \
        'Only longhands can implement ParseSingleValue [%s]' % name
    assert not has_method('ParseShorthand') or prop['is_shorthand'], \
        'Only shorthands can implement ParseShorthand [%s]' % name
    assert not prop['field_template'] or prop['is_longhand'], \
        'Only longhands can have a field_template [%s]' % name
    assert not prop['valid_for_first_letter'] or prop['is_longhand'], \
        'Only longhands can be valid_for_first_letter [%s]' % name
    assert not prop['valid_for_cue'] or prop['is_longhand'], \
        'Only longhands can be valid_for_cue [%s]' % name
    assert not prop['valid_for_marker'] or prop['is_longhand'], \
        'Only longhands can be valid_for_marker [%s]' % name


def validate_alias(alias):
    name = alias['name']
    is_internal = lambda x: x['name'].original.startswith('-internal-')
    assert not alias['runtime_flag'], \
        'Runtime flags are not supported for aliases [%s]' % name
    assert not is_internal(alias), \
        'Internal aliases not supported [%s]' % name


def validate_field(field):
    name = field['name']
    assert not field['mutable'] or field['field_template'] == 'monotonic_flag',\
        'mutable requires field_template:monotonic_flag [%s]' % name


class CSSProperties(object):
    def __init__(self, file_paths):
        assert len(file_paths) >= 3, \
            "CSSProperties at least needs both css_properties.json5, \
            computed_style_field_aliases.json5 and \
            runtime_enabled_features.json5 to function"

        # computed_style_field_aliases.json5. Used to expand out parameters used
        # in the various generators for ComputedStyle.
        self._field_alias_expander = FieldAliasExpander(file_paths[1])

        self._alias_offset = 1024
        # 0: CSSPropertyID::kInvalid
        # 1: CSSPropertyID::kVariable
        self._first_enum_value = 2
        self._last_used_enum_value = self._first_enum_value

        self._properties_by_id = {}
        self._properties_by_name = {}
        self._aliases = []
        self._longhands = []
        self._shorthands = []
        self._properties_including_aliases = []

        # Add default data in css_properties.json5. This must be consistent
        # across instantiations of this class.
        css_properties_file = json5_generator.Json5File.load_from_files(
            [file_paths[0]])
        self._default_parameters = css_properties_file.parameters
        # Map of feature name -> origin trial feature name
        origin_trial_features = {}
        # TODO(crbug/1031309): Refactor OriginTrialsWriter to reuse logic here.
        origin_trials_writer = OriginTrialsWriter([file_paths[2]], "")
        for feature in origin_trials_writer.origin_trial_features:
            origin_trial_features[str(feature['name'])] = True

        self.add_properties(css_properties_file.name_dictionaries,
                            origin_trial_features)

        assert self._first_enum_value + len(self._properties_by_id) < \
            self._alias_offset, \
            'Property aliasing expects fewer than %d properties.' % \
            self._alias_offset
        self._last_unresolved_property_id = max(
            property_["enum_value"] for property_ in self._aliases)

        # Process extra files passed in.
        self._extra_fields = []
        for i in range(3, len(file_paths)):
            fields = json5_generator.Json5File.load_from_files(
                [file_paths[i]], default_parameters=self._default_parameters)
            self._extra_fields.extend(fields.name_dictionaries)
        for field in self._extra_fields:
            self.expand_parameters(field)
            validate_field(field)

    def add_properties(self, properties, origin_trial_features):
        for property_ in properties:
            self._properties_by_name[property_['name'].original] = property_

        for property_ in properties:
            property_['is_shorthand'] = \
                property_['is_property'] and bool(property_['longhands'])
            property_['is_longhand'] = \
                property_['is_property'] and not property_['is_shorthand']
            self.expand_visited(property_)
            property_['in_origin_trial'] = False
            self.expand_origin_trials(property_, origin_trial_features)
            self.expand_surrogate(property_)

        self._aliases = [
            property_ for property_ in properties if property_['alias_for']
        ]
        self._shorthands = [
            property_ for property_ in properties if property_['longhands']
        ]
        self._longhands = [
            property_ for property_ in properties
            if (not property_['alias_for'] and not property_['longhands'])
        ]

        # Sort the properties by priority, then alphabetically. Ensure that
        # the resulting order is deterministic.
        # Sort properties by priority, then alphabetically.
        for property_ in self._longhands + self._shorthands:
            self.expand_parameters(property_)
            validate_property(property_)
            # This order must match the order in CSSPropertyPriority.h.
            priority_numbers = {'High': 0, 'Low': 1}
            priority = priority_numbers[property_['priority']]
            name_without_leading_dash = property_['name'].original
            if name_without_leading_dash.startswith('-'):
                name_without_leading_dash = name_without_leading_dash[1:]
            property_['sorting_key'] = (priority, name_without_leading_dash)

        sorting_keys = {}
        for property_ in self._longhands + self._shorthands:
            key = property_['sorting_key']
            assert key not in sorting_keys, \
                ('Collision detected - two properties have the same name and '
                 'priority, a potentially non-deterministic ordering can '
                 'occur: {}, {} and {}'.format(
                     key, property_['name'].original, sorting_keys[key]))
            sorting_keys[key] = property_['name'].original
        self._longhands.sort(key=lambda p: p['sorting_key'])
        self._shorthands.sort(key=lambda p: p['sorting_key'])

        # The sorted index becomes the CSSPropertyID enum value.
        for property_ in self._longhands + self._shorthands:
            property_['enum_value'] = self._last_used_enum_value
            self._last_used_enum_value += 1
            # Add the new property into the map of properties.
            assert property_['property_id'] not in self._properties_by_id, \
                ('property with ID {} appears more than once in the '
                 'properties list'.format(property_['property_id']))
            self._properties_by_id[property_['property_id']] = property_

        self.expand_aliases()
        self._properties_including_aliases = self._longhands + \
            self._shorthands + self._aliases

    def expand_origin_trials(self, property_, origin_trial_features):
        if not property_['runtime_flag']:
            return
        if property_['runtime_flag'] in origin_trial_features:
            property_['in_origin_trial'] = True

    def expand_visited(self, property_):
        if not property_['visited_property_for']:
            return
        visited_property_for = property_['visited_property_for']
        unvisited_property = self._properties_by_name[visited_property_for]
        property_['visited'] = True
        # The visited property needs a link to the unvisited counterpart.
        property_['unvisited_property'] = unvisited_property
        # The unvisited property needs a link to the visited counterpart.
        assert 'visited_property' not in unvisited_property, \
            'A property may not have multiple visited properties'
        unvisited_property['visited_property'] = property_

    def expand_surrogate(self, property_):
        if not property_['surrogate_for']:
            return
        assert property_['surrogate_for'] in self._properties_by_name, \
            'surrogate_for must name a property'
        # Upgrade 'surrogate_for' to property reference.
        property_['surrogate_for'] = self._properties_by_name[
            property_['surrogate_for']]

    def expand_aliases(self):
        for i, alias in enumerate(self._aliases):
            validate_alias(alias)
            aliased_property = self._properties_by_id[id_for_css_property(
                alias['alias_for'])]
            aliased_property.setdefault('aliases', [])
            aliased_property['aliases'].append(alias['name'].original)
            updated_alias = aliased_property.copy()
            updated_alias['name'] = alias['name']
            updated_alias['alias_for'] = alias['alias_for']
            updated_alias['aliased_property'] = aliased_property[
                'name'].to_upper_camel_case()
            updated_alias['property_id'] = id_for_css_property_alias(
                alias['name'])
            updated_alias['enum_key'] = enum_key_for_css_property_alias(
                alias['name'])
            updated_alias['enum_value'] = aliased_property['enum_value'] + \
                self._alias_offset
            updated_alias['superclass'] = 'CSSUnresolvedProperty'
            updated_alias['namespace_group'] = \
                'Shorthand' if aliased_property['longhands'] else 'Longhand'
            self._aliases[i] = updated_alias

    def expand_parameters(self, property_):
        def set_if_none(property_, key, value):
            if key not in property_ or property_[key] is None:
                property_[key] = value

        # Basic info.
        name = property_['name']
        property_['property_id'] = id_for_css_property(name)
        property_['enum_key'] = enum_key_for_css_property(name)
        property_['is_internal'] = name.original.startswith('-internal-')
        method_name = property_['name_for_methods']
        if not method_name:
            method_name = name.to_upper_camel_case().replace('Webkit', '')
        set_if_none(property_, 'inherited', False)
        set_if_none(property_, 'affected_by_forced_colors', False)

        # Initial function, Getters and Setters for ComputedStyle.
        set_if_none(property_, 'initial', 'Initial' + method_name)
        simple_type_name = str(property_['type_name']).split('::')[-1]
        set_if_none(property_, 'name_for_methods', method_name)
        set_if_none(property_, 'type_name', 'E' + method_name)
        set_if_none(
            property_, 'getter', method_name
            if simple_type_name != method_name else 'Get' + method_name)
        set_if_none(property_, 'setter', 'Set' + method_name)
        if property_['inherited']:
            property_['is_inherited_setter'] = (
                'Set' + method_name + 'IsInherited')
        property_['is_animation_property'] = (
            property_['priority'] == 'Animation')

        # Figure out whether this property should have style builders at all.
        # E.g. shorthands do not get style builders.
        property_['style_builder_declare'] = (property_['is_property']
                                              and not property_['longhands'])

        # Figure out whether we should generate style builder implementations.
        for x in ['initial', 'inherit', 'value']:
            suppressed = x in property_['style_builder_custom_functions']
            declared = property_['style_builder_declare']
            property_['style_builder_generate_%s' % x] = (declared
                                                          and not suppressed)

        # Expand StyleBuilderConverter params where necessary.
        if property_['type_name'] in PRIMITIVE_TYPES:
            set_if_none(property_, 'converter', 'CSSPrimitiveValue')
        else:
            set_if_none(property_, 'converter', 'CSSIdentifierValue')

        assert not property_['alias_for'], \
            'Use expand_aliases to expand aliases'
        if not property_['longhands']:
            property_['superclass'] = 'Longhand'
            property_['namespace_group'] = 'Longhand'
        elif property_['longhands']:
            property_['superclass'] = 'Shorthand'
            property_['namespace_group'] = 'Shorthand'

        # Expand out field templates.
        if property_['field_template']:
            self._field_alias_expander.expand_field_alias(property_)

            type_name = property_['type_name']
            if (property_['field_template'] == 'keyword'
                    or property_['field_template'] == 'multi_keyword'):
                default_value = (type_name + '::' + NameStyleConverter(
                    property_['default_value']).to_enum_value())
            elif (property_['field_template'] == 'external'
                  or property_['field_template'] == 'primitive'
                  or property_['field_template'] == 'pointer'):
                default_value = property_['default_value']
            else:
                assert property_['field_template'] == 'monotonic_flag', \
                    "Please put a valid value for field_template; got " + \
                    str(property_['field_template'])
                property_['type_name'] = 'bool'
                default_value = 'false'
            property_['default_value'] = default_value

            property_['unwrapped_type_name'] = property_['type_name']
            if property_['wrapper_pointer_name']:
                assert property_['field_template'] in ['pointer', 'external']
                if property_['field_template'] == 'external':
                    property_['type_name'] = '{}<{}>'.format(
                        property_['wrapper_pointer_name'], type_name)

        # Default values for extra parameters in computed_style_extra_fields.json5.
        set_if_none(property_, 'custom_copy', False)
        set_if_none(property_, 'custom_compare', False)
        set_if_none(property_, 'mutable', False)

        if property_['direction_aware_options']:
            if not property_['style_builder_template']:
                property_['style_builder_template'] = 'direction_aware'
            options = property_['direction_aware_options']
            assert 'resolver' in options, 'resolver option is required'
            assert 'physical_group' in options, 'physical_group option is required'
            options['resolver_name'] = NameStyleConverter(options['resolver'])
            options['physical_group_name'] = NameStyleConverter(
                options['physical_group'])

    @property
    def default_parameters(self):
        return self._default_parameters

    @property
    def aliases(self):
        return self._aliases

    @property
    def shorthands(self):
        return self._shorthands

    @property
    def shorthands_including_aliases(self):
        return self._shorthands + [x for x in self._aliases if x['longhands']]

    @property
    def longhands(self):
        return self._longhands

    @property
    def longhands_including_aliases(self):
        return self._longhands + [
            x for x in self._aliases if not x['longhands']
        ]

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
    def first_property_id(self):
        return self._first_enum_value

    @property
    def last_property_id(self):
        return self._first_enum_value + len(self._properties_by_id) - 1

    @property
    def last_unresolved_property_id(self):
        return self._last_unresolved_property_id

    @property
    def property_id_bit_length(self):
        return int.bit_length(self._last_unresolved_property_id)

    @property
    def alias_offset(self):
        return self._alias_offset

    @property
    def extra_fields(self):
        return self._extra_fields
