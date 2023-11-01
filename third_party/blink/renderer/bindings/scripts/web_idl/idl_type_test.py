# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributes
from .idl_type import IdlTypeFactory


class IdlTypesTest(unittest.TestCase):
    def test_property(self):
        factory = IdlTypeFactory()

        self.assertTrue(factory.simple_type('any').is_any)
        self.assertTrue(factory.simple_type('bigint').is_bigint)
        self.assertTrue(factory.simple_type('boolean').is_boolean)
        self.assertTrue(factory.simple_type('object').is_object)
        self.assertTrue(factory.simple_type('undefined').is_undefined)
        self.assertTrue(factory.simple_type('symbol').is_symbol)

        for x in ('byte', 'octet', 'short', 'unsigned short', 'long',
                  'unsigned long', 'long long', 'unsigned long long'):
            self.assertTrue(factory.simple_type(x).is_numeric)
            self.assertTrue(factory.simple_type(x).is_integer)
        for x in ('float', 'unrestricted float', 'double',
                  'unrestricted double'):
            self.assertTrue(factory.simple_type(x).is_numeric)
            self.assertFalse(factory.simple_type(x).is_integer)
        for x in ('DOMString', 'ByteString', 'USVString'):
            self.assertTrue(factory.simple_type(x).is_string)

        short_type = factory.simple_type('short')
        string_type = factory.simple_type('DOMString')
        self.assertTrue(factory.promise_type(short_type).is_promise)
        self.assertTrue(factory.record_type(short_type, string_type).is_record)
        self.assertTrue(factory.sequence_type(short_type).is_sequence)
        self.assertTrue(factory.frozen_array_type(short_type).is_frozen_array)
        self.assertTrue(factory.union_type([short_type, string_type]).is_union)
        self.assertTrue(factory.nullable_type(short_type).is_nullable)
        self.assertTrue(factory.variadic_type(short_type).is_variadic)

        self.assertFalse(factory.simple_type('long').is_string)
        self.assertFalse(factory.simple_type('DOMString').is_object)
        self.assertFalse(factory.simple_type('symbol').is_string)

        self.assertFalse(factory.nullable_type(short_type).is_numeric)
        self.assertFalse(factory.variadic_type(short_type).is_numeric)
        self.assertTrue(
            factory.nullable_type(short_type).inner_type.is_numeric)
        self.assertTrue(
            factory.variadic_type(short_type).element_type.is_numeric)

        ext_attrs = ExtendedAttributes([ExtendedAttribute('Clamp')])
        annotated_type = factory.simple_type(
            'short', extended_attributes=ext_attrs)
        self.assertTrue(annotated_type.extended_attributes)
        self.assertTrue(annotated_type.is_numeric)

        optional_type = factory.simple_type('DOMString', is_optional=True)
        self.assertTrue(optional_type.is_optional)
        self.assertTrue(optional_type.is_string)

        annotated_optional = factory.simple_type(
            'long', is_optional=True, extended_attributes=ext_attrs)
        self.assertTrue(annotated_optional.extended_attributes)
        self.assertTrue(annotated_optional.is_optional)
        self.assertTrue(annotated_optional.is_numeric)

    def test_type_name(self):
        factory = IdlTypeFactory()

        type_names = {
            'byte': 'Byte',
            'unsigned long long': 'UnsignedLongLong',
            'unrestricted double': 'UnrestrictedDouble',
            'DOMString': 'String',
            'ByteString': 'ByteString',
            'USVString': 'USVString',
            'any': 'Any',
            'bigint': 'Bigint',
            'boolean': 'Boolean',
            'object': 'Object',
            'undefined': 'Undefined',
            'symbol': 'Symbol',
        }
        for name, expect in type_names.items():
            self.assertEqual(expect, factory.simple_type(name).type_name)

        short_type = factory.simple_type('short')
        string_type = factory.simple_type('DOMString')
        self.assertEqual(
            'ShortOrString',
            factory.union_type([short_type, string_type]).type_name)
        self.assertEqual(
            'ShortOrString',
            factory.union_type([string_type, short_type]).type_name)
        self.assertEqual('ShortPromise',
                         factory.promise_type(short_type).type_name)
        self.assertEqual(
            'ShortStringRecord',
            factory.record_type(short_type, string_type).type_name)
        self.assertEqual('ShortSequence',
                         factory.sequence_type(short_type).type_name)
        self.assertEqual('ShortArray',
                         factory.frozen_array_type(short_type).type_name)
        self.assertEqual('ShortOrNull',
                         factory.nullable_type(short_type).type_name)

        ext_attrs = ExtendedAttributes(
            [ExtendedAttribute('TreatNullAs', 'EmptyString')])
        self.assertEqual(
            'StringTreatNullAs',
            factory.simple_type('DOMString',
                                extended_attributes=ext_attrs).type_name)

    def test_union_types(self):
        factory = IdlTypeFactory()

        # Test target: ((unrestricted double or object)? or
        #               [TreatNullAs=EmptyString] DOMString)
        treat_null_as = ExtendedAttribute('TreatNullAs', 'EmptyString')
        annotated_string = factory.simple_type(
            'DOMString',
            extended_attributes=ExtendedAttributes([treat_null_as]))
        obj = factory.simple_type('object')
        unrestricted_double = factory.simple_type('unrestricted double')
        union = factory.union_type(
            [factory.union_type([unrestricted_double, obj]), annotated_string])

        self.assertEqual(len(union.member_types), 2)
        # TODO(peria): Enable following tests.
        # self.assertEqual(len(union.flattened_member_types), 3)
        # self.assertTrue(union.does_include_nullable_type)

    # TODO(peria): Implement tests for ReferenceType, DefinitionType, and
    # TypeAlias
