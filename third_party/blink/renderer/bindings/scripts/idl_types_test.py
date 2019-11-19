# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Unit tests for idl_types.py."""

import unittest

from idl_types import IdlNullableType
from idl_types import IdlRecordType
from idl_types import IdlSequenceType
from idl_types import IdlType
from idl_types import IdlUnionType


class IdlTypeTest(unittest.TestCase):

    def test_is_void(self):
        idl_type = IdlType('void')
        self.assertTrue(idl_type.is_void)
        idl_type = IdlType('somethingElse')
        self.assertFalse(idl_type.is_void)


class IdlRecordTypeTest(unittest.TestCase):

    def test_idl_types(self):
        idl_type = IdlRecordType(IdlType('USVString'), IdlType('long'))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 3)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'USVString')
        self.assertEqual(idl_types[2].name, 'Long')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type])

        idl_type = IdlRecordType(IdlType('DOMString'), IdlSequenceType(IdlType('unrestricted float')))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 4)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'String')
        self.assertEqual(idl_types[2].name, 'UnrestrictedFloatSequence')
        self.assertEqual(idl_types[3].name, 'UnrestrictedFloat')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type, idl_type.value_type.element_type])

        idl_type = IdlRecordType(IdlType('ByteString'),
                                 IdlRecordType(IdlType('DOMString'), IdlType('octet')))
        idl_types = list(idl_type.idl_types())
        self.assertEqual(len(idl_types), 5)
        self.assertIs(idl_types[0], idl_type)
        self.assertEqual(idl_types[1].name, 'ByteString')
        self.assertEqual(idl_types[2].name, 'StringOctetRecord')
        self.assertEqual(idl_types[3].name, 'String')
        self.assertEqual(idl_types[4].name, 'Octet')
        self.assertListEqual(list(idl_type.idl_types()),
                             [idl_type, idl_type.key_type, idl_type.value_type, idl_type.value_type.key_type,
                              idl_type.value_type.value_type])

    def test_is_record(self):
        idl_type = IdlType('USVString')
        self.assertFalse(idl_type.is_record_type)
        idl_type = IdlSequenceType(IdlRecordType(IdlType('DOMString'), IdlType('byte')))
        self.assertFalse(idl_type.is_record_type)
        idl_type = IdlRecordType(IdlType('USVString'), IdlType('long'))
        self.assertTrue(idl_type.is_record_type)
        idl_type = IdlRecordType(IdlType('USVString'), IdlSequenceType(IdlType('boolean')))
        self.assertTrue(idl_type.is_record_type)

    def test_name(self):
        idl_type = IdlRecordType(IdlType('ByteString'), IdlType('octet'))
        self.assertEqual(idl_type.name, 'ByteStringOctetRecord')
        idl_type = IdlRecordType(IdlType('USVString'), IdlSequenceType(IdlType('double')))
        self.assertEqual(idl_type.name, 'USVStringDoubleSequenceRecord')
        idl_type = IdlRecordType(IdlType('DOMString'),
                                 IdlRecordType(IdlType('ByteString'),
                                               IdlSequenceType(IdlType('unsigned short'))))
        self.assertEqual(idl_type.name, 'StringByteStringUnsignedShortSequenceRecordRecord')


