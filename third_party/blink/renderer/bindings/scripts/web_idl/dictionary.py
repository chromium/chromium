# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .code_generator_info import CodeGeneratorInfo
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .exposure import Exposure
from .idl_type import IdlType
from .ir_map import IRMap
from .literal_constant import LiteralConstant
from .make_copy import make_copy
from .reference import RefById
from .user_defined_type import UserDefinedType


class Dictionary(UserDefinedType, WithExtendedAttributes,
                 WithCodeGeneratorInfo, WithExposure, WithComponent,
                 WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-dictionaries"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     inherited=None,
                     own_members=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert isinstance(own_members, (list, tuple)) and all(
                isinstance(member, DictionaryMember.IR)
                for member in own_members)

            kind = (IRMap.IR.Kind.PARTIAL_DICTIONARY
                    if is_partial else IRMap.IR.Kind.DICTIONARY)
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.inherited = inherited
            self.own_members = list(own_members)

        def iter_all_members(self):
            return iter(self.own_members)

    def __init__(self, ir):
        assert isinstance(ir, Dictionary.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._inherited = ir.inherited
        self._own_members = tuple([
            DictionaryMember(member_ir, owner=self)
            for member_ir in ir.own_members
        ])

    @property
    def inherited(self):
        """Returns the inherited dictionary or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def own_members(self):
        """
        Returns own dictionary members.  Inherited members are not included.

        Note that members are not sorted alphabetically.  The order should be
        the declaration order.
        """
        return self._own_members

    @property
    def members(self):
        """
        Returns all dictionary members including inherited members, sorted in
        order from least to most derived dictionaries and lexicographical order
        within each dictionary.
        """

        def collect_inherited_members(dictionary):
            if dictionary is None:
                return []
            return (collect_inherited_members(dictionary.inherited) + sorted(
                dictionary.own_members, key=lambda member: member.identifier))

        return tuple(collect_inherited_members(self))

    # UserDefinedType overrides
    @property
    def is_dictionary(self):
        return True


class DictionaryMember(WithIdentifier, WithExtendedAttributes,
                       WithCodeGeneratorInfo, WithExposure, WithOwner,
                       WithComponent, WithDebugInfo):
    class IR(WithIdentifier, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     idl_type=None,
                     default_value=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(idl_type, IdlType)
            assert default_value is None or isinstance(default_value,
                                                       LiteralConstant)
            assert not default_value or idl_type.is_optional

            WithIdentifier.__init__(self, identifier)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

            self.idl_type = idl_type
            self.default_value = default_value

    def __init__(self, ir, owner):
        assert isinstance(ir, DictionaryMember.IR)
        assert isinstance(owner, Dictionary)

        ir = make_copy(ir)
        WithIdentifier.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._idl_type = ir.idl_type
        self._default_value = ir.default_value

    @property
    def idl_type(self):
        """Returns the type."""
        return self._idl_type

    @property
    def is_required(self):
        """Returns True if this is a required dictionary member."""
        return not self._idl_type.is_optional

    @property
    def default_value(self):
        """Returns the default value or None."""
        raise exceptions.NotImplementedError()
