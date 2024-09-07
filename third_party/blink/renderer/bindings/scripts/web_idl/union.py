# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools

from .code_generator_info import CodeGeneratorInfoMutable
from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithIdentifier


class Union(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
            WithDebugInfo):
    """
    Union class makes a group of union types with the same flattened member
    types and the same result whether it includes a nullable type or not.

    For example, the following union types will be grouped into one Union
    instance.
      (A? or B or C), (A or B? or C), ((A or B) or C?), (A or (B or C?)), ...
    All these unions have the same set of flattened member types (A, B, C) and
    include a nullable type.

    However, all following union types will be grouped into separate Union
    instances.
      (A or B), ([X] A or B), ([Y] A or B)
    IdlType(A), IdlType([X] A), and IdlType([Y] A) are all distinguished from
    each other as they behave differently.  Bindings code generators are
    expected to define an implementation class for each Union instance.
    """

    _null_token = "Null"

    @classmethod
    def unique_token(cls, union_type):
        """
        Returns an unique token per a set of union types that are considered
        as the same group.
        """
        assert union_type.is_union

        token_pieces = []

        def collect_token_pieces(idl_type):
            idl_type = idl_type.unwrap()
            if idl_type.is_union:
                for member_type in idl_type.member_types:
                    collect_token_pieces(member_type)
            else:
                # Typename is not guaranteed to be unique, however it's
                # unlikely that a conflict happens.
                token_pieces.append(
                    idl_type.type_name_with_extended_attribute_key_values)

        collect_token_pieces(union_type)
        token_pieces.sort()
        if union_type.does_include_nullable_type:
            token_pieces.append(cls._null_token)
        return tuple(token_pieces)

    class IR(object):
        # Note that Union.IR is, despite of its name, very different from other
        # IDL definitions' IR classes.  This class is not meant to be stored in
        # IRMap nor managed with 'compilation phase'.

        def __init__(self, token, union_types):
            assert all(union_type.is_union for union_type in union_types)

            self.token = token
            self._member_set = set(token)
            self.union_types = union_types
            self.typedefs = []
            self.sub_union_irs = []
            self.public_object = None
            self.usage = functools.reduce(lambda usage, u: usage | u.usage,
                                          union_types, 0)

        def __lt__(self, other):
            if len(self.token) == len(other.token):
                return self.token < other.token
            else:
                return len(self.token) < len(other.token)

        def contains(self, other):
            assert isinstance(other, Union.IR)
            return (self.token != other.token
                    and self._member_set.issuperset(other._member_set))

    def __init__(self, ir):
        assert isinstance(ir, Union.IR)
        assert ir.public_object is None

        identifier = Identifier('Union_{}'.format('_'.join(ir.token)))
        union_type = ir.union_types[0]
        flattened_member_types = union_type.flattened_member_types
        does_include_nullable_type = union_type.does_include_nullable_type
        does_include_nullable_or_dict = (
            union_type.does_include_nullable_or_dict)

        typedef_members = set()
        union_members = set()
        for union_type in ir.union_types:
            assert union_type.flattened_member_types == flattened_member_types
            assert (union_type.does_include_nullable_type ==
                    does_include_nullable_type)
            for member_type in union_type.member_types:
                if member_type.is_typedef:
                    typedef_members.add(member_type.typedef_object)
        for sub_union_ir in ir.sub_union_irs:
            assert isinstance(sub_union_ir.public_object, Union)
            typedef_members.update(sub_union_ir.typedefs)
            union_members.add(sub_union_ir.public_object)

        components = set()
        for_testing = [False]

        def collect_primary_component(idl_type):
            type_definition_object = idl_type.type_definition_object
            if type_definition_object and type_definition_object.components:
                components.add(type_definition_object.components[0])
            if (type_definition_object and
                    type_definition_object.code_generator_info.for_testing):
                for_testing[0] = True

        for idl_type in flattened_member_types:
            idl_type.apply_to_all_composing_elements(collect_primary_component)
        code_generator_info = CodeGeneratorInfoMutable()
        code_generator_info.set_for_testing(for_testing[0])

        WithIdentifier.__init__(self, identifier)
        WithCodeGeneratorInfo.__init__(self,
                                       code_generator_info,
                                       readonly=True)
        WithComponent.__init__(self, sorted(components), readonly=True)
        WithDebugInfo.__init__(self)

        sort_key_typename = lambda idl_type: (
            idl_type.type_name_with_extended_attribute_key_values)
        sort_key_identifier = lambda x: x.identifier

        self._idl_types = tuple(ir.union_types)
        self._member_tokens = ir.token
        self._flattened_member_types = tuple(
            sorted(flattened_member_types, key=sort_key_typename))
        self._does_include_nullable_type = does_include_nullable_type
        self._does_include_nullable_or_dict = does_include_nullable_or_dict
        self._typedef_members = tuple(
            sorted(typedef_members, key=sort_key_identifier))
        self._union_members = tuple(
            sorted(union_members, key=sort_key_identifier))
        self._aliasing_typedefs = tuple(
            sorted(ir.typedefs, key=sort_key_identifier))
        self._usage = ir.usage

        ir.public_object = self

    @property
    def idl_types(self):
        """Returns a list of IdlTypes which this object represents."""
        return self._idl_types

    @property
    def member_tokens(self):
        """Returns a list of unique names of union member types."""
        return self._member_tokens

    @property
    def flattened_member_types(self):
        """
        Returns the same list of flattened member types as
        IdlType.flattened_member_types.
        """
        return self._flattened_member_types

    @property
    def does_include_nullable_type(self):
        """
        Returns True if any of member type is nullable or a member union
        includes a nullable type.
        """
        return self._does_include_nullable_type

    @property
    def does_include_nullable_or_dict(self):
        """
        Returns True if this type includes a nullable type or a dictionary
        type.
        """
        return self._does_include_nullable_or_dict

    @property
    def typedef_members(self):
        """
        Returns a list of typedef types which are direct members of union types
        which this object represents.

        Given the following union types,
          (AT or B), (A or BT) where typedef A AT, and typedef B BT
        typedef_members returns a list of IdlType(AT) and IdlType(BT).
        """
        return self._typedef_members

    @property
    def union_members(self):
        """
        Returns a list of union types which are direct members of union types
        which this object represents.

        Given the following union types,
          ((A or B) or C), (A or (B or C))
        union_members returns a list of IdlType(A or B) and IdlType(B or C).
        """
        return self._union_members

    @property
    def aliasing_typedefs(self):
        """
        Returns a list of typedef types which are aliases to union types which
        this object represents.

        Given the following typedef definitions,
          typedef ((A or B) or C) T1;
          typedef (A or (B or C)) T2;
        aliasing_typedefs returns a list of IdlType(T1) and IdlType(T2).
        """
        return self._aliasing_typedefs

    @property
    def usage(self):
        return self._usage
