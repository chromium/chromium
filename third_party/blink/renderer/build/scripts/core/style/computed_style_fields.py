# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from itertools import chain

from blinkbuild.name_style_converter import NameStyleConverter


def _flatten_list(x):
    """Flattens a list of lists into a single list."""
    return list(chain.from_iterable(x))


def _num_32_bit_words_for_bit_fields(bit_fields):
    """
    Gets the number of 32 bit unsigned ints needed store a list of bit fields.
    """
    num_buckets, cur_bucket = 0, 0
    for field in bit_fields:
        if field.size + cur_bucket > 32:
            num_buckets += 1
            cur_bucket = 0
        cur_bucket += field.size
    return num_buckets + (cur_bucket > 0)


class Group(object):
    """Represents a group of fields stored together in a class.

    Attributes:
        name: The name of the group as a string, or None.
        subgroups: List of Group instances that are stored as subgroups under
            this group.
        fields: List of Field instances stored directly under this group.
        parent: The parent group, or None if this is the root group.
    """

    def __init__(self, name, subgroups, fields):
        self.name = name
        self.subgroups = subgroups
        self.fields = fields
        self.parent = None

        converter = NameStyleConverter(name or '')
        self.type_name = converter.to_class_name(prefix='style', suffix='data')
        self.member_name = converter.to_class_data_member(suffix='data')
        self.num_32_bit_words_for_bit_fields = _num_32_bit_words_for_bit_fields(
            field for field in fields if field.is_bit_field)

        # Recursively get all the fields in the subgroups as well
        self.all_fields = _flatten_list(subgroup.all_fields
                                        for subgroup in subgroups) + fields
        self.all_subgroups = _flatten_list(
            subgroup.all_subgroups for subgroup in subgroups) + subgroups

        self.needs_diff = any(field.needs_diff for field in self.all_fields)

        # Ensure that all fields/subgroups on this group link to it
        for field in fields:
            field.group = self

        for subgroup in subgroups:
            subgroup.parent = self

    def path_without_root(self):
        """Return list of ancestor groups, excluding the root group.

        The first item is the current group, second item is the parent, third
        is the grandparent and so on.
        """
        group_path = []
        current_group = self
        while current_group.name:
            group_path.insert(0, current_group)
            current_group = current_group.parent
        return group_path


class Enum(object):
    """Represents a generated enum in ComputedStyleBaseConstants."""

    def __init__(self, type_name, keywords, set_type):
        self.type_name = type_name
        self.keywords = keywords
        self.values = [
            NameStyleConverter(keyword).to_enum_value() for keyword in keywords
        ]
        self.set_type = set_type


class Field(object):
    """
    The generated ComputedStyle object is made up of a series of Fields.
    Each Field has a name, size, type, etc, and a bunch of attributes to
    determine which methods it will be used in.

    A Field also has enough information to use any storage type in C++, such as
    regular member variables, or more complex storage like vectors or hashmaps.
    Almost all properties will have at least one Field, often more than one.

    Most attributes in this class correspond to parameters in
    css_properties.json5. See that file for a more detailed explanation of
    each attribute.

    Attributes:
        field_role: The semantic role of the field. Can be:
            - 'property': for fields that store CSS properties
            - 'inherited_flag': for single-bit flags that store whether a
                                property is inherited by this style or
                                set explicitly
        name_for_methods: String used to form the names of getters and setters.
            Should be in upper camel case.
        property_name: Name of the property that the field is part of.
        type_name: Name of the C++ type exposed by the generated interface
            (e.g. EClear, int).
        wrapper_pointer_name: Name of the pointer type that wraps this field
            (e.g. RefPtr).
        field_template: Determines the interface generated for the field. Can
            be one of: keyword, flag, or monotonic_flag.
        size: Number of bits needed for storage.
        default_value: Default value for this field when it is first initialized
    """

    def __init__(self, field_role, name_for_methods, property_name, type_name,
                 wrapper_pointer_name, field_template, size, default_value,
                 derived_from, invalidate, reset_on_new_style, custom_compare,
                 mutable, getter_method_name, setter_method_name,
                 initial_method_name, computed_style_custom_functions,
                 computed_style_protected_functions, **kwargs):
        name_source = NameStyleConverter(name_for_methods)
        self.name = name_source.to_class_data_member()
        self.property_name = property_name
        self.type_name = type_name
        self.wrapper_pointer_name = wrapper_pointer_name
        self.alignment_type = self.wrapper_pointer_name or self.type_name
        self.requires_tracing = wrapper_pointer_name == 'Member'
        self.field_template = field_template
        self.size = size
        self.default_value = default_value
        self.derived_from = derived_from
        self.invalidate = [
            NameStyleConverter(value).to_enum_value() for value in invalidate
        ]
        self.needs_diff = bool(invalidate)
        self.reset_on_new_style = reset_on_new_style
        self.custom_compare = custom_compare
        self.mutable = mutable
        self.group = None

        # Method names
        self.getter_method_name = getter_method_name
        self.setter_method_name = setter_method_name
        self.internal_getter_method_name = name_source.to_function_name(
            suffix='internal')
        self.internal_mutable_method_name = name_source.to_function_name(
            prefix='mutable', suffix='internal')
        self.internal_setter_method_name = NameStyleConverter(
            setter_method_name).to_function_name(suffix='internal')
        self.initial_method_name = initial_method_name
        self.resetter_method_name = name_source.to_function_name(
            prefix='reset')
        self.internal_resetter_method_name = NameStyleConverter(
            self.resetter_method_name).to_function_name(suffix='internal')
        self.computed_style_custom_functions = computed_style_custom_functions
        self.computed_style_protected_functions = computed_style_protected_functions
        self.getter_visibility = self.get_visibility('getter')
        self.setter_visibility = self.get_visibility('setter')
        self.resetter_visibility = self.get_visibility('resetter')

        # Only bitfields have sizes.
        self.is_bit_field = self.size is not None

        # Field role: one of these must be true
        self.is_property = field_role == 'property'
        self.is_inherited_flag = field_role == 'inherited_flag'
        assert (self.is_property, self.is_inherited_flag).count(True) == 1, \
            'Field role has to be exactly one of: property, inherited_flag'

        if not self.is_inherited_flag:
            self.is_inherited = kwargs.pop('inherited')
            self.is_independent = kwargs.pop('independent')
            self.is_semi_independent_variable = kwargs.pop(
                'semi_independent_variable')
            assert self.is_inherited or not self.is_independent, \
                'Only inherited fields can be independent'

            suffix = ['is', 'inherited']
            if 'getter' in self.computed_style_custom_functions:
                suffix.append('internal')
            self.is_inherited_method_name = name_source.to_function_name(
                suffix=suffix)
        assert len(kwargs) == 0, \
            'Unexpected arguments provided to Field: ' + str(kwargs)

    def get_visibility(self, function):
        if function in self.computed_style_protected_functions:
            return 'protected'
        if function in self.computed_style_custom_functions:
            return 'protected'
        return 'public'
