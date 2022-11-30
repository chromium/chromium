# Copyright 2020 The Chromium Authors
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

# A sentinel value that can be passed as the merge argument to
# get_value/get_value_from_kwargs to have the provided value merged as a list.
MERGE_LIST = _sentinel("__merge_list__")

# A sentinel value that can be passed as the merge argument to
# get_value/get_value_from_kwargs to have the provided value merged as a dict.
MERGE_DICT = _sentinel("__merge_dict__")

_IGNORE_DEFAULT_ATTR = "_ignore_default"

def defaults(extends = None, **vars):
    """Define a structure provides a group of module-level defaults.

    Args:
      extends: A struct containing `lucicfg.var` attributes to add to the
        resulting struct.
      **vars: Defaults to define. Each entry results in a `lucicfg.var` attribute
        being added to the resulting struct. The name of the attribute is the
        keyword and the default value of the `lucicfg.var` is the keyword's value.

    Returns:
      A struct containing `lucicfg.var` attributes providing the module level
      defaults and the following methods:
      * get_value(name, value, merge=None) - Gets the value of an argument. The
        behavior of the function depends on the value of the `merge` argument:
        * None (default) - If `value` is not `DEFAULT`, `value` is returned.
          Otherwise, the module-level default for `name` is returned.
        * MERGE_LIST - If `value` is wrapped using `ignore_defaults`, the result
          of calling `listify(value.value)` is returned. Otherwise, the result
          of calling `listify` with the module-level default for `name` and
          `value` is returned.
        * MERGE_DICT - If `value` is wrapped using `ignore_defaults`,
          `value.value` is returned. Otherwise, the returned value will be the
          module-level default for `name` updated with `value`.
      * get_value_from_kwargs(name, kwargs, merge=None) - Gets the value of a keyword
        argument.
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

    def get_value(name, value, merge = None):
        default = vars[name].get()
        ignore_default, value = _should_ignore_default(value)

        if not merge:
            if ignore_default:
                fail("attribute {!r} does not merge with the default, ignore_defaults cannot be used".format(name))
            if value != DEFAULT:
                return value
            return default

        if merge == MERGE_DICT:
            if value == DEFAULT:
                value = {}
            if value and type(value) != type({}):
                fail("attribute {!r} requires a dict value or None, got {!r}".format(name, value))
            value = value or {}
            if ignore_default:
                return value
            new_value = default or {}
            new_value.update(value)
            return new_value

        if merge == MERGE_LIST:
            if value == DEFAULT:
                value = []
            if ignore_default:
                return listify(value)
            return listify(default, value)

        fail("unknown merge value: {}".format(merge))

    def get_value_from_kwargs(name, kwargs, merge = None):
        return get_value(name, kwargs.get(name, DEFAULT), merge = merge)

    def set(**kwargs):
        for k, v in kwargs.items():
            vars[k].set(v)

    return struct(
        get_value = get_value,
        get_value_from_kwargs = get_value_from_kwargs,
        set = set,
        **vars
    )

def ignore_default(value):
    """Wraps a value to ignore defaults for the argument.

    For arguments that merge the provided value with a module-level default,
    this provides a way to explicitly set an exact value. It is an error to use
    this for attributes that don't merge with the module-level default.
    """
    return struct(
        _ignore_default = True,
        value = value,
    )

def _should_ignore_default(value):
    ignore_default = getattr(value, _IGNORE_DEFAULT_ATTR, False)
    if ignore_default:
        value = value.value
    return ignore_default, value

def listify(*args):
    """Create a single list from multiple arguments.

    Each argument can be either a single element or a list of elements. A single
    element will appear as an element in the resulting list iff it is non-None.
    A list of elements will have all non-None elements appear in the resulting
    list.

    Args:
      *args: The arguments to merge into a list.

    Returns:
      A list composed of the pased in arguments.
    """
    wrap_in_ignore_default = False
    l = []
    for a in args:
        should_ignore_default, a = _should_ignore_default(a)
        if should_ignore_default:
            wrap_in_ignore_default = True
        if type(a) != type([]):
            a = [a]
        for e in a:
            should_ignore_default, val = _should_ignore_default(e)
            if should_ignore_default:
                wrap_in_ignore_default = True
            if val != None:
                l.append(val)
    if wrap_in_ignore_default:
        return ignore_default(l)
    return l

args = struct(
    COMPUTE = COMPUTE,
    DEFAULT = DEFAULT,
    MERGE_LIST = MERGE_LIST,
    MERGE_DICT = MERGE_DICT,
    defaults = defaults,
    ignore_default = ignore_default,
    listify = listify,
)
