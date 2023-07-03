# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def make_copy(obj, memo=None):
    """
    Creates a copy of the given object, which should be an IR or part of IR.

    The copy is created basically as a deep copy of the object, but |make_copy|
    method is used to create a (part of) copy if the object (or part of it) has
    the method.  |memo| argument behaves as the same as |deepcopy|.
    """

    if memo is None:
        memo = dict()

    if obj is None or isinstance(obj, (bool, int, float, complex, str)):
        # Do not make a copy if the object is of an immutable primitive type
        # (or its subclass).
        #
        # Memoization is tricky in case that both of Identifier('x') and
        # Component('x') exist.  We could memoize them as
        # |memo[(type(obj), obj)] = copy|, though.
        return obj

    if hasattr(obj, 'make_copy'):
        return obj.make_copy(memo=memo)

    memoizable = obj.__hash__ is not None

    if memoizable:
        copy = memo.get(obj, None)
        if copy is not None:
            return copy

    def memoize(copy):
        if memoizable:
            memo[obj] = copy
        return copy

    cls = type(obj)

    if isinstance(obj, (list, tuple, set, frozenset)):
        return memoize(cls(map(lambda x: make_copy(x, memo), obj)))

    if isinstance(obj, dict):
        return memoize(
            cls([(make_copy(key, memo), make_copy(value, memo))
                 for key, value in obj.items()]))

    if hasattr(obj, '__dict__'):
        copy = memoize(cls.__new__(cls))
        for name, value in obj.__dict__.items():
            setattr(copy, name, make_copy(value, memo))
        return copy

    assert False, 'Unsupported type of object: {}'.format(cls)
