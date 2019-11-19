# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .composition_parts import WithIdentifier
from .idl_type import IdlType


class FunctionLike(WithIdentifier):
    class IR(WithIdentifier):
        def __init__(self, identifier, arguments, return_type,
                     is_static=False):
            assert isinstance(arguments, (list, tuple)) and all(
                isinstance(arg, Argument.IR) for arg in arguments)
            assert isinstance(return_type, IdlType)
            assert isinstance(is_static, bool)

            WithIdentifier.__init__(self, identifier)
            self.arguments = list(arguments)
            self.return_type = return_type
            self.is_static = is_static

    def __init__(self, ir):
        assert isinstance(ir, FunctionLike.IR)

        WithIdentifier.__init__(self, ir.identifier)
        self._arguments = tuple(
            [Argument(arg_ir, self) for arg_ir in ir.arguments])
        self._return_type = ir.return_type
        self._is_static = ir.is_static

    @property
    def arguments(self):
        """Returns a list of arguments."""
        return self._arguments

    @property
    def return_type(self):
        """Returns the return type."""
        return self._return_type

    @property
    def is_static(self):
        """Returns True if this is a static function."""
        return self._is_static

    @property
    def num_of_required_arguments(self):
        """Returns the number of required arguments."""
        return len(
            filter(lambda arg: not (arg.is_optional or arg.is_variadic),
                   self.arguments))
