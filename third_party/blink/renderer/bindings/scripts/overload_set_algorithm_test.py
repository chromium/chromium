# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access

"""Unit tests for overload_set_algorithm.py."""

import unittest
from overload_set_algorithm import effective_overload_set


class EffectiveOverloadSetTest(unittest.TestCase):
    def test_example_in_comments(self):
        operation_list = [
            # f1: f(optional long x)
            {'arguments': [{'idl_type_object': 'long',
                            'is_optional': True,
                            'is_variadic': False}]},
            # f2: f(DOMString s)
            {'arguments': [{'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False}]}]

        overload_set = [
            # <f1, (long), (optional)>
            ({'arguments': [{'idl_type_object': 'long',
                             'is_optional': True,
                             'is_variadic': False}]},
             ('long',),
             ('optional',)),
            # <f1, (), ()>
            ({'arguments': [{'idl_type_object': 'long',
                             'is_optional': True,
                             'is_variadic': False}]},
             (),
             ()),
            # <f2, (DOMString), (required)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False}]},
             ('DOMString',),
             ('required',))]

        self.assertEqual(effective_overload_set(operation_list), overload_set)

    def test_example_in_spec(self):
        """Tests the example provided in Web IDL spec:
           https://heycam.github.io/webidl/#dfn-effective-overload-set,
           look for example right after the algorithm."""
        operation_list = [
            # f1: f(DOMString a)
            {'arguments': [{'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False}]},
            # f2: f(Node a, DOMString b, double... c)
            {'arguments': [{'idl_type_object': 'Node',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'double',
                            'is_optional': False,
                            'is_variadic': True}]},
            # f3: f()
            {'arguments': []},
            # f4: f(Event a, DOMString b, optional DOMString c, double... d)
            {'arguments': [{'idl_type_object': 'Event',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'DOMString',
                            'is_optional': True,
                            'is_variadic': False},
                           {'idl_type_object': 'double',
                            'is_optional': False,
                            'is_variadic': True}]}]

        overload_set = [
            # <f1, (DOMString), (required)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False}]},
             ('DOMString',),
             ('required',)),
            # <f2, (Node, DOMString, double), (required, required, variadic)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node', 'DOMString', 'double'),
             ('required', 'required', 'variadic')),
            # <f2, (Node, DOMString, double, double),
            #      (required, required, variadic, variadic)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node', 'DOMString', 'double', 'double'),
             ('required', 'required', 'variadic', 'variadic')),
            # <f2, (Node, DOMString), (required, required)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node', 'DOMString'),
             ('required', 'required')),
            # <f3, (), ()>
            ({'arguments': []},
             (),
             ()),
            # <f4, (Event, DOMString, DOMString, double),
            #      (required, required, optional, variadic)>
            ({'arguments': [{'idl_type_object': 'Event',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Event', 'DOMString', 'DOMString', 'double'),
             ('required', 'required', 'optional', 'variadic')),
            # <f4, (Event, DOMString, DOMString),
            #      (required, required, optional)>
            ({'arguments': [{'idl_type_object': 'Event',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Event', 'DOMString', 'DOMString'),
             ('required', 'required', 'optional')),
            # <f4, (Event, DOMString), (required, required)>
            ({'arguments': [{'idl_type_object': 'Event',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'double',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Event', 'DOMString'),
             ('required', 'required'))]

        self.assertEqual(effective_overload_set(operation_list), overload_set)

    def test_element_create_proposed_syntax(self):
        """Tests the proposed syntax for the convenience method Element.create.
           Github issue: https://github.com/whatwg/dom/issues/477"""
        operation_list = [
            # f1: f(DOMString tag, Record<DOMString, DOMString> attrs, (Node or DOMString)... children)
            {'arguments': [{'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'record<DOMString, DOMString>',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'NodeOrDOMString',
                            'is_optional': False,
                            'is_variadic': True}]},
            # f2: f(DOMString tag, (Node or DOMString)... children)
            {'arguments': [{'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'NodeOrDOMString',
                            'is_optional': False,
                            'is_variadic': True}]}]

        overload_set = [
            # <f1, (DOMString, Record, NodeOrDOMString), (required, required, variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'record<DOMString, DOMString>',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'NodeOrDOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'record<DOMString, DOMString>', 'NodeOrDOMString'),
             ('required', 'required', 'variadic')),
            # <f1, (DOMString, Record), (required, required)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'record<DOMString, DOMString>',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'NodeOrDOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'record<DOMString, DOMString>'),
             ('required', 'required')),
            # <f2, (DOMString, NodeOrDOMString), (required, variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'NodeOrDOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'NodeOrDOMString'),
             ('required', 'variadic')),
            # <f2, (DOMString, NodeOrDOMString, NodeOrDOMString), (required, variadic, variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'NodeOrDOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'NodeOrDOMString', 'NodeOrDOMString'),
             ('required', 'variadic', 'variadic')),
            # <f2, (DOMString), (required)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'NodeOrDOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString',),
             ('required',))]

        self.assertEqual(effective_overload_set(operation_list), overload_set)

    def test_optional_preceding_variadic(self):
        operation_list = [
            # f1: f(Node a, optional long b, DOMString... c)
            {'arguments': [{'idl_type_object': 'Node',
                            'is_optional': False,
                            'is_variadic': False},
                           {'idl_type_object': 'long',
                            'is_optional': True,
                            'is_variadic': False},
                           {'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': True}]},
            # f2: f(DOMString... a)
            {'arguments': [{'idl_type_object': 'DOMString',
                            'is_optional': False,
                            'is_variadic': True}]}]

        overload_set = [
            # <f1, (Node, long, DOMString), (required, optional, variadic)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'long',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node', 'long', 'DOMString'),
             ('required', 'optional', 'variadic')),
            # <f1, (Node, long), (required, optional)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'long',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node', 'long'),
             ('required', 'optional')),
            # <f1, (Node), (required)>
            ({'arguments': [{'idl_type_object': 'Node',
                             'is_optional': False,
                             'is_variadic': False},
                            {'idl_type_object': 'long',
                             'is_optional': True,
                             'is_variadic': False},
                            {'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('Node',),
             ('required',)),
            # <f2, (DOMString), (variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString',),
             ('variadic',)),
            # <f2, (DOMString, DOMString), (variadic, variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'DOMString'),
             ('variadic', 'variadic')),
            # <f2, (DOMString, DOMString, DOMString), (variadic, variadic, variadic)>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             ('DOMString', 'DOMString', 'DOMString'),
             ('variadic', 'variadic', 'variadic')),
            # <f2, (), ()>
            ({'arguments': [{'idl_type_object': 'DOMString',
                             'is_optional': False,
                             'is_variadic': True}]},
             (),
             ())]

        self.assertEqual(effective_overload_set(operation_list), overload_set)
