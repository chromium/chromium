# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom.generate import pack
from mojom.generate import module as mojom
from functools import singledispatchmethod


class BackwardCompatibilityChecker:
  """Used for memoization while recursively checking two type definitions for
  backward-compatibility."""

  def __init__(self):
    self._cache = {}

  def IsBackwardCompatible(self, new_kind: mojom.Kind, old_kind: mojom.Kind):
    key = (new_kind, old_kind)
    result = self._cache.get(key)
    if result is None:
      # Ensure that the type of two types are compatible before doing more work.
      if isinstance(new_kind, type(old_kind)):
        # Assume they're compatible at first to effectively ignore recursive
        # checks between these types, e.g. if both kinds are a struct or union
        # that references itself in a field.
        self._cache[key] = True
        result = self._CheckCompat(new_kind, old_kind)
        self._cache[key] = result
      else:
        self._cache[key] = False
    return result

  def _IsFieldBackwardCompatible(self, new_field: mojom.Kind,
                                 old_field: mojom.Kind):
    if (new_field.min_version or 0) != (old_field.min_version or 0):
      return False

    return self.IsBackwardCompatible(new_field.kind, old_field.kind)

  # Each type should register their own compatibility check under this
  # dispatcher. Assume that both new and old are the same type when the
  # type specific compatibility checker is invoked.
  @singledispatchmethod
  def _CheckCompat(self, new, old):
    raise NotImplementedError("unknown types: (%s, %s)" %
                              (repr(new), repr(old)))

  @_CheckCompat.register(mojom.Kind)
  def _(self, new: mojom.Kind, old: mojom.Kind):
    return new == old

  @_CheckCompat.register(mojom.Struct)
  def _(self, new: mojom.Struct, old: mojom.Struct):
    """The new struct is backward-compatible with older_struct if and only if
    all of the following conditions hold:
      - Any newly added field is tagged with a [MinVersion] attribute specifying
        a version number greater than all previously used [MinVersion]
        attributes within the struct.
      - All fields present in the old struct remain present in the new struct,
        with the same ordinal position, same optional or non-optional status,
        same (or backward-compatible) type and where applicable, the same
        [MinVersion] attribute value.
      - All [MinVersion] attributes must be non-decreasing in ordinal order.
      - All reference-typed (string, array, map, struct, or union) fields tagged
        with a [MinVersion] greater than zero must be optional.
    """

    def buildOrdinalFieldMap(struct):
      fields_by_ordinal = {}
      for field in struct.fields:
        if field.ordinal in fields_by_ordinal:
          raise Exception('Multiple fields with ordinal %s in struct %s.' %
                          (field.ordinal, struct.mojom_name))
        fields_by_ordinal[field.ordinal] = field
      return fields_by_ordinal

    new_fields = buildOrdinalFieldMap(new)
    old_fields = buildOrdinalFieldMap(old)
    if len(new_fields) < len(old_fields):
      # At least one field was removed, which is not OK.
      raise Exception('Removing struct fields from struct %s is not allowed.' %
                      (new.mojom_name))

    # If there are N fields, existing ordinal values must exactly cover the
    # range from 0 to N-1.
    num_old_ordinals = len(old_fields)
    max_old_min_version = 0
    for ordinal in range(num_old_ordinals):
      new_field = new_fields[ordinal]
      old_field = old_fields[ordinal]
      if (old_field.min_version or 0) > max_old_min_version:
        max_old_min_version = old_field.min_version
      if not self._IsFieldBackwardCompatible(new_field, old_field):
        # Type or min-version mismatch between old and new versions of the same
        # ordinal field.
        raise Exception(
            'Struct %s field with ordinal value %d have different type'
            ' or min version, old name %s, new name %s.' %
            (new.mojom_name, ordinal, old_field.mojom_name,
             new_field.mojom_name))

    # At this point we know all old fields are intact in the new struct
    # definition. Now verify that all new fields have a high enough min version
    # and are appropriately optional where required.
    num_new_ordinals = len(new_fields)
    last_min_version = max_old_min_version
    for ordinal in range(num_old_ordinals, num_new_ordinals):
      new_field = new_fields[ordinal]
      min_version = new_field.min_version or 0
      if min_version <= max_old_min_version:
        # A new field is being added to an existing version, which is not OK.
        raise Exception(
            'Adding new fields to an existing MinVersion is not allowed'
            ' for struct %s' % (new.mojom_name))
      if min_version < last_min_version:
        # The [MinVersion] of a field cannot be lower than the [MinVersion] of
        # a field with lower ordinal value.
        raise Exception(
            'MinVersion of struct %s field %s cannot be lower than MinVersion'
            ' of preceding fields' % (new.mojom_name, new_field))
      if mojom.IsReferenceKind(
          new_field.kind) and not mojom.IsNullableKind(new_field.kind):
        # New fields whose type can be nullable MUST be nullable.
        raise Exception('New struct %s field %s must be nullable' %
                        (new.mojom_name, new_field))

    return True

  @_CheckCompat.register(mojom.Union)
  def _(self, new: mojom.Union, old: mojom.Union):
    """This union is backward-compatible with old union if and only if
    all of the following conditions hold:
      - Any newly added field is tagged with a [MinVersion] attribute specifying
        a version number greater than all previously used [MinVersion]
        attributes within the union.
      - All fields present in old union remain present in the new union,
        with the same ordinal value, same optional or non-optional status,
        same (or backward-compatible) type, and where applicable, the same
        [MinVersion] attribute value.
    """

    def buildOrdinalFieldMap(union):
      fields_by_ordinal = {}
      for field in union.fields:
        if field.ordinal in fields_by_ordinal:
          raise Exception('Multiple fields with ordinal %s in union %s.' %
                          (field.ordinal, union.mojom_name))
        fields_by_ordinal[field.ordinal] = field
      return fields_by_ordinal

    new_fields = buildOrdinalFieldMap(new)
    old_fields = buildOrdinalFieldMap(old)
    if len(new_fields) < len(old_fields):
      # At least one field was removed, which is not OK.
      return False

    max_old_min_version = 0
    for ordinal, old_field in old_fields.items():
      new_field = new_fields.get(ordinal)
      if not new_field:
        # A field was removed, which is not OK.
        return False
      if not self._IsFieldBackwardCompatible(new_field, old_field):
        # An field changed its type or MinVersion, which is not OK.
        return False
      old_min_version = old_field.min_version or 0
      if old_min_version > max_old_min_version:
        max_old_min_version = old_min_version

    new_ordinals = set(new_fields.keys()) - set(old_fields.keys())
    for ordinal in new_ordinals:
      if (new_fields[ordinal].min_version or 0) <= max_old_min_version:
        # New fields must use a MinVersion greater than any old fields.
        return False

    return True

  @_CheckCompat.register(mojom.Array)
  def _(self, new: mojom.Array, old: mojom.Array):
    return new.length == old.length and self.IsBackwardCompatible(
        new.kind, old.kind)

  @_CheckCompat.register(mojom.Map)
  def _(self, new: mojom.Map, old: mojom.Map):
    return self.IsBackwardCompatible(
        new.key_kind, old.key_kind) and self.IsBackwardCompatible(
            new.value_kind, old.value_kind)

  @_CheckCompat.register(mojom.PendingRemote)
  def _(self, new: mojom.PendingRemote, old: mojom.PendingRemote):
    return self.IsBackwardCompatible(new.kind, old.kind)

  @_CheckCompat.register(mojom.PendingReceiver)
  def _(self, new: mojom.PendingReceiver, old: mojom.PendingReceiver):
    return self.IsBackwardCompatible(new.kind, old.kind)

  @_CheckCompat.register(mojom.PendingAssociatedRemote)
  def _(self, new: mojom.PendingAssociatedRemote,
        old: mojom.PendingAssociatedRemote):
    return self.IsBackwardCompatible(new.kind, old.kind)

  @_CheckCompat.register(mojom.PendingAssociatedReceiver)
  def _(self, new: mojom.PendingAssociatedReceiver,
        old: mojom.PendingAssociatedReceiver):
    return self.IsBackwardCompatible(new.kind, old.kind)

  @_CheckCompat.register(mojom.Interface)
  def _(self, new: mojom.Interface, old: mojom.Interface):
    """This interface is backward-compatible with old interface if and
    only if all of the following conditions hold:
      - All defined methods in the old interface (when identified by ordinal)
        have backward-compatible definitions in this interface. For each method
        this means:
          - The parameter list is backward-compatible, according to backward-
            compatibility rules for structs, where each parameter is essentially
            a struct field.
          - If the old method definition does not specify a reply message, the
            new method definition must not specify a reply message.
          - If the old method definition specifies a reply message, the new
            method definition must also specify a reply message with a parameter
            list that is backward-compatible according to backward-compatibility
            rules for structs.
      - All newly introduced methods in this interface have a [MinVersion]
        attribute specifying a version greater than any method in
        the old interface.
    """

    def buildOrdinalMethodMap(interface):
      methods_by_ordinal = {}
      for method in interface.methods:
        if method.ordinal in methods_by_ordinal:
          raise Exception('Multiple methods with ordinal %s in interface %s.' %
                          (method.ordinal, interface.mojom_name))
        methods_by_ordinal[method.ordinal] = method
      return methods_by_ordinal

    new_methods = buildOrdinalMethodMap(new)
    old_methods = buildOrdinalMethodMap(old)
    max_old_min_version = 0
    for ordinal, old_method in old_methods.items():
      new_method = new_methods.get(ordinal)
      if not new_method:
        # A method was removed, which is not OK.
        return False

      if not self.IsBackwardCompatible(new_method.param_struct,
                                       old_method.param_struct):
        # The parameter list is not backward-compatible, which is not OK.
        return False

      if old_method.response_param_struct is None:
        if new_method.response_param_struct is not None:
          # A reply was added to a message which didn't have one before, and
          # this is not OK.
          return False
      else:
        if new_method.response_param_struct is None:
          # A reply was removed from a message, which is not OK.
          return False
        if not self.IsBackwardCompatible(new_method.response_param_struct,
                                         old_method.response_param_struct):
          # The new message's reply is not backward-compatible with the old
          # message's reply, which is not OK.
          return False

      if (old_method.min_version or 0) > max_old_min_version:
        max_old_min_version = old_method.min_version

    # All the old methods are compatible with their new counterparts. Now verify
    # that newly added methods are properly versioned.
    new_ordinals = set(new_methods.keys()) - set(old_methods.keys())
    for ordinal in new_ordinals:
      new_method = new_methods[ordinal]
      if (new_method.min_version or 0) <= max_old_min_version:
        # A method was added to an existing version, which is not OK.
        return False

    return True

  @_CheckCompat.register(mojom.Enum)
  def _(self, new: mojom.Enum, old: mojom.Enum):
    """This enum is backward-compatible with old enum if and only if one
    of the following conditions holds:
        - Neither enum is [Extensible] and both have the exact same set of valid
          numeric values. Field names and aliases for the same numeric value do
          not affect compatibility.
        - old is [Extensible], and for every version defined by
          the old enum, this enum has the exact same set of valid numeric
          values.
    """

    def buildVersionFieldMap(enum):
      fields_by_min_version = {}
      for field in enum.fields:
        if field.min_version not in fields_by_min_version:
          fields_by_min_version[field.min_version] = set()
        fields_by_min_version[field.min_version].add(field.numeric_value)
      return fields_by_min_version

    old_fields = buildVersionFieldMap(old)
    new_fields = buildVersionFieldMap(new)

    if new_fields.keys() != old_fields.keys() and not old.extensible:
      raise Exception("Non-extensible enum cannot be modified")

    for min_version, valid_values in old_fields.items():
      if min_version not in new_fields:
        raise Exception('New values added to an extensible enum '
                        'did not specify MinVersion: %s' % new_fields)

      if (new_fields[min_version] != valid_values):
        if (len(new_fields[min_version]) < len(valid_values)):
          raise Exception('Removing values for an existing MinVersion %s '
                          'is not allowed' % min_version)

        raise Exception(
            'New values don\'t match old values '
            'for an existing MinVersion %s, '
            'please specify MinVersion equal to "Next version" '
            'in the enum description '
            'for the following values:\n%s' %
            (min_version, new_fields[min_version].difference(valid_values)))
    return True