class IdlUnionTypeTest(unittest.TestCase):

    def test_flattened_member_types(self):
        # We are only testing the algorithm here, so we do create some ambiguous union types.

        def compare_flattened_members(actual, expected):
            """Compare a set of IDL types by name, as the objects are different"""
            actual_names = set([member.name for member in actual])
            expected_names = set([member.name for member in expected])
            self.assertEqual(actual_names, expected_names)

        idl_type = IdlUnionType([IdlType('long'), IdlType('SomeInterface')])
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('long'), IdlType('SomeInterface')]))

        idl_type = IdlUnionType([IdlUnionType([IdlType('ByteString'), IdlType('float')]),
                                 IdlType('boolean')])
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('float'), IdlType('boolean'), IdlType('ByteString')]))

        idl_type = IdlUnionType([IdlUnionType([IdlType('ByteString'), IdlType('DOMString')]),
                                 IdlType('DOMString')])
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('DOMString'), IdlType('ByteString')]))

        idl_type = IdlUnionType(
            [IdlNullableType(IdlType('ByteString')), IdlType('byte')])
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('ByteString'), IdlType('byte')]))

        idl_type = IdlUnionType(
            [IdlNullableType(IdlType('ByteString')), IdlType('byte'),
             IdlUnionType([IdlType('ByteString'), IdlType('float')])])
        self.assertEqual(len(idl_type.flattened_member_types), 3)
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('ByteString'), IdlType('byte'), IdlType('float')]))

        # From the example in the spec: "the flattened member types of the union type (Node or (sequence<long> or Event) or
        # (XMLHttpRequest or DOMString)? or sequence<(sequence<double> or NodeList)>) are the six types Node, sequence<long>,
        # Event, XMLHttpRequest, DOMString and sequence<(sequence<double> or NodeList)>"
        idl_type = IdlUnionType(
            [IdlType('Node'),
             IdlUnionType([IdlSequenceType(IdlType('long')), IdlType('Event')]),
             IdlNullableType(IdlUnionType([IdlType('XMLHttpRequest'), IdlType('DOMString')])),
             IdlSequenceType(IdlUnionType([IdlSequenceType(IdlType('double')), IdlType('NodeList')]))])
        self.assertEqual(len(idl_type.flattened_member_types), 6)
        compare_flattened_members(
            idl_type.flattened_member_types,
            set([IdlType('Node'), IdlSequenceType(IdlType('long')), IdlType('Event'),
                 IdlType('XMLHttpRequest'), IdlType('DOMString'),
                 IdlSequenceType(IdlUnionType([IdlSequenceType(IdlType('double')), IdlType('NodeList')]))]))

    def test_resolve_typedefs(self):
        # This is a simplification of the typedef mechanism to avoid having to
        # use idl_definitions and use actual nodes from //tools/idl_parser.
        typedefs = {
            'Foo': IdlType('unsigned short'),
            'MyBooleanType': IdlType('boolean'),
            'SomeInterfaceT': IdlType('MyInterface'),
        }

        # (long long or MyBooleanType)
        union = IdlUnionType([IdlType('long long'), IdlType('MyBooleanType')]).resolve_typedefs(typedefs)
        self.assertEqual(union.name, 'LongLongOrBoolean')
        self.assertEqual(union.member_types[0].name, 'LongLong')
        self.assertEqual(union.member_types[1].name, 'Boolean')

        # (Foo or SomeInterfaceT)
        union = IdlUnionType([IdlType('Foo'), IdlType('SomeInterfaceT')]).resolve_typedefs(typedefs)
        self.assertEqual(union.name, 'UnsignedShortOrMyInterface')
        self.assertEqual(union.member_types[0].name, 'UnsignedShort')
        self.assertEqual(union.member_types[1].name, 'MyInterface')

        # (Foo or sequence<(MyBooleanType or double)>)
        union = IdlUnionType([
            IdlType('Foo'),
            IdlSequenceType(IdlUnionType([IdlType('MyBooleanType'),
                                          IdlType('double')]))]).resolve_typedefs(typedefs)
        self.assertEqual(union.name, 'UnsignedShortOrBooleanOrDoubleSequence')
        self.assertEqual(union.member_types[0].name, 'UnsignedShort')
        self.assertEqual(union.member_types[1].name, 'BooleanOrDoubleSequence')
        self.assertEqual(union.member_types[1].element_type.name, 'BooleanOrDouble')
        self.assertEqual(union.member_types[1].element_type.member_types[0].name,
                         'Boolean')
        self.assertEqual(union.member_types[1].element_type.member_types[1].name,
                         'Double')
        self.assertEqual(2, len(union.flattened_member_types))


class IdlNullableTypeTest(unittest.TestCase):
    _TEST_DICTIONARY = 'TestDictionary'

    def setUp(self):
        IdlType.dictionaries.add(self._TEST_DICTIONARY)

    def tearDown(self):
        IdlType.dictionaries.remove(self._TEST_DICTIONARY)

    def test_valid_inner_type_basic(self):
        inner_type = IdlType('long')
        self.assertTrue(IdlNullableType(inner_type).is_nullable)

    def test_valid_inner_type_dictionary(self):
        inner_type = IdlType(self._TEST_DICTIONARY)
        self.assertTrue(IdlNullableType(inner_type).is_nullable)

    def test_valid_inner_type_union(self):
        inner_type = IdlUnionType([IdlType('long')])
        self.assertTrue(IdlNullableType(inner_type).is_nullable)

    def test_invalid_inner_type_any(self):
        inner_type = IdlType('any')
        with self.assertRaises(ValueError):
            IdlNullableType(inner_type)

    def test_invalid_inner_type_promise(self):
        inner_type = IdlType('Promise')
        with self.assertRaises(ValueError):
            IdlNullableType(inner_type)

    def test_invalid_inner_type_nullable(self):
        inner_type = IdlNullableType(IdlType('long'))
        with self.assertRaises(ValueError):
            IdlNullableType(inner_type)

    def test_invalid_inner_type_union_nullable_member(self):
        inner_type = IdlUnionType([IdlNullableType(IdlType('long'))])
        with self.assertRaises(ValueError):
            IdlNullableType(inner_type)

    def test_invalid_inner_type_union_dictionary_member(self):
        inner_type = IdlUnionType([IdlType(self._TEST_DICTIONARY)])
        with self.assertRaises(ValueError):
            IdlNullableType(inner_type)
