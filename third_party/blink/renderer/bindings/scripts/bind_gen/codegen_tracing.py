# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import inspect


class CodeGenTracing(object):
    """
    Represents code generation tracing information to track down which line of
    Python code in the code generator has generated the resulting code. Meant
    to be used for purely debugging purposes.
    """

    class _CallFrame(object):

        def __init__(self, qualname, lineno):
            self._qualname = qualname
            self._lineno = lineno

        def __str__(self):
            return "  /* {func}:{line} */".format(func=self._qualname,
                                                  line=self._lineno)

    _is_code_generation_tracing_enabled = False
    _modules_to_be_ignored = []

    @classmethod
    def enable_code_generation_tracing(cls):
        """
        Enables the code generation tracing feature. The methods of this class
        must not be called unless the feature is enabled.
        """
        cls._is_code_generation_tracing_enabled = True

    @classmethod
    def is_enabled(cls):
        """Returns True if the code generation tracing feature is enabled."""
        return cls._is_code_generation_tracing_enabled

    @classmethod
    def add_modules_to_be_ignored(cls, modules):
        """
        Adds modules to be ignored when capturing a caller which creates a code
        node.
        """
        cls._modules_to_be_ignored.extend(map(inspect.getmodule, modules))

    @classmethod
    def capture_caller(cls):
        """Captures the caller function information."""
        assert cls.is_enabled()

        frame = inspect.currentframe()
        while frame:
            if inspect.getmodule(frame) in cls._modules_to_be_ignored:
                frame = frame.f_back
                continue

            return cls._CallFrame(qualname=frame.f_code.co_qualname,
                                  lineno=frame.f_lineno)
        return cls._CallFrame(qualname="<unknown>", lineno=0)


# This module should be ignored.
CodeGenTracing.add_modules_to_be_ignored([CodeGenTracing])
