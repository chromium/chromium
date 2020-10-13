# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing utilities for handling args in libraries."""

def _sentinel(tag):
    return struct(**{tag: tag})

# A sentinel value indicating a value that will be computed based off of some
# other values
COMPUTE = _sentinel("__compute__")

# A sentinel value to be used as a default so that it can be distinguished
# between an unspecified value and an explicit value of None
DEFAULT = _sentinel("__default__")

def defaults(extends = None, **vars):
    """Define a structure that provides module-level defaults for function
    arguments.

    Arguments:
      * extends - A struct containing `lucicfg.var` attributes to add to the
        resulting struct.
      * vars - Defaults to define. Each entry results in a `lucicfg.var` attribute
        being added to the resulting struct. The name of the attribute is the
        keyword and the default value of the `lucicfg.var` is the keyword's value.

    Returns:
      A struct containing `lucicfg.var` attributes providing the module level
      defaults and the following methods:
      * get_value(name, value) - Gets the value of an argument. If `value` is not
        `DEFAULT`, `value` is returned. Otherwise, the module-level default for
        `name` is returned.
      * get_value_from_kwargs(name, kwargs) - Gets the value of a keyword
        argument. If `name` is in `kwargs`, `kwargs[name]` is returned. Otherwise,
        the module-level default for `name` is returned.
      * set(**kwargs) - Sets module-level defaults. For each keyword, sets the
        module-level default with the keyword as the name to the value of the
        keyword.
    """
    methods = ["get_value", "get_value_from_kwargs", "set"]
    for m in methods:
        if m in vars:
            fail("{!r} can't be used as the name of a default: it is a method"
                .format(a))

    vars = {k: lucicfg.var(default = v) for k, v in vars.items()}
    for a in dir(extends):
        if a not in methods:
            vars[a] = getattr(extends, a)

    def get_value(name, value):
        if value != DEFAULT:
            return value
        return vars[name].get()

    def get_value_from_kwargs(name, kwargs):
        return get_value(name, kwargs.get(name, DEFAULT))

    def set(**kwargs):
        for k, v in kwargs.items():
            vars[k].set(v)

    return struct(
        get_value = get_value,
        get_value_from_kwargs = get_value_from_kwargs,
        set = set,
        **vars
    )

args = struct(
    COMPUTE = COMPUTE,
    DEFAULT = DEFAULT,
    defaults = defaults,
)
