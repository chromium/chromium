# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This module's classes provide an interface to mojo modules. Modules are
# collections of interfaces and structs to be used by mojo ipc clients and
# servers.
#
# A simple interface would be created this way:
# module = mojom.generate.module.Module('Foo')
# interface = module.AddInterface('Bar')
# method = interface.AddMethod('Tat', 0)
# method.AddParameter('baz', 0, mojom.INT32)

import pickle
from collections import OrderedDict
from uuid import UUID

# pylint: disable=raise-missing-from


class BackwardCompatibilityChecker:
  """Used for memoization while recursively checking two type definitions for
  backward-compatibility."""

  def __init__(self):
    self._cache = {}

  def IsBackwardCompatible(self, new_kind, old_kind):
    key = (new_kind, old_kind)
    result = self._cache.get(key)
    if result is None:
      # Assume they're compatible at first to effectively ignore recursive
      # checks between these types, e.g. if both kinds are a struct or union
      # that references itself in a field.
      self._cache[key] = True
      result = new_kind.IsBackwardCompatible(old_kind, self)
      self._cache[key] = result
    return result


# We use our own version of __repr__ when displaying the AST, as the
# AST currently doesn't capture which nodes are reference (e.g. to
# types) and which nodes are definitions. This allows us to e.g. print
# the definition of a struct when it's defined inside a module, but
# only print its name when it's referenced in e.g. a method parameter.
def Repr(obj, as_ref=True):
  """A version of __repr__ that can distinguish references.

  Sometimes we like to print an object's full representation
  (e.g. with its fields) and sometimes we just want to reference an
  object that was printed in full elsewhere. This function allows us
  to make that distinction.

  Args:
    obj: The object whose string representation we compute.
    as_ref: If True, use the short reference representation.

  Returns:
    A str representation of |obj|.
  """
  if hasattr(obj, 'Repr'):
    return obj.Repr(as_ref=as_ref)
  # Since we cannot implement Repr for existing container types, we
  # handle them here.
  if isinstance(obj, list):
    if not obj:
      return '[]'
    return ('[\n%s\n]' %
            (',\n'.join('    %s' % Repr(elem, as_ref).replace('\n', '\n    ')
                        for elem in obj)))
  if isinstance(obj, dict):
    if not obj:
      return '{}'
    return ('{\n%s\n}' % (',\n'.join('    %s: %s' %
                                     (Repr(key, as_ref).replace('\n', '\n    '),
                                      Repr(val, as_ref).replace('\n', '\n    '))
                                     for key, val in obj.items())))
  return repr(obj)


def GenericRepr(obj, names):
  """Compute generic Repr for |obj| based on the attributes in |names|.

  Args:
    obj: The object to compute a Repr for.
    names: A dict from attribute names to include, to booleans
        specifying whether those attributes should be shown as
        references or not.

  Returns:
    A str representation of |obj|.
  """

  def ReprIndent(name, as_ref):
    return '    %s=%s' % (name, Repr(getattr(obj, name), as_ref).replace(
        '\n', '\n    '))

  return '%s(\n%s\n)' % (obj.__class__.__name__, ',\n'.join(
      ReprIndent(name, as_ref) for (name, as_ref) in names.items()))


class Kind:
  """Kind represents a type (e.g. int8, string).

  Attributes:
    spec: A string uniquely identifying the type. May be None.
    module: {Module} The defining module. Set to None for built-in types.
    parent_kind: The enclosing type. For example, an enum defined
        inside an interface has that interface as its parent. May be None.
  """

  def __init__(self, spec=None, module=None):
    self.spec = spec
    self.module = module
    self.parent_kind = None

  def Repr(self, as_ref=True):
    # pylint: disable=unused-argument
    return '<%s spec=%r>' % (self.__class__.__name__, self.spec)

  def __repr__(self):
    # Gives us a decent __repr__ for all kinds.
    return self.Repr()

  def __eq__(self, rhs):
    # pylint: disable=unidiomatic-typecheck
    return (type(self) == type(rhs)
            and (self.spec, self.parent_kind) == (rhs.spec, rhs.parent_kind))

  def __hash__(self):
    # TODO(crbug.com/1060471): Remove this and other __hash__ methods on Kind
    # and its subclasses. This is to support existing generator code which uses
    # some primitive Kinds as dict keys. The default hash (object identity)
    # breaks these dicts when a pickled Module instance is unpickled and used
    # during a subsequent run of the parser.
    return hash((self.spec, self.parent_kind))

  # pylint: disable=unused-argument
  def IsBackwardCompatible(self, rhs, checker):
    return self == rhs


class ReferenceKind(Kind):
  """ReferenceKind represents pointer and handle types.

  A type is nullable if null (for pointer types) or invalid handle (for handle
  types) is a legal value for the type.

  Attributes:
    is_nullable: True if the type is nullable.
  """

  def __init__(self, spec=None, is_nullable=False, module=None):
    assert spec is None or is_nullable == spec.startswith('?')
    Kind.__init__(self, spec, module)
    self.is_nullable = is_nullable
    self.shared_definition = {}

  def Repr(self, as_ref=True):
    return '<%s spec=%r is_nullable=%r>' % (self.__class__.__name__, self.spec,
                                            self.is_nullable)

  def MakeNullableKind(self):
    assert not self.is_nullable

    if self == STRING:
      return NULLABLE_STRING
    if self == HANDLE:
      return NULLABLE_HANDLE
    if self == DCPIPE:
      return NULLABLE_DCPIPE
    if self == DPPIPE:
      return NULLABLE_DPPIPE
    if self == MSGPIPE:
      return NULLABLE_MSGPIPE
    if self == SHAREDBUFFER:
      return NULLABLE_SHAREDBUFFER
    if self == PLATFORMHANDLE:
      return NULLABLE_PLATFORMHANDLE

    nullable_kind = type(self)()
    nullable_kind.shared_definition = self.shared_definition
    if self.spec is not None:
      nullable_kind.spec = '?' + self.spec
    nullable_kind.is_nullable = True
    nullable_kind.parent_kind = self.parent_kind
    nullable_kind.module = self.module

    return nullable_kind

  @classmethod
  def AddSharedProperty(cls, name):
    """Adds a property |name| to |cls|, which accesses the corresponding item in
       |shared_definition|.

       The reason of adding such indirection is to enable sharing definition
       between a reference kind and its nullable variation. For example:
         a = Struct('test_struct_1')
         b = a.MakeNullableKind()
         a.name = 'test_struct_2'
         print(b.name)  # Outputs 'test_struct_2'.
    """

    def Get(self):
      try:
        return self.shared_definition[name]
      except KeyError:  # Must raise AttributeError if property doesn't exist.
        raise AttributeError

    def Set(self, value):
      self.shared_definition[name] = value

    setattr(cls, name, property(Get, Set))

  def __eq__(self, rhs):
    return (isinstance(rhs, ReferenceKind) and super().__eq__(rhs)
            and self.is_nullable == rhs.is_nullable)

  def __hash__(self):
    return hash((super().__hash__(), self.is_nullable))

  def IsBackwardCompatible(self, rhs, checker):
    return (super().IsBackwardCompatible(rhs, checker)
            and self.is_nullable == rhs.is_nullable)


