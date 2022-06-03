# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
A collection of functions that format a variety of names (class name, function
name, variable name, etc.)

The functions are grouped into two.

xxx(*args):
    The name is made by concatenating the arguments.

xxx_f(format_string, *args, **kwargs):
    The name is formatted with the given format string and arguments.
"""

from blinkbuild import name_style_converter


def api_func(*args):
    """Applies the style of Blink implementation function names for Web API."""
    return _concat(raw.lower_camel_case, args)


def api_func_f(format_string, *args, **kwargs):
    """Applies the style of Blink implementation function names for Web API."""
    return raw.lower_camel_case(
        _format(raw.upper_camel_case, format_string, *args, **kwargs))


def arg(*args):
    """Applies the style of argument variable names."""
    return _concat(raw.snake_case, args)


def arg_f(format_string, *args, **kwargs):
    """Applies the style of argument variable names."""
    return _format(raw.snake_case, format_string, *args, **kwargs)


def class_(*args):
    """Applies the style of class names."""
    return _concat(raw.upper_camel_case, args)


def class_f(format_string, *args, **kwargs):
    """Applies the style of class names."""
    return _format(raw.upper_camel_case, format_string, *args, **kwargs)


def constant(*args):
    """Applies the style of constant names."""
    return "k" + _concat(raw.upper_camel_case, args)


def constant_f(format_string, *args, **kwargs):
    """Applies the style of constant names."""
    return "k" + raw.upper_camel_case(
        _format(raw.upper_camel_case, format_string, *args, **kwargs))


def file(*args):
    """Applies the style of filenames."""
    return _concat(raw.snake_case, args)


def file_f(format_string, *args, **kwargs):
    """Applies the style of filenames."""
    return _format(raw.snake_case, format_string, *args, **kwargs)


def func(*args):
    """Applies the style of general Blink function names."""
    return _concat(raw.upper_camel_case, args)


def func_f(format_string, *args, **kwargs):
    """Applies the style of general Blink function names."""
    return _format(raw.upper_camel_case, format_string, *args, **kwargs)


def header_guard(*args):
    """Applies the style of header guard names."""
    return _concat(raw.macro_case, args) + "_"


def header_guard_f(format_string, *args, **kwargs):
    """Applies the style of header guard names."""
    return _format(raw.macro_case, format_string, *args, **kwargs) + "_"


def local_var(*args):
    """Applies the style of function local variable names."""
    return _concat(raw.snake_case, args)


def local_var_f(format_string, *args, **kwargs):
    """Applies the style of function local variable names."""
    return _format(raw.snake_case, format_string, *args, **kwargs)


def macro(*args):
    """Applies the style of macro names."""
    return _concat(raw.macro_case, args)


def macro_f(format_string, *args, **kwargs):
    """Applies the style of macro names."""
    return _format(raw.macro_case, format_string, *args, **kwargs)


def member_var(*args):
    """Applies the style of member variable names."""
    return _concat(raw.snake_case, args) + "_"


def member_var_f(format_string, *args, **kwargs):
    """Applies the style of member variable names."""
    return _format(raw.snake_case, format_string, *args, **kwargs) + "_"


def namespace(*args):
    """Applies the style of namespace names."""
    return _concat(raw.snake_case, args)


def namespace_f(format_string, *args, **kwargs):
    """Applies the style of namespace names."""
    return _format(raw.snake_case, format_string, *args, **kwargs)


def _concat(style_func, args):
    assert callable(style_func)

    return style_func(" ".join(map(_tokenize, args)))


def _format(style_func, format_string, *args, **kwargs):
    assert callable(style_func)
    assert isinstance(format_string, str)

    args = map(style_func, map(_tokenize, args))
    for key, value in kwargs.items():
        kwargs[key] = style_func(_tokenize(value))
    return format_string.format(*args, **kwargs)


def _tokenize(s):
    s = str(s)
    if "_" in s and s.isupper():
        # NameStyleConverter doesn't treat "ABC_DEF" as two tokens of "abc" and
        # "def" while treating "abc_def" as "abc" and "def".  Help
        # NameStyleConverter by lowering the string.
        return s.lower()
    return s


class raw(object):
    """
    Namespace to provide (unrecommended) raw controls on case conversions.

    This class is pretending to be a module.
    """

    _NameStyleConverter = name_style_converter.NameStyleConverter

    def __init__(self):
        assert False

    @staticmethod
    def tokenize(name):
        return name_style_converter.tokenize_name(name)

    @staticmethod
    def snake_case(name):
        return raw._NameStyleConverter(name).to_snake_case()

    @staticmethod
    def upper_camel_case(name):
        return raw._NameStyleConverter(name).to_upper_camel_case()

    @staticmethod
    def lower_camel_case(name):
        return raw._NameStyleConverter(name).to_lower_camel_case()

    @staticmethod
    def macro_case(name):
        return raw._NameStyleConverter(name).to_macro_case()
