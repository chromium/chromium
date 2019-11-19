# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithIdentifier
from .idl_type import IdlType
from .typedef import Typedef


class Union(WithIdentifier, WithComponent, WithDebugInfo):
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

    def __init__(self, identifier, union_types, typedef_backrefs):
        """
        Args:
            union_types: Union types of which this object consists.  All types
                in |union_types| must have the same flattened_member_types and
                the same value of does_include_nullable_type.
            typedef_backrefs: Typedef instances whose typedef'ed type is this
                union.
        """
        assert isinstance(union_types, (list, tuple))
        assert len(union_types) > 0
        assert all(
            isinstance(union_type, IdlType) and union_type.is_union
            for union_type in union_types)
        assert isinstance(typedef_backrefs, (list, tuple))
        assert all(
            isinstance(typedef, Typedef) for typedef in typedef_backrefs)

        flattened_members = union_types[0].flattened_member_types
        does_include_nullable_type = union_types[0].does_include_nullable_type

        nullable_members = set()
        typedef_members = set()
        union_members = set()
        for union_type in union_types:
            assert union_type.flattened_member_types == flattened_members
            assert (union_type.does_include_nullable_type ==
                    does_include_nullable_type)
            for direct_member in union_type.member_types:
                if direct_member.is_nullable:
                    nullable_members.add(direct_member)
                if direct_member.is_typedef:
                    typedef_members.add(direct_member)
                if direct_member.is_union:
                    union_members.add(direct_member)

        sort_key = lambda x: x.syntactic_form

        components = set()

        def collect_components(idl_type):
            user_defined_type = idl_type.type_definition_object
            if user_defined_type:
                components.update(user_defined_type.components)

        for idl_type in flattened_members:
            idl_type.apply_to_all_composing_elements(collect_components)

        WithIdentifier.__init__(self, identifier)
        WithComponent.__init__(self, components=sorted(components))
        WithDebugInfo.__init__(self)

        # Sort improves reproducibility.
        self._flattened_members = tuple(
            sorted(flattened_members, key=sort_key))
        self._does_include_nullable_type = does_include_nullable_type
        self._nullable_members = tuple(sorted(nullable_members, key=sort_key))
        self._typedef_members = tuple(sorted(typedef_members, key=sort_key))
        self._union_members = tuple(sorted(union_members, key=sort_key))
        self._typedef_backrefs = tuple(typedef_backrefs)

    @property
    def flattened_member_types(self):
        """
        Returns the same list of flattened member types as
        IdlType.flattened_member_types.
        """
        return self._flattened_members

    @property
    def does_include_nullable_type(self):
        """
        Returns True if any of member type is nullable or a member union
        includes a nullable type.
        """
        return self._does_include_nullable_type

    @property
    def nullable_member_types(self):
        """
        Returns a list of nullable types which are direct members of union types
        of which this object consists.

        Given the following unions,
          (A? or B or C), (A or B? or C), (A or B or CN) where typedef C? CN;
        nullable_member_types returns a list of IdlType(A?) and IdlType(B?).
        """
        return self._nullable_members

    @property
    def typedef_member_types(self):
        """
        Returns a list of typedef types which are direct members of union types
        of which this object consists.

        Given the following unions,
          (AT or B), (A or BT) where typedef A AT, and typedef B BT;
        typedef_member_types returns a list of IdlType(AT) and IdlType(BT).
        """
        return self._typedef_members

    @property
    def union_member_types(self):
        """
        Returns a list of union types which are direct members of union types of
        which this object consists.

        Given the following unions,
          ((A or B) or C), (A or (B or C))
        union_member_types returns a list of IdlType(A or B) and
        IdlType(B or C).
        """
        return self._union_members

    @property
    def aliasing_typedefs(self):
        """
        Returns a list of typedefs which are aliases to this union type.
        """
        return self._typedef_backrefs