# Initialize the set of primitive types. These can be accessed by clients.
BOOL = Kind('b')
INT8 = Kind('i8')
INT16 = Kind('i16')
INT32 = Kind('i32')
INT64 = Kind('i64')
UINT8 = Kind('u8')
UINT16 = Kind('u16')
UINT32 = Kind('u32')
UINT64 = Kind('u64')
FLOAT = Kind('f')
DOUBLE = Kind('d')
STRING = ReferenceKind('s')
HANDLE = ReferenceKind('h')
DCPIPE = ReferenceKind('h:d:c')
DPPIPE = ReferenceKind('h:d:p')
MSGPIPE = ReferenceKind('h:m')
SHAREDBUFFER = ReferenceKind('h:s')
PLATFORMHANDLE = ReferenceKind('h:p')
NULLABLE_STRING = ReferenceKind('?s', True)
NULLABLE_HANDLE = ReferenceKind('?h', True)
NULLABLE_DCPIPE = ReferenceKind('?h:d:c', True)
NULLABLE_DPPIPE = ReferenceKind('?h:d:p', True)
NULLABLE_MSGPIPE = ReferenceKind('?h:m', True)
NULLABLE_SHAREDBUFFER = ReferenceKind('?h:s', True)
NULLABLE_PLATFORMHANDLE = ReferenceKind('?h:p', True)

# Collection of all Primitive types
PRIMITIVES = (
    BOOL,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT,
    DOUBLE,
    STRING,
    HANDLE,
    DCPIPE,
    DPPIPE,
    MSGPIPE,
    SHAREDBUFFER,
    PLATFORMHANDLE,
    NULLABLE_STRING,
    NULLABLE_HANDLE,
    NULLABLE_DCPIPE,
    NULLABLE_DPPIPE,
    NULLABLE_MSGPIPE,
    NULLABLE_SHAREDBUFFER,
    NULLABLE_PLATFORMHANDLE,
)

ATTRIBUTE_MIN_VERSION = 'MinVersion'
ATTRIBUTE_DEFAULT = 'Default'
ATTRIBUTE_EXTENSIBLE = 'Extensible'
ATTRIBUTE_NO_INTERRUPT = 'NoInterrupt'
ATTRIBUTE_STABLE = 'Stable'
ATTRIBUTE_SYNC = 'Sync'
ATTRIBUTE_UNLIMITED_SIZE = 'UnlimitedSize'
ATTRIBUTE_UUID = 'Uuid'
ATTRIBUTE_SERVICE_SANDBOX = 'ServiceSandbox'
ATTRIBUTE_REQUIRE_CONTEXT = 'RequireContext'
ATTRIBUTE_ALLOWED_CONTEXT = 'AllowedContext'


class NamedValue:
  def __init__(self, module, parent_kind, mojom_name):
    self.module = module
    self.parent_kind = parent_kind
    self.mojom_name = mojom_name

  def GetSpec(self):
    return (self.module.GetNamespacePrefix() +
            (self.parent_kind and
             (self.parent_kind.mojom_name + '.') or "") + self.mojom_name)

  def __eq__(self, rhs):
    return (isinstance(rhs, NamedValue)
            and (self.parent_kind, self.mojom_name) == (rhs.parent_kind,
                                                        rhs.mojom_name))

  def __hash__(self):
    return hash((self.parent_kind, self.mojom_name))


class BuiltinValue:
  def __init__(self, value):
    self.value = value

  def __eq__(self, rhs):
    return isinstance(rhs, BuiltinValue) and self.value == rhs.value


class ConstantValue(NamedValue):
  def __init__(self, module, parent_kind, constant):
    NamedValue.__init__(self, module, parent_kind, constant.mojom_name)
    self.constant = constant

  @property
  def name(self):
    return self.constant.name


class EnumValue(NamedValue):
  def __init__(self, module, enum, field):
    NamedValue.__init__(self, module, enum.parent_kind, field.mojom_name)
    self.field = field
    self.enum = enum

  def GetSpec(self):
    return (self.module.GetNamespacePrefix() +
            (self.parent_kind and (self.parent_kind.mojom_name + '.') or "") +
            self.enum.mojom_name + '.' + self.mojom_name)

  @property
  def name(self):
    return self.field.name


class Constant:
  def __init__(self, mojom_name=None, kind=None, value=None, parent_kind=None):
    self.mojom_name = mojom_name
    self.name = None
    self.kind = kind
    self.value = value
    self.parent_kind = parent_kind

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeConstant(self.mojom_name)

  def __eq__(self, rhs):
    return (isinstance(rhs, Constant)
            and (self.mojom_name, self.kind, self.value,
                 self.parent_kind) == (rhs.mojom_name, rhs.kind, rhs.value,
                                       rhs.parent_kind))


