# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools

from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithOwner
from .composition_parts import WithOwnerMixin
from .function_like import FunctionLike
from .function_like import OverloadGroup
from .make_copy import make_copy


class Constructor(FunctionLike, WithExtendedAttributes, WithCodeGeneratorInfo,
                  WithExposure, WithOwner, WithOwnerMixin, WithComponent,
                  WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-constructors"""

    class IR(FunctionLike.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithOwnerMixin, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     arguments,
                     return_type,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert identifier is None or isinstance(identifier, Identifier)
            FunctionLike.IR.__init__(
                self,
                identifier=(identifier or Identifier('constructor')),
                arguments=arguments,
                return_type=return_type)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithOwnerMixin.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

    def __init__(self, ir, owner):
        assert isinstance(ir, Constructor.IR)

        FunctionLike.__init__(self, ir)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithOwner.__init__(self, owner)
        WithOwnerMixin.__init__(self, ir)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)


class ConstructorGroup(OverloadGroup, WithExtendedAttributes,
                       WithCodeGeneratorInfo, WithExposure, WithOwner,
                       WithComponent, WithDebugInfo):
    """
    Represents a group of constructors of an interface.

    The number of constructors in this group may be 1 or 2+.  In the latter
    case, the constructors are overloaded.
    """

    class IR(OverloadGroup.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithDebugInfo):
        def __init__(self,
                     constructors,
                     extended_attributes=None,
                     code_generator_info=None,
                     debug_info=None):
            OverloadGroup.IR.__init__(self, constructors)
            WithExtendedAttributes.__init__(self, extended_attributes)
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

        components = functools.reduce(
            lambda s, constructor: s.union(constructor.components),
            constructors, set())

        ir = make_copy(ir)
        OverloadGroup.__init__(self, functions=constructors)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithOwner.__init__(self, owner)
        WithComponent.__init__(self, sorted(components))
        WithDebugInfo.__init__(self, ir)
