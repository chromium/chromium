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
    is_nullable: True if the type is nullable.
  """

  def __init__(self, spec=None, is_nullable=False, module=None):
    self.spec = spec
    self.module = module
    self.parent_kind = None
    self.is_nullable = is_nullable
    self.shared_definition = {}

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

  def Repr(self, as_ref=True):
    # pylint: disable=unused-argument
    return '<%s spec=%r is_nullable=%r>' % (self.__class__.__name__, self.spec,
                                            self.is_nullable)

  def __repr__(self):
    # Gives us a decent __repr__ for all kinds.
    return self.Repr()

  def __eq__(self, rhs):
    # pylint: disable=unidiomatic-typecheck
    return (type(self) == type(rhs)
            and (self.spec, self.parent_kind, self.is_nullable)
            == (rhs.spec, rhs.parent_kind, rhs.is_nullable))

  def __hash__(self):
    # TODO(crbug.com/40122051): Remove this and other __hash__ methods on Kind
    # and its subclasses. This is to support existing generator code which uses
    # some primitive Kinds as dict keys. The default hash (object identity)
    # breaks these dicts when a pickled Module instance is unpickled and used
    # during a subsequent run of the parser.
    return hash((self.spec, self.parent_kind, self.is_nullable))


class ValueKind(Kind):
  """ValueKind represents values that aren't reference kinds.

  The primary difference is the wire representation for nullable value kinds
  still reserves space for the value type itself, even if that value itself
  is logically null.
  """
  def __init__(self, spec=None, is_nullable=False, module=None):
    assert spec is None or is_nullable == spec.startswith('?')
    Kind.__init__(self, spec, is_nullable, module)

  def MakeNullableKind(self):
    assert not self.is_nullable

    if self == BOOL:
      return NULLABLE_BOOL
    if self == INT8:
      return NULLABLE_INT8
    if self == INT16:
      return NULLABLE_INT16
    if self == INT32:
      return NULLABLE_INT32
    if self == INT64:
      return NULLABLE_INT64
    if self == UINT8:
      return NULLABLE_UINT8
    if self == UINT16:
      return NULLABLE_UINT16
    if self == UINT32:
      return NULLABLE_UINT32
    if self == UINT64:
      return NULLABLE_UINT64
    if self == FLOAT:
      return NULLABLE_FLOAT
    if self == DOUBLE:
      return NULLABLE_DOUBLE

    nullable_kind = type(self)()
    nullable_kind.shared_definition = self.shared_definition
    if self.spec is not None:
      nullable_kind.spec = '?' + self.spec
    nullable_kind.is_nullable = True
    nullable_kind.parent_kind = self.parent_kind
    nullable_kind.module = self.module

    return nullable_kind

  def MakeUnnullableKind(self):
    assert self.is_nullable

    if self == NULLABLE_BOOL:
      return BOOL
    if self == NULLABLE_INT8:
      return INT8
    if self == NULLABLE_INT16:
      return INT16
    if self == NULLABLE_INT32:
      return INT32
    if self == NULLABLE_INT64:
      return INT64
    if self == NULLABLE_UINT8:
      return UINT8
    if self == NULLABLE_UINT16:
      return UINT16
    if self == NULLABLE_UINT32:
      return UINT32
    if self == NULLABLE_UINT64:
      return UINT64
    if self == NULLABLE_FLOAT:
      return FLOAT
    if self == NULLABLE_DOUBLE:
      return DOUBLE

    nullable_kind = type(self)()
    nullable_kind.shared_definition = self.shared_definition
    if self.spec is not None:
      nullable_kind.spec = self.spec[1:]
    nullable_kind.is_nullable = False
    nullable_kind.parent_kind = self.parent_kind
    nullable_kind.module = self.module

    return nullable_kind

  def __eq__(self, rhs):
    return (isinstance(rhs, ValueKind) and super().__eq__(rhs))

  def __hash__(self):  # pylint: disable=useless-super-delegation
    return super().__hash__()


class ReferenceKind(Kind):
  """ReferenceKind represents pointer and handle types.

  A type is nullable if null (for pointer types) or invalid handle (for handle
  types) is a legal value for the type.
  """

  def __init__(self, spec=None, is_nullable=False, module=None):
    assert spec is None or is_nullable == spec.startswith('?')
    Kind.__init__(self, spec, is_nullable, module)

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

  def MakeUnnullableKind(self):
    assert self.is_nullable

    if self == NULLABLE_STRING:
      return STRING
    if self == NULLABLE_HANDLE:
      return HANDLE
    if self == NULLABLE_DCPIPE:
      return DCPIPE
    if self == NULLABLE_DPPIPE:
      return DPPIPE
    if self == NULLABLE_MSGPIPE:
      return MSGPIPE
    if self == NULLABLE_SHAREDBUFFER:
      return SHAREDBUFFER
    if self == NULLABLE_PLATFORMHANDLE:
      return PLATFORMHANDLE

    unnullable_kind = type(self)()
    unnullable_kind.shared_definition = self.shared_definition
    if self.spec is not None:
      assert self.spec[0] == '?'
      unnullable_kind.spec = self.spec[1:]
    unnullable_kind.is_nullable = False
    unnullable_kind.parent_kind = self.parent_kind
    unnullable_kind.module = self.module

    return unnullable_kind

  def __eq__(self, rhs):
    return (isinstance(rhs, ReferenceKind) and super().__eq__(rhs))

  def __hash__(self):  # pylint: disable=useless-super-delegation
    return super().__hash__()


# Initialize the set of primitive types. These can be accessed by clients.
BOOL = ValueKind('b')
INT8 = ValueKind('i8')
INT16 = ValueKind('i16')
INT32 = ValueKind('i32')
INT64 = ValueKind('i64')
UINT8 = ValueKind('u8')
UINT16 = ValueKind('u16')
UINT32 = ValueKind('u32')
UINT64 = ValueKind('u64')
FLOAT = ValueKind('f')
DOUBLE = ValueKind('d')
NULLABLE_BOOL = ValueKind('?b', True)
NULLABLE_INT8 = ValueKind('?i8', True)
NULLABLE_INT16 = ValueKind('?i16', True)
NULLABLE_INT32 = ValueKind('?i32', True)
NULLABLE_INT64 = ValueKind('?i64', True)
NULLABLE_UINT8 = ValueKind('?u8', True)
NULLABLE_UINT16 = ValueKind('?u16', True)
NULLABLE_UINT32 = ValueKind('?u32', True)
NULLABLE_UINT64 = ValueKind('?u64', True)
NULLABLE_FLOAT = ValueKind('?f', True)
NULLABLE_DOUBLE = ValueKind('?d', True)
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
    NULLABLE_BOOL,
    NULLABLE_INT8,
    NULLABLE_INT16,
    NULLABLE_INT32,
    NULLABLE_INT64,
    NULLABLE_UINT8,
    NULLABLE_UINT16,
    NULLABLE_UINT32,
    NULLABLE_UINT64,
    NULLABLE_FLOAT,
    NULLABLE_DOUBLE,
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
ATTRIBUTE_DISPATCH_DEBUG_ALIAS = 'DispatchDebugAlias'
ATTRIBUTE_ESTIMATE_SIZE = 'EstimateSize'
ATTRIBUTE_EXTENSIBLE = 'Extensible'
ATTRIBUTE_NO_INTERRUPT = 'NoInterrupt'
ATTRIBUTE_STABLE = 'Stable'
ATTRIBUTE_SUPPORTS_URGENT = 'SupportsUrgent'
ATTRIBUTE_SYNC = 'Sync'
ATTRIBUTE_UNLIMITED_SIZE = 'UnlimitedSize'
ATTRIBUTE_UUID = 'Uuid'
ATTRIBUTE_SERVICE_SANDBOX = 'ServiceSandbox'
ATTRIBUTE_REQUIRE_CONTEXT = 'RequireContext'
ATTRIBUTE_ALLOWED_CONTEXT = 'AllowedContext'
ATTRIBUTE_RUNTIME_FEATURE = 'RuntimeFeature'


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


class Feature(ReferenceKind):
  """A runtime enabled feature defined from mojom.

  Attributes:
    mojom_name: {str} The name of the feature type as defined in mojom.
    name: {str} The stylized name. (Note: not the "name" used by FeatureList.)
    constants: {List[Constant]} The constants defined in the feature scope.
    attributes: {dict} Additional information about the feature.
  """

  Kind.AddSharedProperty('mojom_name')
  Kind.AddSharedProperty('name')
  Kind.AddSharedProperty('constants')
  Kind.AddSharedProperty('attributes')

  def __init__(self, mojom_name=None, module=None, attributes=None):
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    ReferenceKind.__init__(self, spec, False, module)
    self.mojom_name = mojom_name
    self.name = None
    self.constants = []
    self.attributes = attributes

  def Stylize(self, stylizer):
    self.name = stylizer.StylizeFeature(self.mojom_name)
    for constant in self.constants:
      constant.Stylize(stylizer)


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

  Kind.AddSharedProperty('mojom_name')
  Kind.AddSharedProperty('name')
  Kind.AddSharedProperty('native_only')
  Kind.AddSharedProperty('custom_serializer')
  Kind.AddSharedProperty('fields')
  Kind.AddSharedProperty('enums')
  Kind.AddSharedProperty('constants')
  Kind.AddSharedProperty('attributes')

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
  Kind.AddSharedProperty('mojom_name')
  Kind.AddSharedProperty('name')
  Kind.AddSharedProperty('fields')
  Kind.AddSharedProperty('attributes')
  Kind.AddSharedProperty('default_field')

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

  Kind.AddSharedProperty('kind')
  Kind.AddSharedProperty('length')

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


class Map(ReferenceKind):
  """A map.

  Attributes:
    key_kind: {Kind} The type of the keys. May be None.
    value_kind: {Kind} The type of the elements. May be None.
  """
  Kind.AddSharedProperty('key_kind')
  Kind.AddSharedProperty('value_kind')

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


class PendingRemote(ReferenceKind):
  Kind.AddSharedProperty('kind')

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


class PendingReceiver(ReferenceKind):
  Kind.AddSharedProperty('kind')

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


class PendingAssociatedRemote(ReferenceKind):
  Kind.AddSharedProperty('kind')

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


class PendingAssociatedReceiver(ReferenceKind):
  Kind.AddSharedProperty('kind')

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
  def estimate_message_size(self):
    return self.attributes.get(ATTRIBUTE_ESTIMATE_SIZE) \
        if self.attributes else False

  @property
  def unlimited_message_size(self):
    return self.attributes.get(ATTRIBUTE_UNLIMITED_SIZE) \
        if self.attributes else False

  @property
  def allowed_context(self):
    return self.attributes.get(ATTRIBUTE_ALLOWED_CONTEXT) \
        if self.attributes else None

  @property
  def supports_urgent(self):
    return self.attributes.get(ATTRIBUTE_SUPPORTS_URGENT) \
        if self.attributes else None

  @property
  def runtime_feature(self):
    if not self.attributes:
      return None
    runtime_feature = self.attributes.get(ATTRIBUTE_RUNTIME_FEATURE, None)
    if runtime_feature is None:
      return None
    if not isinstance(runtime_feature, Feature):
      raise Exception("RuntimeFeature attribute on %s must be a feature." %
                      self.name)
    return runtime_feature

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
  Kind.AddSharedProperty('mojom_name')
  Kind.AddSharedProperty('name')
  Kind.AddSharedProperty('methods')
  Kind.AddSharedProperty('enums')
  Kind.AddSharedProperty('constants')
  Kind.AddSharedProperty('attributes')

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
  def runtime_feature(self):
    if not self.attributes:
      return None
    runtime_feature = self.attributes.get(ATTRIBUTE_RUNTIME_FEATURE, None)
    if runtime_feature is None:
      return None
    if not isinstance(runtime_feature, Feature):
      raise Exception("RuntimeFeature attribute on %s must be a feature." %
                      self.name)
    return runtime_feature

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

  @property
  def dispatch_debug_alias(self):
    return self.attributes.get(ATTRIBUTE_DISPATCH_DEBUG_ALIAS) \
           if self.attributes else None

  def __hash__(self):
    return id(self)


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


class Enum(ValueKind):
  Kind.AddSharedProperty('mojom_name')
  Kind.AddSharedProperty('name')
  Kind.AddSharedProperty('native_only')
  Kind.AddSharedProperty('fields')
  Kind.AddSharedProperty('attributes')
  Kind.AddSharedProperty('min_value')
  Kind.AddSharedProperty('max_value')
  Kind.AddSharedProperty('default_field')

  def __init__(self, mojom_name=None, module=None, attributes=None):
    if mojom_name is not None:
      spec = 'x:' + mojom_name
    else:
      spec = None
    super().__init__(spec, False, module)
    self.mojom_name = mojom_name
    self.name = None
    self.native_only = False
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
    self.features = []
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
    return (isinstance(rhs, Module)
            and (self.path, self.attributes, self.mojom_namespace, self.imports,
                 self.constants, self.enums, self.structs, self.unions,
                 self.interfaces, self.features)
            == (rhs.path, rhs.attributes, rhs.mojom_namespace, rhs.imports,
                rhs.constants, rhs.enums, rhs.structs, rhs.unions,
                rhs.interfaces, rhs.features))

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
            'unions': False,
            'features': False,
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

  def AddFeature(self, mojom_name, attributes=None):
    feature = Feature(mojom_name, self, attributes)
    self.features.append(feature)
    return feature

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
    for feature in self.features:
      feature.Stylize(stylizer)

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
  return kind.spec == BOOL.spec or kind.spec == NULLABLE_BOOL.spec


def IsFloatKind(kind):
  return kind.spec == FLOAT.spec or kind.spec == NULLABLE_FLOAT.spec


def IsDoubleKind(kind):
  return kind.spec == DOUBLE.spec or kind.spec == NULLABLE_DOUBLE.spec


def IsIntegralKind(kind):
  return (kind.spec == BOOL.spec or kind.spec == INT8.spec
          or kind.spec == INT16.spec or kind.spec == INT32.spec
          or kind.spec == INT64.spec or kind.spec == UINT8.spec
          or kind.spec == UINT16.spec or kind.spec == UINT32.spec
          or kind.spec == UINT64.spec or kind.spec == NULLABLE_BOOL.spec
          or kind.spec == NULLABLE_INT8.spec or kind.spec == NULLABLE_INT16.spec
          or kind.spec == NULLABLE_INT32.spec
          or kind.spec == NULLABLE_INT64.spec
          or kind.spec == NULLABLE_UINT8.spec
          or kind.spec == NULLABLE_UINT16.spec
          or kind.spec == NULLABLE_UINT32.spec
          or kind.spec == NULLABLE_UINT64.spec)


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


def IsFeatureKind(kind):
  return isinstance(kind, Feature)


def IsInterfaceKind(kind):
  return isinstance(kind, Interface)


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


def IsValueKind(kind):
  return isinstance(kind, ValueKind)


def IsReferenceKind(kind):
  return isinstance(kind, ReferenceKind)


def IsNullableKind(kind):
  return kind.is_nullable


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
  return (IsInterfaceKind(kind) or IsAssociatedKind(kind)
          or IsPendingRemoteKind(kind) or IsPendingReceiverKind(kind))


def IsAnyHandleOrInterfaceKind(kind):
  return IsAnyHandleKind(kind) or IsAnyInterfaceKind(kind)


def IsAssociatedKind(kind):
  return (IsPendingAssociatedRemoteKind(kind)
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


def EnsureUnnullable(kind):
  if IsNullableKind(kind):
    return kind.MakeUnnullableKind()
  return kind
