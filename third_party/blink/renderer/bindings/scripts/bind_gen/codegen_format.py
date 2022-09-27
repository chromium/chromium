# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import string


def format_template(format_string, *args, **kwargs):
    """
    Formats a string like the built-in |format| allowing unbound keys.

        format_template("${template_var} {format_var}", format_var=42)
    will produce
        "${template_var} 42"
    without raising an exception that |template_var| is unbound.
    """
    return _TemplateFormatter().format(format_string, *args, **kwargs)


class _TemplateFormatter(string.Formatter):
    def __init__(self):
        string.Formatter.__init__(self)
        self._template_formatter_indexing_count_ = 0

    def get_value(self, key, args, kwargs):
        if isinstance(key, int):
            return args[key]
        assert isinstance(key, str)
        if not key:
            # TODO(crbug.com/1174969):
            # Prior to Python 3.1, when a positional argument specifier is
            # omitted, |format_string="{}"| produces |key=""|.  Should be
            # removed once Python2 gets retired.
            index = self._template_formatter_indexing_count_
            self._template_formatter_indexing_count_ += 1
            return args[index]
        if key in kwargs:
            return kwargs[key]
        else:
            return "{" + key + "}"


class NonRenderable(object):
    """
    Represents a non-renderable object.

    Unlike a template variable bound to None, which is a valid Python value,
    this object raises an exception when rendered, just like an unbound template
    variable.
    """

    def __init__(self, error_message="Undefined"):
        self._error_message = error_message

    def __bool__(self):
        raise NameError(self._error_message)

    def __str__(self):
        raise NameError(self._error_message)
