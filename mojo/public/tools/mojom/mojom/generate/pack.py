# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
from mojom.generate import module as mojom

# This module provides a mechanism for determining the packed order and offsets
# of a mojom.Struct.
#
# ps = pack.PackedStruct(struct)
# ps.packed_fields will access a list of PackedField objects, each of which
# will have an offset, a size and a bit (for mojom.BOOLs).

# Size of struct header in bytes: num_bytes [4B] + version [4B].
HEADER_SIZE = 8


class PackedField:
  kind_to_size = {
      mojom.BOOL: 1,
      mojom.INT8: 1,
      mojom.UINT8: 1,
      mojom.INT16: 2,
      mojom.UINT16: 2,
      mojom.INT32: 4,
      mojom.UINT32: 4,
      mojom.FLOAT: 4,
      mojom.HANDLE: 4,
      mojom.MSGPIPE: 4,
      mojom.SHAREDBUFFER: 4,
      mojom.PLATFORMHANDLE: 4,
      mojom.DCPIPE: 4,
      mojom.DPPIPE: 4,
      mojom.NULLABLE_HANDLE: 4,
      mojom.NULLABLE_MSGPIPE: 4,
      mojom.NULLABLE_SHAREDBUFFER: 4,
      mojom.NULLABLE_PLATFORMHANDLE: 4,
      mojom.NULLABLE_DCPIPE: 4,
      mojom.NULLABLE_DPPIPE: 4,
      mojom.INT64: 8,
      mojom.UINT64: 8,
      mojom.DOUBLE: 8,
      mojom.STRING: 8,
      mojom.NULLABLE_STRING: 8
  }

  @classmethod
  def GetSizeForKind(cls, kind):
    if isinstance(kind, (mojom.Array, mojom.Map, mojom.Struct, mojom.Interface,
                         mojom.PendingRemote, mojom.PendingAssociatedRemote)):
      return 8
    if isinstance(kind, mojom.Union):
      return 16
    if isinstance(kind, mojom.PendingReceiver):
      kind = mojom.MSGPIPE
    if isinstance(kind, mojom.PendingAssociatedReceiver):
      return 4
    if isinstance(kind, mojom.Enum):
      # TODO(mpcomplete): what about big enums?
      return cls.kind_to_size[mojom.INT32]
    if not kind in cls.kind_to_size:
      raise Exception("Undefined type: %s. Did you forget to import the file "
                      "containing the definition?" % kind.spec)
    return cls.kind_to_size[kind]

  @classmethod
  def GetAlignmentForKind(cls, kind):
    if isinstance(
        kind,
        (mojom.Interface, mojom.PendingRemote, mojom.PendingAssociatedRemote)):
      return 4
    if isinstance(kind, mojom.Union):
      return 8
    return cls.GetSizeForKind(kind)

  def __init__(self,
               field,
               index,
               ordinal,
               original_field=None,
               sub_ordinal=None,
               linked_value_packed_field=None):
    """
    Args:
      field: the original field.
      index: the position of the original field in the struct.
      ordinal: the ordinal of the field for serialization.
      original_field: See below.
      sub_ordinal: See below.
      linked_value_packed_field: See below.

    original_field, sub_ordinal, and linked_value_packed_field are used to
    support nullable ValueKind fields. For legacy reasons, nullable ValueKind
    fields actually generate two PackedFields. This allows:

    - backwards compatibility prior to Mojo support for nullable ValueKinds.
    - correct packing of fields for the aforementioned backwards compatibility.

    When translating Fields to PackedFields, the original field is turned into
    two PackedFields: the first PackedField always has type mojom.BOOL, while
    the second PackedField has the non-nullable version of the field's kind.

    When constructing these PackedFields, original_field references the field
    as defined in the mojom; the name as defined in the mojom will be used for
    all layers above the wire/data layer.

    sub_ordinal is used to sort the two PackedFields correctly with respect to
    each other: the first mojom.BOOL field always has sub_ordinal 0, while the
    second field always has sub_ordinal 1.

    Finally, linked_value_packed_field is used by the serialization and
    deserialization helpers, which generally just iterate over a PackedStruct's
    PackedField's in ordinal order. This allows the helpers to easily reference
    any related PackedFields rather than having to lookup related PackedFields
    by index while iterating.
    """
    self.field = field
    self.index = index
    self.ordinal = ordinal
    self.original_field = original_field
    self.sub_ordinal = sub_ordinal
    self.linked_value_packed_field = linked_value_packed_field
    self.size = self.GetSizeForKind(self.field.kind)
    self.alignment = self.GetAlignmentForKind(self.field.kind)
    self.offset = None
    self.bit = None
    self.min_version = None


