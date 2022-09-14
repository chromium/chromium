# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for working with starlark structs.

The functionality for this module can be used by loading the structs symbol,
which provides the following functions:
* to_properties - convert a struct to a dict that can represent a proto in
    properties
* evolve - set new values for struct attributes
* extend - extend values for struct attributes
* remove - remove values for struct attributes
"""

load("./args.star", "args")

def _convert_to_dict(s):
    """Convert a struct to a dict

    Due to starlark not supporting recursion, in order to enable a struct
    containing nested structs to be recursively converted, the conversion won't
    necessarily be complete so a partially converted dict will be returned along
    with information of the attributes that need to themselves be converted.

    Args:
        s: The struct to convert.

    Returns:
        A 2-tuple:
            * A dict of converted attributes.
            * A sequence of 3-tuples for each attribute that needs to be recursively
            converted:
                * The dictionary to add the converted value to.
                * The name of the key to set with the converted value.
                * The value to convert.
    """
    d = {}
    to_convert = []
    for a in dir(s):
        v = getattr(s, a)
        if v == None or v == []:
            continue
        if type(v) == type(struct()):
            to_convert.append((d, a, v))
            continue
        d[a] = v
    return d, to_convert

def _to_proto_properties(s):
    """Converts a struct to a properties dict that can represent a proto.

    Args:
        s: The struct to convert.

    Returns:
        A dict with items corresponding to the attributes of `s`. Attributes
        with a None value or empty list will be omitted from the dict since the
        corresponding jsonpb values are equivalent to the field not being set.
    """
    top_level_dict, to_convert = _convert_to_dict(s)

    # Since starlark doesn't support recursion, iterate over the depth of the
    # structure, recording the work to be done at the next depth. Since starlark
    # doesn't support while loops, iterate up to an aribtrary maximum depth and
    # break out of the loop if there's no more work to be done.
    for _ in range(15):
        if not to_convert:
            break
        next_to_convert = []
        for d, k, s in to_convert:
            converted_s, to_convert_for_s = _convert_to_dict(s)
            next_to_convert.extend(to_convert_for_s)
            d[k] = converted_s
        to_convert = next_to_convert

    if to_convert:
        fail("excessively nested struct")

    return top_level_dict

def _evolve(s, **kwargs):
    """Modify a struct's attributes.

    Args:
        s: The struct to modify.
        **kwargs: The attributes to update the struct with.

    Returns:
        A new struct with the value of each attribute specified in `kwargs`
        is set to the corresponding value.

    Fails:
        If `kwargs` contains an item for an attribute not present on `s`.
    """
    d = {a: getattr(s, a) for a in dir(s)}
    for k, v in kwargs.items():
        if k not in d:
            fail("attempting to modify unknown field {!r}".format(k))
        d[k] = v
    return struct(**d)

def _extend(s, **kwargs):
    """Extend a struct's list attributes.

    Args:
        s: The struct to modify.
        **kwargs: The attributes to update the struct with. The value of each
            element can either be a single value or a list of values.

    Returns:
        A new struct where the value of each attribute specified in `kwargs`
        is set to the combination of the existing list of values if any and the
        values specified for the attribute in `kwargs`.

    Fails:
        If `kwargs` contains an item for an attribute not present on `s`.
        If any of the attributes to modify do not have list values.
    """
    d = {a: getattr(s, a) for a in dir(s)}
    for k, v in kwargs.items():
        if k not in d:
            fail("attempting to modify unknown field {!r}".format(k))
        value = d[k]
        if type(value) != type([]):
            fail("attempting to extend a non-list field {!r}".format(k))
        d[k] = args.listify(value, v)
    return struct(**d)

def _remove(s, **kwargs):
    """Remove elements from a struct's list attributes.

    Args:
        s: The struct to modify.
        **kwargs: The attributes to update the struct with. The value of each
            element can either be a single value or a list of values.

    Returns:
        A new struct where the value of each attribute specified in `kwargs`
        has the given values removed.

    Fails:
        If `kwargs` contains an item for an attribute not present on `s`.
        If any of the attributes to modify do not have list values.
        If any of the elements to remove do not exist in the specified
            attribute.
    """
    d = {a: getattr(s, a) for a in dir(s)}
    for k, v in kwargs.items():
        if k not in d:
            fail("attempting to modify unknown field {!r}".format(k))
        value = d[k]
        if type(value) != type([]):
            fail("attempting to remove elements from a non-list field {!r}"
                .format(k))
        for e in args.listify(v):
            value.remove(e)
        d[k] = value
    return struct(**d)

structs = struct(
    to_proto_properties = _to_proto_properties,
    evolve = _evolve,
    extend = _extend,
    remove = _remove,
)
