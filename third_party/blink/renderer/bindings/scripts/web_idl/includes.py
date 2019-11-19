# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import Identifier
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .ir_map import IRMap


class Includes(WithComponent, WithDebugInfo):
    """https://heycam.github.io/webidl/#include"""

    class IR(IRMap.IR, WithComponent, WithDebugInfo):
        def __init__(self,
                     interface_identifier,
                     mixin_identifier,
                     component=None,
                     debug_info=None):
            assert isinstance(interface_identifier, Identifier)
            assert isinstance(mixin_identifier, Identifier)

            # Includes statements are treated similarly to partial definitions
            # because it's convenient for IdlCompiler that 'includes' are
            # grouped by interface's identifier, for example, a group of mixins
            # are merged into an interface.  So, the interface's identifier is
            # turned into this IR's identifier.
            IRMap.IR.__init__(
                self,
                identifier=interface_identifier,
                kind=IRMap.IR.Kind.INCLUDES)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.interafce_identifier = interface_identifier
            self.mixin_identifier = mixin_identifier

    # Includes are not exposed publicly so far.
    __init__ = None
