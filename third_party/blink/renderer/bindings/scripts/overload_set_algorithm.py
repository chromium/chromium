# Copyright 2017 The Chromium Authors. All rights reserved.
# coding=utf-8
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import Counter
import itertools
from operator import itemgetter


def sort_and_groupby(list_to_sort, key=None):
    """Returns a generator of (key, list), sorting and grouping list by key."""
    list_to_sort.sort(key=key)
    return ((k, list(g)) for k, g in itertools.groupby(list_to_sort, key))


def effective_overload_set(F):  # pylint: disable=invalid-name
    """Returns the effective overload set of an overloaded function.

    An effective overload set is the set of overloaded functions + signatures
    derived from the set F by transforming it into the distinct permutations of
    function invocations possible given the argument types. It is used by the
    overload resolution algorithm.

    For example, given input:
    [
        f1(DOMString a),
        f2(Node a, DOMString b, double... c),
        f3(),
        f4(Event a, DOMString b, optional DOMString c, double... d)
    ]

    The output is:
    [
        (f1, [DOMString], [required]),
        (f2, [Node, DOMString], [required, required]),
        (f2, [Node, DOMString, double], [required, required, variadic]),
        (f2, [Node, DOMString, double, double], [required, required, variadic, variadic]),
        (f3, [], []),
        (f4, [Event, DOMString], [required, required],
        (f4, [Event, DOMString, DOMString], [required, required, optional]),
        (f4, [Event, DOMString, DOMString, double], [required, required, optional, variadic])
    ]

    Spec: http://heycam.github.io/webidl/#dfn-effective-overload-set.

    Formally the input and output lists are sets, but methods are stored
    internally as dicts, which can't be stored in a set because they are not
    hashable, so we use lists instead.

    Arguments:
        F: list of overloads for a given callable name.

    Returns:
        S: list of tuples of the form:
        (callable, tuple(type list), tuple(optionality list)).
    """
    # Code closely follows the algorithm in the spec, for clarity and
    # correctness, and hence is not very Pythonic.

    # 1. Let S be an ordered set.
    # (We use a list because we can't use a set, as noted above.)
    S = []  # pylint: disable=invalid-name

    # 2. Let F be an ordered set with items as follows, according to the kind of
    # effective overload set:
    # (Passed as argument, nothing to do.)

    # 3. Let maxarg be the maximum number of arguments the operations,
    # constructor extended attributes or callback functions in F are declared
    # to take. For variadic operations and constructor extended attributes,
    # the argument on which the ellipsis appears counts as a single argument.
    # Note: So void f(long x, long... y); is considered to be declared to take
    # two arguments.
    # X is the "callable".
    maxarg = max([len(X['arguments']) for X in F])

    # 4. Let max be max(maxarg, N).
    # Per: https://github.com/heycam/webidl/issues/600.

    # The effective overload set as defined in the Web IDL spec is used at
    # runtime as an input to the overload resolution algorithm. The runtime
    # portion of the overload resolution algorithm includes coercing arguments
    # into the proper type. To perform that coercion, the effective overload set
    # must produce variadic entries in the type list to match the number of
    # arguments supplied for the invocation (N) of the function.

    # Our use of the effective overload set, however, is limited to determining
    # which function overload should handle the invocation. Coercion of
    # arguments is a separate problem making N irrelevant and max always equal
    # to maxarg.

    # 5. For each operation, extended attribute or callback function X in F:
    for X in F:  # pylint: disable=invalid-name
        # 5.1. Let arguments be the list of arguments X is declared to take.
        arguments = X['arguments']  # pylint: disable=invalid-name
        # 5.2. Let n be the size of arguments.
        n = len(arguments)  # pylint: disable=invalid-name
        # 5.3. Let types be a type list.
        # 5.4. Let optionalityValues be an optionality list.
        types, optionality_values = [], []

        # 5.5. For each argument in arguments:
        for argument in arguments:
            # 5.5.1. Append the type of argument to types.
            types.append(argument['idl_type_object'])
            # 5.5.2. Append "variadic" to optionalityValues if argument is a
            # final, variadic argument, "optional" if argument is optional,
            # and "required" otherwise.
            if argument['is_variadic']:
                optionality_values.append('variadic')
            elif argument['is_optional']:
                optionality_values.append('optional')
            else:
                optionality_values.append('required')

        # 5.6. Append the tuple (X, types, optionalityValues) to S.
        S.append((X, tuple(types), tuple(optionality_values)))

        # 5.7. If X is declared to be variadic, then:
        if optionality_values and optionality_values[-1] == 'variadic':
            # 5.7.1. For each i in the range n to max - 1, inclusive:
            for i in range(n, maxarg):
                # 5.7.1.1. Let t be a type list.
                # 5.7.1.2. Let o be an optionality list.
                type_list, optionality_list = [], []
                # 5.7.1.3. For each j in the range 0 to n-1, inclusive:
                for j in range(0, n):
                    # 5.7.1.3.1. Append types[j] to t.
                    type_list.append(types[j])
                    # 5.7.1.3.2. Append optionalityValues[j] to o.
                    optionality_list.append(optionality_values[j])
                # 5.7.1.4. For each j in the range n to i, inclusive:
                for j in range(n, i + 1):
                    # 5.7.1.4.1. Append types[n - 1] to t.
                    type_list.append(types[n - 1])
                    # 5.7.1.4.2. Append "variadic" to o.
                    optionality_list.append('variadic')
                # 5.7.1.5. Append the tuple (X, t, o) to S.
                S.append((X, tuple(type_list), tuple(optionality_list)))
        # 5.8. Let i be n - 1.
        i = n - 1

        # 5.9. While i ≥ 0:
        while i >= 0:
            # 5.9.1. If arguments[i] is not optional (i.e., it is not marked as
            # "optional" and is not a final, variadic argument), then break.
            if optionality_values[i] == 'required':
                break

            # 5.9.2. Let t be a type list.
            # 5.9.3. Let o be an optionality list.
            type_list, optionality_list = [], []
            # 5.9.4. For each j in the range 0 to i-1, inclusive:
            for j in range(0, i):
                # 5.9.4.1. Append types[j] to t.
                type_list.append(types[j])
                # 5.9.4.2. Append optionalityValues[j] to o.
                optionality_list.append(optionality_values[j])
            # 5.9.5. Append the tuple (X, t, o) to S.
            # Note: if i is 0, this means to add to S the tuple (X, « », « »);
            # (where "« »" represents an empty list).
            S.append((X, tuple(type_list), tuple(optionality_list)))
            # 5.9.6. Set i to i−1.
            i = i - 1

    # 6. Return S.
    return S


def effective_overload_set_by_length(overloads):
    def type_list_length(entry):
        # Entries in the effective overload set are 3-tuples:
        # (callable, type list, optionality list)
        return len(entry[1])

    effective_overloads = effective_overload_set(overloads)
    return list(sort_and_groupby(effective_overloads, type_list_length))


def method_overloads_by_name(methods):
    """Returns generator of overloaded methods by name: [name, [method]]"""
    # Filter to only methods that are actually overloaded
    method_counts = Counter(method['name'] for method in methods)
    overloaded_method_names = set(name
                                  for name, count in method_counts.iteritems()
                                  if count > 1)
    overloaded_methods = [method for method in methods
                          if method['name'] in overloaded_method_names]

    # Group by name (generally will be defined together, but not necessarily)
    return sort_and_groupby(overloaded_methods, itemgetter('name'))