def GetPad(offset, alignment):
  """Returns the pad necessary to reserve space so that |offset + pad| equals to
  some multiple of |alignment|."""
  return (alignment - (offset % alignment)) % alignment


def GetFieldOffset(field, last_field):
  """Returns a 2-tuple of the field offset and bit (for BOOLs)."""
  if (field.field.kind == mojom.BOOL and last_field.field.kind == mojom.BOOL
      and last_field.bit < 7):
    return (last_field.offset, last_field.bit + 1)

  offset = last_field.offset + last_field.size
  pad = GetPad(offset, field.alignment)
  return (offset + pad, 0)


def GetPayloadSizeUpToField(field):
  """Returns the payload size (not including struct header) if |field| is the
  last field.
  """
  if not field:
    return 0
  offset = field.offset + field.size
  pad = GetPad(offset, 8)
  return offset + pad


def IsNullableValueKindPackedField(field):
  """Returns true if `field` is derived from a nullable ValueKind field.

  Nullable ValueKind fields often require special handling in the bindings due
  to the way the implementation is constrained for wire compatibility.
  """
  assert isinstance(field, PackedField)
  return field.sub_ordinal is not None


def IsPrimaryNullableValueKindPackedField(field):
  """Returns true if `field` is derived from a nullable ValueKind mojom field
  and is the "primary" field.

  The primary field is a bool PackedField that controls if the field should be
  considered as present or not; it will have a reference to the PackedField that
  holds the actual value representation if considered present.

  Bindings code that translates between the wire protocol and the higher layers
  can use this to simplify mapping multiple PackedFields to the single field
  that is logically exposed to bindings consumers.
  """
  assert isinstance(field, PackedField)
  return field.linked_value_packed_field is not None


class PackedStruct:
  def __init__(self, struct):
    self.struct = struct
    # |packed_fields| contains all the fields, in increasing offset order.
    self.packed_fields = []
    # |packed_fields_in_ordinal_order| refers to the same fields as
    # |packed_fields|, but in ordinal order.
    self.packed_fields_in_ordinal_order = []

    # No fields.
    if (len(struct.fields) == 0):
      return

    # Start by sorting by ordinal.
    src_fields = self.packed_fields_in_ordinal_order
    ordinal = 0
    for index, field in enumerate(struct.fields):
      if field.ordinal is not None:
        ordinal = field.ordinal
      # Nullable value types are a bit weird: they generate two PackedFields
      # despite being a single ValueKind. This is for wire compatibility to
      # ease the transition from legacy mojom syntax where nullable value types
      # were not supported.
      if isinstance(field.kind, mojom.ValueKind) and field.kind.is_nullable:
        # The suffixes start with a '$' to avoid collision with user defined
        # identifiers.
        has_value_field = copy.copy(field)
        has_value_field.name = f'{field.mojom_name}_$flag'
        has_value_field.kind = mojom.BOOL
        if field.default:
          has_value_field.default = 'true'

        value_field = copy.copy(field)
        value_field.name = f'{field.mojom_name}_$value'
        value_field.kind = field.kind.MakeUnnullableKind()

        value_packed_field = PackedField(value_field,
                                         index,
                                         ordinal,
                                         original_field=field,
                                         sub_ordinal=1,
                                         linked_value_packed_field=None)
        has_value_packed_field = PackedField(
            has_value_field,
            index,
            ordinal,
            original_field=field,
            sub_ordinal=0,
            linked_value_packed_field=value_packed_field)
        src_fields.append(has_value_packed_field)
        src_fields.append(value_packed_field)
      else:
        src_fields.append(PackedField(field, index, ordinal))
      ordinal += 1
    src_fields.sort(key=lambda field: (field.ordinal, field.sub_ordinal))

    # Set |min_version| for each field.
    next_min_version = 0
    for packed_field in src_fields:
      if packed_field.field.min_version is None:
        assert next_min_version == 0
      else:
        assert packed_field.field.min_version >= next_min_version
        next_min_version = packed_field.field.min_version
      packed_field.min_version = next_min_version

      if (packed_field.min_version != 0
          and mojom.IsReferenceKind(packed_field.field.kind)
          and not packed_field.field.kind.is_nullable):
        raise Exception(
            "Non-nullable reference fields are only allowed in version 0 of a "
            "struct. %s.%s is defined with [MinVersion=%d]." %
            (self.struct.mojom_name, packed_field.field.mojom_name,
             packed_field.min_version))

    src_field = src_fields[0]
    src_field.offset = 0
    src_field.bit = 0
    dst_fields = self.packed_fields
    dst_fields.append(src_field)

    # Then find first slot that each field will fit.
    for src_field in src_fields[1:]:
      last_field = dst_fields[0]
      for i in range(1, len(dst_fields)):
        next_field = dst_fields[i]
        offset, bit = GetFieldOffset(src_field, last_field)
        if offset + src_field.size <= next_field.offset:
          # Found hole.
          src_field.offset = offset
          src_field.bit = bit
          dst_fields.insert(i, src_field)
          break
        last_field = next_field
      if src_field.offset is None:
        # Add to end
        src_field.offset, src_field.bit = GetFieldOffset(src_field, last_field)
        dst_fields.append(src_field)