class Field:
  def __init__(self,
               mojom_name=None,
               kind=None,
               ordinal=None,
               default=None,
               attributes=None):
    if self.__class__.__name__ == 'Field':
      raise Exception()
    self.mojom_name = mojom_name
    self.name = None
    self.kind = kind
    self.ordinal = ordinal
    self.default = default
    self.attributes = attributes

  def Repr(self, as_ref=True):
    # pylint: disable=unused-argument
    # Fields are only referenced by objects which define them and thus
    # they are always displayed as non-references.
    return GenericRepr(self, {'mojom_name': False, 'kind': True})

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeField(self.mojom_name)

  @property
  def min_version(self):
    return self.attributes.get(ATTRIBUTE_MIN_VERSION) \
        if self.attributes else None

  def __eq__(self, rhs):
    return (isinstance(rhs, Field)
            and (self.mojom_name, self.kind, self.ordinal, self.default,
                 self.attributes) == (rhs.mojom_name, rhs.kind, rhs.ordinal,
                                      rhs.default, rhs.attributes))

  def __hash__(self):
    return hash((self.mojom_name, self.kind, self.ordinal, self.default))


class StructField(Field):
  def __hash__(self):
    return super(Field, self).__hash__()


class UnionField(Field):
  def __init__(self,
               mojom_name=None,
               kind=None,
               ordinal=None,
               default=None,
               attributes=None):
    Field.__init__(self, mojom_name, kind, ordinal, default, attributes)

  @property
  def is_default(self):
    return self.attributes.get(ATTRIBUTE_DEFAULT, False) \
        if self.attributes else False


def _IsFieldBackwardCompatible(new_field, old_field, checker):
  if (new_field.min_version or 0) != (old_field.min_version or 0):
    return False

  return checker.IsBackwardCompatible(new_field.kind, old_field.kind)


class Struct(ReferenceKind):
  """A struct with typed fields.

  Attributes:
    mojom_name: {str} The name of the struct type as defined in mojom.
    name: {str} The stylized name.
    native_only: {bool} Does the struct have a body (i.e. any fields) or is it
        purely a native struct.
    custom_serializer: {bool} Should we generate a serializer for the struct or
        will one be provided by non-generated code.
    fields: {List[StructField]} The members of the struct.
    enums: {List[Enum]} The enums defined in the struct scope.
    constants: {List[Constant]} The constants defined in the struct scope.
    attributes: {dict} Additional information about the struct, such as
        if it's a native struct.
  """

  ReferenceKind.AddSharedProperty('mojom_name')
  ReferenceKind.AddSharedProperty('name')
  ReferenceKind.AddSharedProperty('native_only')
  ReferenceKind.AddSharedProperty('custom_serializer')
  ReferenceKind.AddSharedProperty('fields')
  ReferenceKind.AddSharedProperty('enums')
  ReferenceKind.AddSharedProperty('constants')
  ReferenceKind.AddSharedProperty('attributes')

  def __init__(self, mojom_name=None, module=None, attributes=None):
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    ReferenceKind.__init__(self, spec, False, module)
    self.mojom_name = mojom_name
    self.name = None
    self.native_only = False
    self.custom_serializer = False
    self.fields = []
    self.enums = []
    self.constants = []
    self.attributes = attributes

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s mojom_name=%r module=%s>' % (self.__class__.__name__,
                                               self.mojom_name,
                                               Repr(self.module, as_ref=True))
    return GenericRepr(self, {
        'mojom_name': False,
        'fields': False,
        'module': True
    })

  def AddField(self,
               mojom_name,
               kind,
               ordinal=None,
               default=None,
               attributes=None):
    field = StructField(mojom_name, kind, ordinal, default, attributes)
    self.fields.append(field)
    return field

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeStruct(self.mojom_name)
    for field in self.fields:
      field.Stylize(stylizer)
    for enum in self.enums:
      enum.Stylize(stylizer)
    for constant in self.constants:
      constant.Stylize(stylizer)

  def IsBackwardCompatible(self, rhs, checker):
    """This struct is backward-compatible with rhs (older_struct) if and only if
    all of the following conditions hold:
      - Any newly added field is tagged with a [MinVersion] attribute specifying
        a version number greater than all previously used [MinVersion]
        attributes within the struct.
      - All fields present in rhs remain present in the new struct,
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

    new_fields = buildOrdinalFieldMap(self)
    old_fields = buildOrdinalFieldMap(rhs)
    if len(new_fields) < len(old_fields):
      # At least one field was removed, which is not OK.
      return False

    # If there are N fields, existing ordinal values must exactly cover the
    # range from 0 to N-1.
    num_old_ordinals = len(old_fields)
    max_old_min_version = 0
    for ordinal in range(num_old_ordinals):
      new_field = new_fields[ordinal]
      old_field = old_fields[ordinal]
      if (old_field.min_version or 0) > max_old_min_version:
        max_old_min_version = old_field.min_version
      if not _IsFieldBackwardCompatible(new_field, old_field, checker):
        # Type or min-version mismatch between old and new versions of the same
        # ordinal field.
        return False

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
        return False
      if min_version < last_min_version:
        # The [MinVersion] of a field cannot be lower than the [MinVersion] of
        # a field with lower ordinal value.
        return False
      if IsReferenceKind(new_field.kind) and not IsNullableKind(new_field.kind):
        # New fields whose type can be nullable MUST be nullable.
        return False

    return True

  @property
  def stable(self):
    return self.attributes.get(ATTRIBUTE_STABLE, False) \
        if self.attributes else False

  @property
  def qualified_name(self):
    if self.parent_kind:
      prefix = self.parent_kind.qualified_name + '.'
    else:
      prefix = self.module.GetNamespacePrefix()
    return '%s%s' % (prefix, self.mojom_name)

  def _tuple(self):
    return (self.mojom_name, self.native_only, self.fields, self.constants,
            self.attributes)

  def __eq__(self, rhs):
    return isinstance(rhs, Struct) and self._tuple() == rhs._tuple()

  def __lt__(self, rhs):
    if not isinstance(self, type(rhs)):
      return str(type(self)) < str(type(rhs))

    return self._tuple() < rhs._tuple()

  def __hash__(self):
    return id(self)


class Union(ReferenceKind):
  """A union of several kinds.

  Attributes:
    mojom_name: {str} The name of the union type as defined in mojom.
    name: {str} The stylized name.
    fields: {List[UnionField]} The members of the union.
    attributes: {dict} Additional information about the union, such as
        which Java class name to use to represent it in the generated
        bindings.
  """
  ReferenceKind.AddSharedProperty('mojom_name')
  ReferenceKind.AddSharedProperty('name')
  ReferenceKind.AddSharedProperty('fields')
  ReferenceKind.AddSharedProperty('attributes')
  ReferenceKind.AddSharedProperty('default_field')

  def __init__(self, mojom_name=None, module=None, attributes=None):
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    ReferenceKind.__init__(self, spec, False, module)
    self.mojom_name = mojom_name
    self.name = None
    self.fields = []
    self.attributes = attributes
    self.default_field = None

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s spec=%r is_nullable=%r fields=%s>' % (
          self.__class__.__name__, self.spec, self.is_nullable, Repr(
              self.fields))
    return GenericRepr(self, {'fields': True, 'is_nullable': False})

  def AddField(self, mojom_name, kind, ordinal=None, attributes=None):
    field = UnionField(mojom_name, kind, ordinal, None, attributes)
    self.fields.append(field)
    return field

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeUnion(self.mojom_name)
    for field in self.fields:
      field.Stylize(stylizer)

  def IsBackwardCompatible(self, rhs, checker):
    """This union is backward-compatible with rhs (older_union) if and only if
    all of the following conditions hold:
      - Any newly added field is tagged with a [MinVersion] attribute specifying
        a version number greater than all previously used [MinVersion]
        attributes within the union.
      - All fields present in rhs remain present in the new union,
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

    new_fields = buildOrdinalFieldMap(self)
    old_fields = buildOrdinalFieldMap(rhs)
    if len(new_fields) < len(old_fields):
      # At least one field was removed, which is not OK.
      return False

    max_old_min_version = 0
    for ordinal, old_field in old_fields.items():
      new_field = new_fields.get(ordinal)
      if not new_field:
        # A field was removed, which is not OK.
        return False
      if not _IsFieldBackwardCompatible(new_field, old_field, checker):
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

  @property
  def extensible(self):
    return self.attributes.get(ATTRIBUTE_EXTENSIBLE, False) \
        if self.attributes else False

  @property
  def stable(self):
    return self.attributes.get(ATTRIBUTE_STABLE, False) \
        if self.attributes else False

  @property
  def qualified_name(self):
    if self.parent_kind:
      prefix = self.parent_kind.qualified_name + '.'
    else:
      prefix = self.module.GetNamespacePrefix()
    return '%s%s' % (prefix, self.mojom_name)

  def _tuple(self):
    return (self.mojom_name, self.fields, self.attributes)

  def __eq__(self, rhs):
    return isinstance(rhs, Union) and self._tuple() == rhs._tuple()

  def __lt__(self, rhs):
    if not isinstance(self, type(rhs)):
      return str(type(self)) < str(type(rhs))

    return self._tuple() < rhs._tuple()

  def __hash__(self):
    return id(self)


