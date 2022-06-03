# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for extended_attributes.py."""

import unittest

from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributes


class TestableExtendedAttribute(ExtendedAttribute):
    FORM_NO_ARGS = ExtendedAttribute._FORM_NO_ARGS
    FORM_IDENT = ExtendedAttribute._FORM_IDENT
    FORM_IDENT_LIST = ExtendedAttribute._FORM_IDENT_LIST
    FORM_ARG_LIST = ExtendedAttribute._FORM_ARG_LIST
    FORM_NAMED_ARG_LIST = ExtendedAttribute._FORM_NAMED_ARG_LIST

    def __init__(self, key, values=None, arguments=None, name=None):
        super(TestableExtendedAttribute, self).__init__(
            key=key, values=values, arguments=arguments, name=name)

    @property
    def format(self):
        return self._format


class ExtendedAttributeTest(unittest.TestCase):
    def test_attribute_create(self):
        # NoArgs
        attr = TestableExtendedAttribute(key='Foo')
        self.assertEqual(attr.format, TestableExtendedAttribute.FORM_NO_ARGS)
        self.assertEqual(attr.key, 'Foo')
        self.assertEqual(attr.value, None)
        self.assertEqual(attr.values, ())
        with self.assertRaises(ValueError):
            _ = attr.arguments
        with self.assertRaises(ValueError):
            _ = attr.name

        # Ident
        attr = TestableExtendedAttribute(key='Bar', values='Val')
        self.assertEqual(attr.format, TestableExtendedAttribute.FORM_IDENT)
        self.assertEqual(attr.key, 'Bar')
        self.assertEqual(attr.value, 'Val')
        self.assertEqual(attr.values, ('Val', ))
        with self.assertRaises(ValueError):
            _ = attr.arguments
        with self.assertRaises(ValueError):
            _ = attr.name

        # IdentList
        attr = TestableExtendedAttribute(key='Buz', values=('Val', 'ue'))
        self.assertEqual(attr.format,
                         TestableExtendedAttribute.FORM_IDENT_LIST)
        self.assertEqual(attr.key, 'Buz')
        self.assertEqual(attr.values, ('Val', 'ue'))
        attr = TestableExtendedAttribute(
            key='IdentList', values=['Val', 'ue', 'List'])
        self.assertEqual(attr.values, ('Val', 'ue', 'List'))
        with self.assertRaises(ValueError):
            _ = attr.arguments
        with self.assertRaises(ValueError):
            _ = attr.name

        # ArgList
        attr = TestableExtendedAttribute(
            key='Foo', arguments=(('Left', 'Right'), ('foo', 'bar')))
        self.assertEqual(attr.format, TestableExtendedAttribute.FORM_ARG_LIST)
        self.assertEqual(attr.key, 'Foo')
        with self.assertRaises(ValueError):
            _ = attr.values
        self.assertEqual(attr.arguments, (('Left', 'Right'), ('foo', 'bar')))
        with self.assertRaises(ValueError):
            _ = attr.name

        # NamedArgList
        attr = TestableExtendedAttribute(
            key='Bar', arguments=(('Left', 'Right'), ), name='Buz')
        self.assertEqual(attr.format,
                         TestableExtendedAttribute.FORM_NAMED_ARG_LIST)
        self.assertEqual(attr.key, 'Bar')
        with self.assertRaises(ValueError):
            _ = attr.values
        self.assertEqual(attr.arguments, (('Left', 'Right'), ))
        self.assertEqual(attr.name, 'Buz')

    def test_attributes(self):
        attrs = [
            ExtendedAttribute(key='A', values='val'),
            ExtendedAttribute(key='B'),
            ExtendedAttribute(key='C', values=('Val', 'ue')),
            ExtendedAttribute(key='B', values=('Val', 'ue', 'B'))
        ]
        attributes = ExtendedAttributes(attrs)
        self.assertTrue('A' in attributes)
        self.assertFalse('D' in attributes)
        self.assertEqual(attributes.get('A').value, 'val')
        b_values = attributes.get_list_of('B')
        self.assertEqual(b_values[0].values, ())
        self.assertEqual(b_values[1].values, ('Val', 'ue', 'B'))
