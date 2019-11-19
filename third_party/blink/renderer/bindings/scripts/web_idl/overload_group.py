# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithIdentifier
from .function_like import FunctionLike


class OverloadGroup(WithIdentifier):
    class IR(WithIdentifier):
        def __init__(self, functions):
            assert isinstance(functions, (list, tuple))
            assert all(
                isinstance(function, FunctionLike.IR)
                for function in functions)
            assert len(set(
                [function.identifier for function in functions])) == 1
            assert len(set(
                [function.is_static for function in functions])) == 1

            WithIdentifier.__init__(self, functions[0].identifier)
            self.functions = list(functions)
            self.is_static = functions[0].is_static

        def __iter__(self):
            return iter(self.functions)

        def __len__(self):
            return len(self.functions)

    def __init__(self, functions):
        assert isinstance(functions, (list, tuple))
        assert all(
            isinstance(function, FunctionLike) for function in functions)
        assert len(set([function.identifier for function in functions])) == 1
        assert len(set([function.is_static for function in functions])) == 1

        WithIdentifier.__init__(self, functions[0].identifier)
        self._functions = tuple(functions)
        self._is_static = functions[0].is_static

    def __iter__(self):
        return iter(self._functions)

    def __len__(self):
        return len(self._functions)

    @property
    def is_static(self):
        return self._is_static

    @property
    def min_num_of_required_arguments(self):
        """
        Returns the minimum number of required arguments of overloaded
        functions.
        """
        return min(map(lambda func: func.num_of_required_arguments, self))
