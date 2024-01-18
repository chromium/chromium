# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

from mojom.generate import module as mojom
from mojom.generate import pack


class PackTest(unittest.TestCase):
  def testOrdinalOrder(self):
    struct = mojom.Struct('test')
    struct.AddField('testfield1', mojom.INT32, 2)
    struct.AddField('testfield2', mojom.INT32, 1)
    ps = pack.PackedStruct(struct)

    self.assertEqual(2, len(ps.packed_fields))
    self.assertEqual('testfield2', ps.packed_fields[0].field.mojom_name)
    self.assertEqual('testfield1', ps.packed_fields[1].field.mojom_name)

  def testZeroFields(self):
    struct = mojom.Struct('test')
    ps = pack.PackedStruct(struct)
    self.assertEqual(0, len(ps.packed_fields))

  def testOneField(self):
    struct = mojom.Struct('test')
    struct.AddField('testfield1', mojom.INT8)
    ps = pack.PackedStruct(struct)
    self.assertEqual(1, len(ps.packed_fields))

  def _CheckPackSequence(self, kinds, fields, offsets):
    """Checks the pack order and offsets of a sequence of mojom.Kinds.

    Args:
      kinds: A sequence of mojom.Kinds that specify the fields that are to be
      created.
      fields: The expected order of the resulting fields, with the integer "1"
      first.
      offsets: The expected order of offsets, with the integer "0" first.
    """
    struct = mojom.Struct('test')
    index = 1
    for kind in kinds:
      struct.AddField('%d' % index, kind)
      index += 1
    ps = pack.PackedStruct(struct)
    num_fields = len(ps.packed_fields)
    self.assertEqual(len(kinds), num_fields)
    for i in range(num_fields):
      self.assertEqual('%d' % fields[i], ps.packed_fields[i].field.mojom_name)
      self.assertEqual(offsets[i], ps.packed_fields[i].offset)

  def testPaddingPackedInOrder(self):
    return self._CheckPackSequence((mojom.INT8, mojom.UINT8, mojom.INT32),
                                   (1, 2, 3), (0, 1, 4))

  def testPaddingPackedOutOfOrder(self):
    return self._CheckPackSequence((mojom.INT8, mojom.INT32, mojom.UINT8),
                                   (1, 3, 2), (0, 1, 4))

  def testPaddingPackedOverflow(self):
    kinds = (mojom.INT8, mojom.INT32, mojom.INT16, mojom.INT8, mojom.INT8)
    # 2 bytes should be packed together first, followed by short, then by int.
    fields = (1, 4, 3, 2, 5)
    offsets = (0, 1, 2, 4, 8)
    return self._CheckPackSequence(kinds, fields, offsets)

  def testNullableTypes(self):
    kinds = (mojom.STRING.MakeNullableKind(), mojom.HANDLE.MakeNullableKind(),
             mojom.Struct('test_struct').MakeNullableKind(),
             mojom.DCPIPE.MakeNullableKind(), mojom.Array().MakeNullableKind(),
             mojom.DPPIPE.MakeNullableKind(),
             mojom.Array(length=5).MakeNullableKind(),
             mojom.MSGPIPE.MakeNullableKind(),
             mojom.Interface('test_interface').MakeNullableKind(),
             mojom.SHAREDBUFFER.MakeNullableKind(),
             mojom.PendingReceiver().MakeNullableKind())
    fields = (1, 2, 4, 3, 5, 6, 8, 7, 9, 10, 11)
    offsets = (0, 8, 12, 16, 24, 32, 36, 40, 48, 56, 60)
    return self._CheckPackSequence(kinds, fields, offsets)

  def testAllTypes(self):
    return self._CheckPackSequence(
        (mojom.BOOL, mojom.INT8, mojom.STRING, mojom.UINT8, mojom.INT16,
         mojom.DOUBLE, mojom.UINT16, mojom.INT32, mojom.UINT32, mojom.INT64,
         mojom.FLOAT, mojom.STRING, mojom.HANDLE, mojom.UINT64,
         mojom.Struct('test'), mojom.Array(), mojom.STRING.MakeNullableKind()),
        (1, 2, 4, 5, 7, 3, 6, 8, 9, 10, 11, 13, 12, 14, 15, 16, 17, 18),
        (0, 1, 2, 4, 6, 8, 16, 24, 28, 32, 40, 44, 48, 56, 64, 72, 80, 88))

  def testPaddingPackedOutOfOrderByOrdinal(self):
    struct = mojom.Struct('test')
    struct.AddField('testfield1', mojom.INT8)
    struct.AddField('testfield3', mojom.UINT8, 3)
    struct.AddField('testfield2', mojom.INT32, 2)
    ps = pack.PackedStruct(struct)
    self.assertEqual(3, len(ps.packed_fields))

    # Second byte should be packed in behind first, altering order.
    self.assertEqual('testfield1', ps.packed_fields[0].field.mojom_name)
    self.assertEqual('testfield3', ps.packed_fields[1].field.mojom_name)
    self.assertEqual('testfield2', ps.packed_fields[2].field.mojom_name)

    # Second byte should be packed with first.
    self.assertEqual(0, ps.packed_fields[0].offset)
    self.assertEqual(1, ps.packed_fields[1].offset)
    self.assertEqual(4, ps.packed_fields[2].offset)

  def testBools(self):
    struct = mojom.Struct('test')
    struct.AddField('bit0', mojom.BOOL)
    struct.AddField('bit1', mojom.BOOL)
    struct.AddField('int', mojom.INT32)
    struct.AddField('bit2', mojom.BOOL)
    struct.AddField('bit3', mojom.BOOL)
    struct.AddField('bit4', mojom.BOOL)
    struct.AddField('bit5', mojom.BOOL)
    struct.AddField('bit6', mojom.BOOL)
    struct.AddField('bit7', mojom.BOOL)
    struct.AddField('bit8', mojom.BOOL)
    ps = pack.PackedStruct(struct)
    self.assertEqual(10, len(ps.packed_fields))

    # First 8 bits packed together.
    for i in range(8):
      pf = ps.packed_fields[i]
      self.assertEqual(0, pf.offset)
      self.assertEqual("bit%d" % i, pf.field.mojom_name)
      self.assertEqual(i, pf.bit)

    # Ninth bit goes into second byte.
    self.assertEqual("bit8", ps.packed_fields[8].field.mojom_name)
    self.assertEqual(1, ps.packed_fields[8].offset)
    self.assertEqual(0, ps.packed_fields[8].bit)

    # int comes last.
    self.assertEqual("int", ps.packed_fields[9].field.mojom_name)
    self.assertEqual(4, ps.packed_fields[9].offset)

  def testMinVersion(self):
    """Tests that |min_version| is properly set for packed fields."""
    struct = mojom.Struct('test')
    struct.AddField('field_2', mojom.BOOL, 2)
    struct.AddField('field_0', mojom.INT32, 0)
    struct.AddField('field_1', mojom.INT64, 1)
    ps = pack.PackedStruct(struct)

    self.assertEqual('field_0', ps.packed_fields[0].field.mojom_name)
    self.assertEqual('field_2', ps.packed_fields[1].field.mojom_name)
    self.assertEqual('field_1', ps.packed_fields[2].field.mojom_name)

    self.assertEqual(0, ps.packed_fields[0].min_version)
    self.assertEqual(0, ps.packed_fields[1].min_version)
    self.assertEqual(0, ps.packed_fields[2].min_version)

    struct.fields[0].attributes = {'MinVersion': 1}
    ps = pack.PackedStruct(struct)

    self.assertEqual(0, ps.packed_fields[0].min_version)
    self.assertEqual(1, ps.packed_fields[1].min_version)
    self.assertEqual(0, ps.packed_fields[2].min_version)

  def testGetVersionInfoEmptyStruct(self):
    """Tests that pack.GetVersionInfo() never returns an empty list, even for
    empty structs.
    """
    struct = mojom.Struct('test')
    ps = pack.PackedStruct(struct)

    versions = pack.GetVersionInfo(ps)
    self.assertEqual(1, len(versions))
    self.assertEqual(0, versions[0].version)
    self.assertEqual(0, versions[0].num_fields)
    self.assertEqual(8, versions[0].num_bytes)

  def testGetVersionInfoComplexOrder(self):
    """Tests pack.GetVersionInfo() using a struct whose definition order,
    ordinal order and pack order for fields are all different.
    """
    struct = mojom.Struct('test')
    struct.AddField(
        'field_3', mojom.BOOL, ordinal=3, attributes={'MinVersion': 3})
    struct.AddField('field_0', mojom.INT32, ordinal=0)
    struct.AddField(
        'field_1', mojom.INT64, ordinal=1, attributes={'MinVersion': 2})
    struct.AddField(
        'field_2', mojom.INT64, ordinal=2, attributes={'MinVersion': 3})
    ps = pack.PackedStruct(struct)

    versions = pack.GetVersionInfo(ps)
    self.assertEqual(3, len(versions))

    self.assertEqual(0, versions[0].version)
    self.assertEqual(1, versions[0].num_fields)
    self.assertEqual(16, versions[0].num_bytes)

    self.assertEqual(2, versions[1].version)
    self.assertEqual(2, versions[1].num_fields)
    self.assertEqual(24, versions[1].num_bytes)

    self.assertEqual(3, versions[2].version)
    self.assertEqual(4, versions[2].num_fields)
    self.assertEqual(32, versions[2].num_bytes)

  def testGetVersionInfoPackedStruct(self):
    """Tests that pack.GetVersionInfo() correctly sets version, num_fields,
    and num_packed_fields for a packed struct.
    """
    struct = mojom.Struct('test')
    struct.AddField('field_0', mojom.BOOL, ordinal=0)
    struct.AddField('field_1',
                    mojom.NULLABLE_BOOL,
                    ordinal=1,
                    attributes={'MinVersion': 1})
    struct.AddField('field_2',
                    mojom.NULLABLE_BOOL,
                    ordinal=2,
                    attributes={'MinVersion': 2})
    ps = pack.PackedStruct(struct)
    versions = pack.GetVersionInfo(ps)

    self.assertEqual(3, len(versions))
    self.assertEqual(0, versions[0].version)
    self.assertEqual(1, versions[1].version)
    self.assertEqual(2, versions[2].version)
    self.assertEqual(1, versions[0].num_fields)
    self.assertEqual(2, versions[1].num_fields)
    self.assertEqual(3, versions[2].num_fields)
    self.assertEqual(1, versions[0].num_packed_fields)
    self.assertEqual(3, versions[1].num_packed_fields)
    self.assertEqual(5, versions[2].num_packed_fields)

  def testInterfaceAlignment(self):
    """Tests that interfaces are aligned on 4-byte boundaries, although the size
    of an interface is 8 bytes.
    """
    kinds = (mojom.INT32, mojom.Interface('test_interface'))
    fields = (1, 2)
    offsets = (0, 4)
    self._CheckPackSequence(kinds, fields, offsets)

  def testNullablePrimitives(self):
    """Tests that the nullable primitives are packed correctly"""
    struct = mojom.Struct('test')
    # The following struct should be created:
    # struct {
    #   bool field_$flag = 'true';
    #   int32 field_$value = 5;
    # }
    struct.AddField('field', mojom.NULLABLE_INT32, ordinal=0, default=5)

    fields = pack.PackedStruct(struct).packed_fields_in_ordinal_order

    self.assertEquals(2, len(fields))

    self.assertEquals('field_$flag', fields[0].field.name)
    self.assertEquals(mojom.BOOL, fields[0].field.kind)
    self.assertEquals('true', fields[0].field.default)

    self.assertEquals('field_$value', fields[1].field.name)
    self.assertEquals(mojom.INT32, fields[1].field.kind)
    self.assertEquals(5, fields[1].field.default)
