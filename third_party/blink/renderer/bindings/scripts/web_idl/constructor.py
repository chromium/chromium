# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .code_generator_info import CodeGeneratorInfo
from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithOwner
from .exposure import Exposure
from .function_like import FunctionLike
from .make_copy import make_copy
from .overload_group import OverloadGroup


class Constructor(FunctionLike, WithExtendedAttributes, WithCodeGeneratorInfo,
                  WithExposure, WithOwner, WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#idl-constructors"""

    class IR(FunctionLike.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     arguments,
                     return_type,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            FunctionLike.IR.__init__(
                self,
                identifier=Identifier('constructor'),
                arguments=arguments,
                return_type=return_type)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component=component)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir, owner):
        assert isinstance(ir, Constructor.IR)

        FunctionLike.__init__(self, ir)
        WithExtendedAttributes.__init__(self, ir.extended_attributes)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, components=ir.components)
        WithDebugInfo.__init__(self, ir.debug_info)


class ConstructorGroup(OverloadGroup, WithCodeGeneratorInfo, WithExposure,
                       WithOwner, WithDebugInfo):
    """
    Represents a group of constructors for an interface.

    The number of constructors in this group may be 1 or 2+.  In the latter
    case, the constructors are overloaded.
    """

    class IR(OverloadGroup.IR, WithCodeGeneratorInfo, WithExposure,
             WithDebugInfo):
        def __init__(self,
                     constructors,
                     code_generator_info=None,
                     debug_info=None):
            OverloadGroup.IR.__init__(self, constructors)
            WithCodeGeneratorInfo.__init__(self, code_generator_info)
            WithExposure.__init__(self)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir, constructors, owner):
        assert isinstance(ir, ConstructorGroup.IR)
        assert isinstance(constructors, (list, tuple))
        assert all(
            isinstance(constructor, Constructor)
            for constructor in constructors)
        assert all(constructor.identifier == ir.identifier
                   for constructor in constructors)

        ir = make_copy(ir)
        OverloadGroup.__init__(self, functions=constructors)
        WithCodeGeneratorInfo.__init__(
            self, CodeGeneratorInfo(ir.code_generator_info))
        WithExposure.__init__(self, Exposure(ir.exposure))
        WithOwner.__init__(self, owner)
        WithDebugInfo.__init__(self, ir.debug_info)


class NamedConstructor(object):
    @property
    def return_type(self):
        """
        Returns IDL type to construct.
        @return IdlInterfaceType
        """
        raise exceptions.NotImplementedError()

    @property
    def name(self):
        """
        Returns the name to be visible as.
        @return Identifier
        """
        raise exceptions.NotImplementedError()

    @property
    def arguments(self):
        """
        Returns a list of arguments.
        @return tuple(Argument)
        """
        raise exceptions.NotImplementedError()