class Array(ReferenceKind):
  """An array.

  Attributes:
    kind: {Kind} The type of the elements. May be None.
    length: The number of elements. None if unknown.
  """

  ReferenceKind.AddSharedProperty('kind')
  ReferenceKind.AddSharedProperty('length')

  def __init__(self, kind=None, length=None):
    if kind is not None:
      if length is not None:
        spec = 'a%d:%s' % (length, kind.spec)
      else:
        spec = 'a:%s' % kind.spec

      ReferenceKind.__init__(self, spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind
    self.length = length

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s spec=%r is_nullable=%r kind=%s length=%r>' % (
          self.__class__.__name__, self.spec, self.is_nullable, Repr(
              self.kind), self.length)
    return GenericRepr(self, {
        'kind': True,
        'length': False,
        'is_nullable': False
    })

  def __eq__(self, rhs):
    return (isinstance(rhs, Array)
            and (self.kind, self.length) == (rhs.kind, rhs.length))

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return (isinstance(rhs, Array) and self.length == rhs.length
            and checker.IsBackwardCompatible(self.kind, rhs.kind))


class Map(ReferenceKind):
  """A map.

  Attributes:
    key_kind: {Kind} The type of the keys. May be None.
    value_kind: {Kind} The type of the elements. May be None.
  """
  ReferenceKind.AddSharedProperty('key_kind')
  ReferenceKind.AddSharedProperty('value_kind')

  def __init__(self, key_kind=None, value_kind=None):
    if (key_kind is not None and value_kind is not None):
      ReferenceKind.__init__(
          self, 'm[' + key_kind.spec + '][' + value_kind.spec + ']')
      if IsNullableKind(key_kind):
        raise Exception("Nullable kinds cannot be keys in maps.")
      if IsAnyHandleKind(key_kind):
        raise Exception("Handles cannot be keys in maps.")
      if IsAnyInterfaceKind(key_kind):
        raise Exception("Interfaces cannot be keys in maps.")
      if IsArrayKind(key_kind):
        raise Exception("Arrays cannot be keys in maps.")
    else:
      ReferenceKind.__init__(self)

    self.key_kind = key_kind
    self.value_kind = value_kind

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s spec=%r is_nullable=%r key_kind=%s value_kind=%s>' % (
          self.__class__.__name__, self.spec, self.is_nullable,
          Repr(self.key_kind), Repr(self.value_kind))
    return GenericRepr(self, {'key_kind': True, 'value_kind': True})

  def __eq__(self, rhs):
    return (isinstance(rhs, Map) and
            (self.key_kind, self.value_kind) == (rhs.key_kind, rhs.value_kind))

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return (isinstance(rhs, Map)
            and checker.IsBackwardCompatible(self.key_kind, rhs.key_kind)
            and checker.IsBackwardCompatible(self.value_kind, rhs.value_kind))


class PendingRemote(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            'pending_remote<T> requires T to be an interface type. Got %r' %
            kind.spec)
      ReferenceKind.__init__(self, 'rmt:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, PendingRemote) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return (isinstance(rhs, PendingRemote)
            and checker.IsBackwardCompatible(self.kind, rhs.kind))


class PendingReceiver(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            'pending_receiver<T> requires T to be an interface type. Got %r' %
            kind.spec)
      ReferenceKind.__init__(self, 'rcv:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, PendingReceiver) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(rhs, PendingReceiver) and checker.IsBackwardCompatible(
        self.kind, rhs.kind)