class ByteInfo:
  def __init__(self):
    self.is_padding = False
    self.packed_fields = []


def GetByteLayout(packed_struct):
  total_payload_size = GetPayloadSizeUpToField(
      packed_struct.packed_fields[-1] if packed_struct.packed_fields else None)
  byte_info = [ByteInfo() for i in range(total_payload_size)]

  limit_of_previous_field = 0
  for packed_field in packed_struct.packed_fields:
    for i in range(limit_of_previous_field, packed_field.offset):
      byte_info[i].is_padding = True
    byte_info[packed_field.offset].packed_fields.append(packed_field)
    limit_of_previous_field = packed_field.offset + packed_field.size

  for i in range(limit_of_previous_field, len(byte_info)):
    byte_info[i].is_padding = True

  for byte in byte_info:
    # A given byte cannot both be padding and have a fields packed into it.
    assert not (byte.is_padding and byte.packed_fields)

  return byte_info


class VersionInfo:
  def __init__(self, version, num_fields, num_packed_fields, num_bytes):
    self.version = version
    self.num_fields = num_fields
    self.num_packed_fields = num_packed_fields
    self.num_bytes = num_bytes


def GetVersionInfo(packed_struct):
  """Get version information for a struct.

  Args:
    packed_struct: A PackedStruct instance.

  Returns:
    A non-empty list of VersionInfo instances, sorted by version in increasing
    order.
    Note: The version numbers may not be consecutive.
  """
  versions = []
  last_version = 0
  last_num_fields = 0
  last_num_packed_fields = 0
  last_payload_size = 0

  for packed_field in packed_struct.packed_fields_in_ordinal_order:
    if packed_field.min_version != last_version:
      versions.append(
          VersionInfo(last_version, last_num_fields, last_num_packed_fields,
                      last_payload_size + HEADER_SIZE))
      last_version = packed_field.min_version

    # Nullable numeric fields (e.g. `int32?`) expand to two packed fields, so to
    # avoid double-counting, only increment if the field is:
    # - not used for representing a nullable value kind field, or
    # - the primary field representing the nullable value kind field.
    last_num_fields += 1 if (
        not IsNullableValueKindPackedField(packed_field)
        or IsPrimaryNullableValueKindPackedField(packed_field)) else 0

    last_num_packed_fields += 1

    # The fields are iterated in ordinal order here. However, the size of a
    # version is determined by the last field of that version in pack order,
    # instead of ordinal order. Therefore, we need to calculate the max value.
    last_payload_size = max(GetPayloadSizeUpToField(packed_field),
                            last_payload_size)

  assert len(
      versions) == 0 or last_num_packed_fields != versions[-1].num_packed_fields
  versions.append(
      VersionInfo(last_version, last_num_fields, last_num_packed_fields,
                  last_payload_size + HEADER_SIZE))
  return versions