class PendingAssociatedRemote(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            'pending_associated_remote<T> requires T to be an interface ' +
            'type. Got %r' % kind.spec)
      ReferenceKind.__init__(self, 'rma:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, PendingAssociatedRemote) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(rhs,
                      PendingAssociatedRemote) and checker.IsBackwardCompatible(
                          self.kind, rhs.kind)


class PendingAssociatedReceiver(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            'pending_associated_receiver<T> requires T to be an interface' +
            'type. Got %r' % kind.spec)
      ReferenceKind.__init__(self, 'rca:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, PendingAssociatedReceiver) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(
        rhs, PendingAssociatedReceiver) and checker.IsBackwardCompatible(
            self.kind, rhs.kind)


class InterfaceRequest(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            "Interface request requires %r to be an interface." % kind.spec)
      ReferenceKind.__init__(self, 'r:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, InterfaceRequest) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(rhs, InterfaceRequest) and checker.IsBackwardCompatible(
        self.kind, rhs.kind)


class AssociatedInterfaceRequest(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, InterfaceRequest):
        raise Exception(
            "Associated interface request requires %r to be an interface "
            "request." % kind.spec)
      assert not kind.is_nullable
      ReferenceKind.__init__(self, 'asso:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind.kind if kind is not None else None

  def __eq__(self, rhs):
    return isinstance(rhs, AssociatedInterfaceRequest) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(
        rhs, AssociatedInterfaceRequest) and checker.IsBackwardCompatible(
            self.kind, rhs.kind)


class Parameter:
  def __init__(self,
               mojom_name=None,
               kind=None,
               ordinal=None,
               default=None,
               attributes=None):
    self.mojom_name = mojom_name
    self.name = None
    self.ordinal = ordinal
    self.kind = kind
    self.default = default
    self.attributes = attributes

  def Repr(self, as_ref=True):
    # pylint: disable=unused-argument
    return '<%s mojom_name=%r kind=%s>' % (
        self.__class__.__name__, self.mojom_name, self.kind.Repr(as_ref=True))

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeParameter(self.mojom_name)

  @property
  def min_version(self):
    return self.attributes.get(ATTRIBUTE_MIN_VERSION) \
        if self.attributes else None

  def __eq__(self, rhs):
    return (isinstance(rhs, Parameter)
            and (self.mojom_name, self.ordinal, self.kind, self.default,
                 self.attributes) == (rhs.mojom_name, rhs.ordinal, rhs.kind,
                                      rhs.default, rhs.attributes))


class Method:
  def __init__(self, interface, mojom_name, ordinal=None, attributes=None):
    self.interface = interface
    self.mojom_name = mojom_name
    self.name = None
    self.explicit_ordinal = ordinal
    self.ordinal = ordinal
    self.parameters = []
    self.param_struct = None
    self.response_parameters = None
    self.response_param_struct = None
    self.attributes = attributes

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s mojom_name=%r>' % (self.__class__.__name__, self.mojom_name)
    return GenericRepr(self, {
        'mojom_name': False,
        'parameters': True,
        'response_parameters': True
    })

  def AddParameter(self,
                   mojom_name,
                   kind,
                   ordinal=None,
                   default=None,
                   attributes=None):
    parameter = Parameter(mojom_name, kind, ordinal, default, attributes)
    self.parameters.append(parameter)
    return parameter

  def AddResponseParameter(self,
                           mojom_name,
                           kind,
                           ordinal=None,
                           default=None,
                           attributes=None):
    if self.response_parameters == None:
      self.response_parameters = []
    parameter = Parameter(mojom_name, kind, ordinal, default, attributes)
    self.response_parameters.append(parameter)
    return parameter

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeMethod(self.mojom_name)
    for param in self.parameters:
      param.Stylize(stylizer)
    if self.response_parameters is not None:
      for param in self.response_parameters:
        param.Stylize(stylizer)

    if self.param_struct:
      self.param_struct.Stylize(stylizer)
    if self.response_param_struct:
      self.response_param_struct.Stylize(stylizer)

  @property
  def min_version(self):
    return self.attributes.get(ATTRIBUTE_MIN_VERSION) \
        if self.attributes else None

  @property
  def sync(self):
    return self.attributes.get(ATTRIBUTE_SYNC) \
        if self.attributes else None

  @property
  def allow_interrupt(self):
    return not self.attributes.get(ATTRIBUTE_NO_INTERRUPT) \
        if self.attributes else True

  @property
  def unlimited_message_size(self):
    return self.attributes.get(ATTRIBUTE_UNLIMITED_SIZE) \
        if self.attributes else False

  @property
  def allowed_context(self):
    return self.attributes.get(ATTRIBUTE_ALLOWED_CONTEXT) \
        if self.attributes else None

  def _tuple(self):
    return (self.mojom_name, self.ordinal, self.parameters,
            self.response_parameters, self.attributes)

  def __eq__(self, rhs):
    return isinstance(rhs, Method) and self._tuple() == rhs._tuple()

  def __lt__(self, rhs):
    if not isinstance(self, type(rhs)):
      return str(type(self)) < str(type(rhs))

    return self._tuple() < rhs._tuple()


class Interface(ReferenceKind):
  ReferenceKind.AddSharedProperty('mojom_name')
  ReferenceKind.AddSharedProperty('name')
  ReferenceKind.AddSharedProperty('methods')
  ReferenceKind.AddSharedProperty('enums')
  ReferenceKind.AddSharedProperty('constants')
  ReferenceKind.AddSharedProperty('attributes')

  def __init__(self, mojom_name=None, module=None, attributes=None):
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    ReferenceKind.__init__(self, spec, False, module)
    self.mojom_name = mojom_name
    self.name = None
    self.methods = []
    self.enums = []
    self.constants = []
    self.attributes = attributes

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s mojom_name=%r>' % (self.__class__.__name__, self.mojom_name)
    return GenericRepr(self, {
        'mojom_name': False,
        'attributes': False,
        'methods': False
    })

  def AddMethod(self, mojom_name, ordinal=None, attributes=None):
    method = Method(self, mojom_name, ordinal, attributes)
    self.methods.append(method)
    return method

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeInterface(self.mojom_name)
    for method in self.methods:
      method.Stylize(stylizer)
    for enum in self.enums:
      enum.Stylize(stylizer)
    for constant in self.constants:
      constant.Stylize(stylizer)

  def IsBackwardCompatible(self, rhs, checker):
    """This interface is backward-compatible with rhs (older_interface) if and
    only if all of the following conditions hold:
      - All defined methods in rhs (when identified by ordinal) have
        backward-compatible definitions in this interface. For each method this
        means:
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
        rhs.
    """

    def buildOrdinalMethodMap(interface):
      methods_by_ordinal = {}
      for method in interface.methods:
        if method.ordinal in methods_by_ordinal:
          raise Exception('Multiple methods with ordinal %s in interface %s.' %
                          (method.ordinal, interface.mojom_name))
        methods_by_ordinal[method.ordinal] = method
      return methods_by_ordinal

    new_methods = buildOrdinalMethodMap(self)
    old_methods = buildOrdinalMethodMap(rhs)
    max_old_min_version = 0
    for ordinal, old_method in old_methods.items():
      new_method = new_methods.get(ordinal)
      if not new_method:
        # A method was removed, which is not OK.
        return False

      if not checker.IsBackwardCompatible(new_method.param_struct,
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
        if not checker.IsBackwardCompatible(new_method.response_param_struct,
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

  @property
  def service_sandbox(self):
    if not self.attributes:
      return None
    service_sandbox = self.attributes.get(ATTRIBUTE_SERVICE_SANDBOX, None)
    if service_sandbox is None:
      return None
    # Constants are only allowed to refer to an enum here, so replace.
    if isinstance(service_sandbox, Constant):
      service_sandbox = service_sandbox.value
    if not isinstance(service_sandbox, EnumValue):
      raise Exception("ServiceSandbox attribute on %s must be an enum value." %
                      self.module.name)
    return service_sandbox

  @property
  def require_context(self):
    if not self.attributes:
      return None
    return self.attributes.get(ATTRIBUTE_REQUIRE_CONTEXT, None)

  @property
  def stable(self):
    return self.attributes.get(ATTRIBUTE_STABLE, False) \
        if self.attributes else False

  @property
  def qualified_name(self):
    if self.parent_kind:
      prefix = self.parent_kind.qualified_name + '.'
    else:
      prefix = self.module.GetNamespacePrefix()
    return '%s%s' % (prefix, self.mojom_name)

  def _tuple(self):
    return (self.mojom_name, self.methods, self.enums, self.constants,
            self.attributes)

  def __eq__(self, rhs):
    return isinstance(rhs, Interface) and self._tuple() == rhs._tuple()

  def __lt__(self, rhs):
    if not isinstance(self, type(rhs)):
      return str(type(self)) < str(type(rhs))

    return self._tuple() < rhs._tuple()

  @property
  def uuid(self):
    uuid_str = self.attributes.get(ATTRIBUTE_UUID) if self.attributes else None
    if uuid_str is None:
      return None

    try:
      u = UUID(uuid_str)
    except:
      raise ValueError('Invalid format for Uuid attribute on interface {}. '
                       'Expected standard RFC 4122 string representation of '
                       'a UUID.'.format(self.mojom_name))
    return (int(u.hex[:16], 16), int(u.hex[16:], 16))

  def __hash__(self):
    return id(self)


class AssociatedInterface(ReferenceKind):
  ReferenceKind.AddSharedProperty('kind')

  def __init__(self, kind=None):
    if kind is not None:
      if not isinstance(kind, Interface):
        raise Exception(
            "Associated interface requires %r to be an interface." % kind.spec)
      assert not kind.is_nullable
      ReferenceKind.__init__(self, 'asso:' + kind.spec)
    else:
      ReferenceKind.__init__(self)
    self.kind = kind

  def __eq__(self, rhs):
    return isinstance(rhs, AssociatedInterface) and self.kind == rhs.kind

  def __hash__(self):
    return id(self)

  def IsBackwardCompatible(self, rhs, checker):
    return isinstance(rhs,
                      AssociatedInterface) and checker.IsBackwardCompatible(
                          self.kind, rhs.kind)


class EnumField:
  def __init__(self,
               mojom_name=None,
               value=None,
               attributes=None,
               numeric_value=None):
    self.mojom_name = mojom_name
    self.name = None
    self.value = value
    self.attributes = attributes
    self.numeric_value = numeric_value

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeEnumField(self.mojom_name)

  @property
  def default(self):
    return self.attributes.get(ATTRIBUTE_DEFAULT, False) \
        if self.attributes else False

  @property
  def min_version(self):
    return self.attributes.get(ATTRIBUTE_MIN_VERSION) \
        if self.attributes else None

  def __eq__(self, rhs):
    return (isinstance(rhs, EnumField)
            and (self.mojom_name, self.value, self.attributes,
                 self.numeric_value) == (rhs.mojom_name, rhs.value,
                                         rhs.attributes, rhs.numeric_value))


class Enum(Kind):
  def __init__(self, mojom_name=None, module=None, attributes=None):
    self.mojom_name = mojom_name
    self.name = None
    self.native_only = False
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    Kind.__init__(self, spec, module)
    self.fields = []
    self.attributes = attributes
    self.min_value = None
    self.max_value = None
    self.default_field = None

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s mojom_name=%r>' % (self.__class__.__name__, self.mojom_name)
    return GenericRepr(self, {'mojom_name': False, 'fields': False})

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeEnum(self.mojom_name)
    for field in self.fields:
      field.Stylize(stylizer)

  @property
  def extensible(self):
    return self.attributes.get(ATTRIBUTE_EXTENSIBLE, False) \
        if self.attributes else False

  @property
  def stable(self):
    return self.attributes.get(ATTRIBUTE_STABLE, False) \
        if self.attributes else False

  @property
  def qualified_name(self):
    if self.parent_kind:
      prefix = self.parent_kind.qualified_name + '.'
    else:
      prefix = self.module.GetNamespacePrefix()
    return '%s%s' % (prefix, self.mojom_name)

  # pylint: disable=unused-argument
  def IsBackwardCompatible(self, rhs, checker):
    """This enum is backward-compatible with rhs (older_enum) if and only if one
    of the following conditions holds:
        - Neither enum is [Extensible] and both have the exact same set of valid
          numeric values. Field names and aliases for the same numeric value do
          not affect compatibility.
        - rhs is [Extensible], and for every version defined by
          rhs, this enum has the exact same set of valid numeric values.
    """

    def buildVersionFieldMap(enum):
      fields_by_min_version = {}
      for field in enum.fields:
        if field.min_version not in fields_by_min_version:
          fields_by_min_version[field.min_version] = set()
        fields_by_min_version[field.min_version].add(field.numeric_value)
      return fields_by_min_version

    old_fields = buildVersionFieldMap(rhs)
    new_fields = buildVersionFieldMap(self)

    if new_fields.keys() != old_fields.keys() and not rhs.extensible:
      return False

    for min_version, valid_values in old_fields.items():
      if (min_version not in new_fields
          or new_fields[min_version] != valid_values):
        return False

    return True

  def _tuple(self):
    return (self.mojom_name, self.native_only, self.fields, self.attributes,
            self.min_value, self.max_value, self.default_field)

  def __eq__(self, rhs):
    return isinstance(rhs, Enum) and self._tuple() == rhs._tuple()

  def __lt__(self, rhs):
    if not isinstance(self, type(rhs)):
      return str(type(self)) < str(type(rhs))

    return self._tuple() < rhs._tuple()

  def __hash__(self):
    return id(self)


class Module:
  def __init__(self, path=None, mojom_namespace=None, attributes=None):
    self.path = path
    self.mojom_namespace = mojom_namespace
    self.namespace = None
    self.structs = []
    self.unions = []
    self.interfaces = []
    self.enums = []
    self.constants = []
    self.kinds = OrderedDict()
    self.attributes = attributes
    self.imports = []
    self.imported_kinds = OrderedDict()
    self.metadata = OrderedDict()

  def __repr__(self):
    # Gives us a decent __repr__ for modules.
    return self.Repr()

  def __eq__(self, rhs):
    return (isinstance(rhs, Module) and
            (self.path, self.attributes, self.mojom_namespace, self.imports,
             self.constants, self.enums, self.structs, self.unions,
             self.interfaces) == (rhs.path, rhs.attributes, rhs.mojom_namespace,
                                  rhs.imports, rhs.constants, rhs.enums,
                                  rhs.structs, rhs.unions, rhs.interfaces))

  def __hash__(self):
    return id(self)

  def Repr(self, as_ref=True):
    if as_ref:
      return '<%s path=%r mojom_namespace=%r>' % (
          self.__class__.__name__, self.path, self.mojom_namespace)
    return GenericRepr(
        self, {
            'path': False,
            'mojom_namespace': False,
            'attributes': False,
            'structs': False,
            'interfaces': False,
            'unions': False
        })

  def GetNamespacePrefix(self):
    return '%s.' % self.mojom_namespace if self.mojom_namespace else ''

  def AddInterface(self, mojom_name, attributes=None):
    interface = Interface(mojom_name, self, attributes)
    self.interfaces.append(interface)
    return interface

  def AddStruct(self, mojom_name, attributes=None):
    struct = Struct(mojom_name, self, attributes)
    self.structs.append(struct)
    return struct

  def AddUnion(self, mojom_name, attributes=None):
    union = Union(mojom_name, self, attributes)
    self.unions.append(union)
    return union

  def Stylize(self, stylizer):
    self.namespace = stylizer.StylizeModule(self.mojom_namespace)
    for struct in self.structs:
      struct.Stylize(stylizer)
    for union in self.unions:
      union.Stylize(stylizer)
    for interface in self.interfaces:
      interface.Stylize(stylizer)
    for enum in self.enums:
      enum.Stylize(stylizer)
    for constant in self.constants:
      constant.Stylize(stylizer)

    for imported_module in self.imports:
      imported_module.Stylize(stylizer)

  def Dump(self, f):
    pickle.dump(self, f)

  @classmethod
  def Load(cls, f):
    result = pickle.load(f)
    assert isinstance(result, Module)
    return result


def IsBoolKind(kind):
  return kind.spec == BOOL.spec


def IsFloatKind(kind):
  return kind.spec == FLOAT.spec


def IsDoubleKind(kind):
  return kind.spec == DOUBLE.spec


def IsIntegralKind(kind):
  return (kind.spec == BOOL.spec or kind.spec == INT8.spec
          or kind.spec == INT16.spec or kind.spec == INT32.spec
          or kind.spec == INT64.spec or kind.spec == UINT8.spec
          or kind.spec == UINT16.spec or kind.spec == UINT32.spec
          or kind.spec == UINT64.spec)


def IsStringKind(kind):
  return kind.spec == STRING.spec or kind.spec == NULLABLE_STRING.spec


def IsGenericHandleKind(kind):
  return kind.spec == HANDLE.spec or kind.spec == NULLABLE_HANDLE.spec


def IsDataPipeConsumerKind(kind):
  return kind.spec == DCPIPE.spec or kind.spec == NULLABLE_DCPIPE.spec


def IsDataPipeProducerKind(kind):
  return kind.spec == DPPIPE.spec or kind.spec == NULLABLE_DPPIPE.spec


def IsMessagePipeKind(kind):
  return kind.spec == MSGPIPE.spec or kind.spec == NULLABLE_MSGPIPE.spec


def IsSharedBufferKind(kind):
  return (kind.spec == SHAREDBUFFER.spec
          or kind.spec == NULLABLE_SHAREDBUFFER.spec)


def IsPlatformHandleKind(kind):
  return (kind.spec == PLATFORMHANDLE.spec
          or kind.spec == NULLABLE_PLATFORMHANDLE.spec)


def IsStructKind(kind):
  return isinstance(kind, Struct)


def IsUnionKind(kind):
  return isinstance(kind, Union)


def IsArrayKind(kind):
  return isinstance(kind, Array)


def IsInterfaceKind(kind):
  return isinstance(kind, Interface)


def IsAssociatedInterfaceKind(kind):
  return isinstance(kind, AssociatedInterface)


def IsInterfaceRequestKind(kind):
  return isinstance(kind, InterfaceRequest)


def IsAssociatedInterfaceRequestKind(kind):
  return isinstance(kind, AssociatedInterfaceRequest)


def IsPendingRemoteKind(kind):
  return isinstance(kind, PendingRemote)


def IsPendingReceiverKind(kind):
  return isinstance(kind, PendingReceiver)


def IsPendingAssociatedRemoteKind(kind):
  return isinstance(kind, PendingAssociatedRemote)


def IsPendingAssociatedReceiverKind(kind):
  return isinstance(kind, PendingAssociatedReceiver)


def IsEnumKind(kind):
  return isinstance(kind, Enum)


def IsReferenceKind(kind):
  return isinstance(kind, ReferenceKind)


def IsNullableKind(kind):
  return IsReferenceKind(kind) and kind.is_nullable


def IsMapKind(kind):
  return isinstance(kind, Map)


def IsObjectKind(kind):
  return IsPointerKind(kind) or IsUnionKind(kind)


def IsPointerKind(kind):
  return (IsStructKind(kind) or IsArrayKind(kind) or IsStringKind(kind)
          or IsMapKind(kind))


# Please note that it doesn't include any interface kind.
def IsAnyHandleKind(kind):
  return (IsGenericHandleKind(kind) or IsDataPipeConsumerKind(kind)
          or IsDataPipeProducerKind(kind) or IsMessagePipeKind(kind)
          or IsSharedBufferKind(kind) or IsPlatformHandleKind(kind))


def IsAnyInterfaceKind(kind):
  return (IsInterfaceKind(kind) or IsInterfaceRequestKind(kind)
          or IsAssociatedKind(kind) or IsPendingRemoteKind(kind)
          or IsPendingReceiverKind(kind))


def IsAnyHandleOrInterfaceKind(kind):
  return IsAnyHandleKind(kind) or IsAnyInterfaceKind(kind)


def IsAssociatedKind(kind):
  return (IsAssociatedInterfaceKind(kind)
          or IsAssociatedInterfaceRequestKind(kind)
          or IsPendingAssociatedRemoteKind(kind)
          or IsPendingAssociatedReceiverKind(kind))


def HasCallbacks(interface):
  for method in interface.methods:
    if method.response_parameters != None:
      return True
  return False


# Finds out whether an interface passes associated interfaces and associated
# interface requests.
def PassesAssociatedKinds(interface):
  visited_kinds = set()
  for method in interface.methods:
    if MethodPassesAssociatedKinds(method, visited_kinds):
      return True
  return False


def _AnyMethodParameterRecursive(method, predicate, visited_kinds=None):
  def _HasProperty(kind):
    if kind in visited_kinds:
      # No need to examine the kind again.
      return False
    visited_kinds.add(kind)
    if predicate(kind):
      return True
    if IsArrayKind(kind):
      return _HasProperty(kind.kind)
    if IsStructKind(kind) or IsUnionKind(kind):
      for field in kind.fields:
        if _HasProperty(field.kind):
          return True
    if IsMapKind(kind):
      if _HasProperty(kind.key_kind) or _HasProperty(kind.value_kind):
        return True
    return False

  if visited_kinds is None:
    visited_kinds = set()

  for param in method.parameters:
    if _HasProperty(param.kind):
      return True
  if method.response_parameters != None:
    for param in method.response_parameters:
      if _HasProperty(param.kind):
        return True
  return False


# Finds out whether a method passes associated interfaces and associated
# interface requests.
def MethodPassesAssociatedKinds(method, visited_kinds=None):
  return _AnyMethodParameterRecursive(
      method, IsAssociatedKind, visited_kinds=visited_kinds)


# Determines whether a method passes interfaces.
def MethodPassesInterfaces(method):
  return _AnyMethodParameterRecursive(method, IsInterfaceKind)


def GetSyncMethodOrdinals(interface):
  return [method.ordinal for method in interface.methods if method.sync]


def HasUninterruptableMethods(interface):
  for method in interface.methods:
    if not method.allow_interrupt:
      return True
  return False


def ContainsHandlesOrInterfaces(kind):
  """Check if the kind contains any handles.

  This check is recursive so it checks all struct fields, containers elements,
  etc.

  Args:
    struct: {Kind} The kind to check.

  Returns:
    {bool}: True if the kind contains handles.
  """
  # We remember the types we already checked to avoid infinite recursion when
  # checking recursive (or mutually recursive) types:
  checked = set()

  def Check(kind):
    if kind.spec in checked:
      return False
    checked.add(kind.spec)
    if IsStructKind(kind):
      return any(Check(field.kind) for field in kind.fields)
    if IsUnionKind(kind):
      return any(Check(field.kind) for field in kind.fields)
    if IsAnyHandleKind(kind):
      return True
    if IsAnyInterfaceKind(kind):
      return True
    if IsArrayKind(kind):
      return Check(kind.kind)
    if IsMapKind(kind):
      return Check(kind.key_kind) or Check(kind.value_kind)
    return False

  return Check(kind)


def ContainsNativeTypes(kind):
  """Check if the kind contains any native type (struct or enum).

  This check is recursive so it checks all struct fields, scoped interface
  enums, etc.

  Args:
    struct: {Kind} The kind to check.

  Returns:
    {bool}: True if the kind contains native types.
  """
  # We remember the types we already checked to avoid infinite recursion when
  # checking recursive (or mutually recursive) types:
  checked = set()

  def Check(kind):
    if kind.spec in checked:
      return False
    checked.add(kind.spec)
    if IsEnumKind(kind):
      return kind.native_only
    if IsStructKind(kind):
      if kind.native_only:
        return True
      if any(enum.native_only for enum in kind.enums):
        return True
      return any(Check(field.kind) for field in kind.fields)
    if IsUnionKind(kind):
      return any(Check(field.kind) for field in kind.fields)
    if IsInterfaceKind(kind):
      return any(enum.native_only for enum in kind.enums)
    if IsArrayKind(kind):
      return Check(kind.kind)
    if IsMapKind(kind):
      return Check(kind.key_kind) or Check(kind.value_kind)
    return False

  return Check(kind)
