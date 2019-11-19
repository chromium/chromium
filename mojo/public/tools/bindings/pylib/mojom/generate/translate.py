# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert parse tree to AST.

This module converts the parse tree to the AST we use for code generation. The
main entry point is OrderedModule, which gets passed the parser
representation of a mojom file. When called it's assumed that all imports have
already been parsed and converted to ASTs before.
"""

import itertools
import os
import re

import mojom.generate.module as mojom
from mojom.parse import ast

def _DuplicateName(values):
  """Returns the 'mojom_name' of the first entry in |values| whose 'mojom_name'
  has already been encountered. If there are no duplicates, returns None."""
  names = set()
  for value in values:
    if value.mojom_name in names:
      return value.mojom_name
    names.add(value.mojom_name)
  return None

def _ElemsOfType(elems, elem_type, scope):
  """Find all elements of the given type.

  Args:
    elems: {Sequence[Any]} Sequence of elems.
    elem_type: {Type[C]} Extract all elems of this type.
    scope: {str} The name of the surrounding scope (e.g. struct
        definition). Used in error messages.

  Returns:
    {List[C]} All elems of matching type.
  """
  assert isinstance(elem_type, type)
  result = [elem for elem in elems if isinstance(elem, elem_type)]
  duplicate_name = _DuplicateName(result)
  if duplicate_name:
    raise Exception('Names in mojom must be unique within a scope. The name '
                    '"%s" is used more than once within the scope "%s".' %
                    (duplicate_name, scope))
  return result

def _MapKind(kind):
  map_to_kind = {'bool': 'b',
                 'int8': 'i8',
                 'int16': 'i16',
                 'int32': 'i32',
                 'int64': 'i64',
                 'uint8': 'u8',
                 'uint16': 'u16',
                 'uint32': 'u32',
                 'uint64': 'u64',
                 'float': 'f',
                 'double': 'd',
                 'string': 's',
                 'handle': 'h',
                 'handle<data_pipe_consumer>': 'h:d:c',
                 'handle<data_pipe_producer>': 'h:d:p',
                 'handle<message_pipe>': 'h:m',
                 'handle<shared_buffer>': 'h:s'}
  if kind.endswith('?'):
    base_kind = _MapKind(kind[0:-1])
    # NOTE: This doesn't rule out enum types. Those will be detected later, when
    # cross-reference is established.
    reference_kinds = ('m', 's', 'h', 'a', 'r', 'x', 'asso', 'rmt', 'rcv',
                       'rma', 'rca')
    if re.split('[^a-z]', base_kind, 1)[0] not in reference_kinds:
      raise Exception(
          'A type (spec "%s") cannot be made nullable' % base_kind)
    return '?' + base_kind
  if kind.endswith('}'):
    lbracket = kind.rfind('{')
    value = kind[0:lbracket]
    return 'm[' + _MapKind(kind[lbracket+1:-1]) + '][' + _MapKind(value) + ']'
  if kind.endswith(']'):
    lbracket = kind.rfind('[')
    typename = kind[0:lbracket]
    return 'a' + kind[lbracket+1:-1] + ':' + _MapKind(typename)
  if kind.endswith('&'):
    return 'r:' + _MapKind(kind[0:-1])
  if kind.startswith('asso<'):
    assert kind.endswith('>')
    return 'asso:' + _MapKind(kind[5:-1])
  if kind.startswith('rmt<'):
    assert kind.endswith('>')
    return 'rmt:' + _MapKind(kind[4:-1])
  if kind.startswith('rcv<'):
    assert kind.endswith('>')
    return 'rcv:' + _MapKind(kind[4:-1])
  if kind.startswith('rma<'):
    assert kind.endswith('>')
    return 'rma:' + _MapKind(kind[4:-1])
  if kind.startswith('rca<'):
    assert kind.endswith('>')
    return 'rca:' + _MapKind(kind[4:-1])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind

def _AttributeListToDict(attribute_list):
  if attribute_list is None:
    return None
  assert isinstance(attribute_list, ast.AttributeList)
  # TODO(vtl): Check for duplicate keys here.
  return dict([(attribute.key, attribute.value)
                   for attribute in attribute_list])

builtin_values = frozenset([
    "double.INFINITY",
    "double.NEGATIVE_INFINITY",
    "double.NAN",
    "float.INFINITY",
    "float.NEGATIVE_INFINITY",
    "float.NAN"])

def _IsBuiltinValue(value):
  return value in builtin_values

def _LookupKind(kinds, spec, scope):
  """Tries to find which Kind a spec refers to, given the scope in which its
  referenced. Starts checking from the narrowest scope to most general. For
  example, given a struct field like
    Foo.Bar x;
  Foo.Bar could refer to the type 'Bar' in the 'Foo' namespace, or an inner
  type 'Bar' in the struct 'Foo' in the current namespace.

  |scope| is a tuple that looks like (namespace, struct/interface), referring
  to the location where the type is referenced."""
  if spec.startswith('x:'):
    mojom_name = spec[2:]
    for i in range(len(scope), -1, -1):
      test_spec = 'x:'
      if i > 0:
        test_spec += '.'.join(scope[:i]) + '.'
      test_spec += mojom_name
      kind = kinds.get(test_spec)
      if kind:
        return kind

  return kinds.get(spec)

def _LookupValue(values, mojom_name, scope, kind):
  """Like LookupKind, but for constant values."""
  # If the type is an enum, the value can be specified as a qualified name, in
  # which case the form EnumName.ENUM_VALUE must be used. We use the presence
  # of a '.' in the requested name to identify this. Otherwise, we prepend the
  # enum name.
  if isinstance(kind, mojom.Enum) and '.' not in mojom_name:
    mojom_name = '%s.%s' % (kind.spec.split(':', 1)[1], mojom_name)
  for i in reversed(range(len(scope) + 1)):
    test_spec = '.'.join(scope[:i])
    if test_spec:
      test_spec += '.'
    test_spec += mojom_name
    value = values.get(test_spec)
    if value:
      return value

  return values.get(mojom_name)

def _FixupExpression(module, value, scope, kind):
  """Translates an IDENTIFIER into a built-in value or structured NamedValue
     object."""
  if isinstance(value, tuple) and value[0] == 'IDENTIFIER':
    # Allow user defined values to shadow builtins.
    result = _LookupValue(module.values, value[1], scope, kind)
    if result:
      if isinstance(result, tuple):
        raise Exception('Unable to resolve expression: %r' % value[1])
      return result
    if _IsBuiltinValue(value[1]):
      return mojom.BuiltinValue(value[1])
  return value

def _Kind(kinds, spec, scope):
  """Convert a type name into a mojom.Kind object.

  As a side-effect this function adds the result to 'kinds'.

  Args:
    kinds: {Dict[str, mojom.Kind]} All known kinds up to this point, indexed by
        their names.
    spec: {str} A name uniquely identifying a type.
    scope: {Tuple[str, str]} A tuple that looks like (namespace,
        struct/interface), referring to the location where the type is
        referenced.

  Returns:
    {mojom.Kind} The type corresponding to 'spec'.
  """
  kind = _LookupKind(kinds, spec, scope)
  if kind:
    return kind

  if spec.startswith('?'):
    kind = _Kind(kinds, spec[1:], scope).MakeNullableKind()
  elif spec.startswith('a:'):
    kind = mojom.Array(_Kind(kinds, spec[2:], scope))
  elif spec.startswith('asso:'):
    inner_kind = _Kind(kinds, spec[5:], scope)
    if isinstance(inner_kind, mojom.InterfaceRequest):
      kind = mojom.AssociatedInterfaceRequest(inner_kind)
    else:
      kind = mojom.AssociatedInterface(inner_kind)
  elif spec.startswith('a'):
    colon = spec.find(':')
    length = int(spec[1:colon])
    kind = mojom.Array(_Kind(kinds, spec[colon+1:], scope), length)
  elif spec.startswith('r:'):
    kind = mojom.InterfaceRequest(_Kind(kinds, spec[2:], scope))
  elif spec.startswith('rmt:'):
    kind = mojom.PendingRemote(_Kind(kinds, spec[4:], scope))
  elif spec.startswith('rcv:'):
    kind = mojom.PendingReceiver(_Kind(kinds, spec[4:], scope))
  elif spec.startswith('rma:'):
    kind = mojom.PendingAssociatedRemote(_Kind(kinds, spec[4:], scope))
  elif spec.startswith('rca:'):
    kind = mojom.PendingAssociatedReceiver(_Kind(kinds, spec[4:], scope))
  elif spec.startswith('m['):
    # Isolate the two types from their brackets.

    # It is not allowed to use map as key, so there shouldn't be nested ']'s
    # inside the key type spec.
    key_end = spec.find(']')
    assert key_end != -1 and key_end < len(spec) - 1
    assert spec[key_end+1] == '[' and spec[-1] == ']'

    first_kind = spec[2:key_end]
    second_kind = spec[key_end+2:-1]

    kind = mojom.Map(_Kind(kinds, first_kind, scope),
                     _Kind(kinds, second_kind, scope))
  else:
    kind = mojom.Kind(spec)

  kinds[spec] = kind
  return kind

def _Import(module, import_module):
  # Copy the struct kinds from our imports into the current module.
  importable_kinds = (mojom.Struct, mojom.Union, mojom.Enum, mojom.Interface)
  for kind in import_module.kinds.values():
    if (isinstance(kind, importable_kinds) and
        kind.module.path == import_module.path):
      module.kinds[kind.spec] = kind
  # Ditto for values.
  for value in import_module.values.values():
    if value.module.path == import_module.path:
      module.values[value.GetSpec()] = value

  return import_module

def _Struct(module, parsed_struct):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_struct: {ast.Struct} Parsed struct.

  Returns:
    {mojom.Struct} AST struct.
  """
  struct = mojom.Struct(module=module)
  struct.mojom_name = parsed_struct.mojom_name
  struct.native_only = parsed_struct.body is None
  struct.spec = 'x:' + module.mojom_namespace + '.' + struct.mojom_name
  module.kinds[struct.spec] = struct
  if struct.native_only:
    struct.enums = []
    struct.constants = []
    struct.fields_data = []
  else:
    struct.enums = map(
        lambda enum: _Enum(module, enum, struct),
        _ElemsOfType(parsed_struct.body, ast.Enum, parsed_struct.mojom_name))
    struct.constants = map(
        lambda constant: _Constant(module, constant, struct),
        _ElemsOfType(parsed_struct.body, ast.Const, parsed_struct.mojom_name))
    # Stash fields parsed_struct here temporarily.
    struct.fields_data = _ElemsOfType(
        parsed_struct.body, ast.StructField, parsed_struct.mojom_name)
  struct.attributes = _AttributeListToDict(parsed_struct.attribute_list)

  # Enforce that a [Native] attribute is set to make native-only struct
  # declarations more explicit.
  if struct.native_only:
    if not struct.attributes or not struct.attributes.get('Native', False):
      raise Exception("Native-only struct declarations must include a " +
                      "Native attribute.")

  if struct.attributes and struct.attributes.get('CustomSerializer', False):
    struct.custom_serializer = True

  return struct

def _Union(module, parsed_union):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_union: {ast.Union} Parsed union.

  Returns:
    {mojom.Union} AST union.
  """
  union = mojom.Union(module=module)
  union.mojom_name = parsed_union.mojom_name
  union.spec = 'x:' + module.mojom_namespace + '.' + union.mojom_name
  module.kinds[union.spec] = union
  # Stash fields parsed_union here temporarily.
  union.fields_data = _ElemsOfType(
      parsed_union.body, ast.UnionField, parsed_union.mojom_name)
  union.attributes = _AttributeListToDict(parsed_union.attribute_list)
  return union

def _StructField(module, parsed_field, struct):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_field: {ast.StructField} Parsed struct field.
    struct: {mojom.Struct} Struct this field belongs to.

  Returns:
    {mojom.StructField} AST struct field.
  """
  field = mojom.StructField()
  field.mojom_name = parsed_field.mojom_name
  field.kind = _Kind(
      module.kinds, _MapKind(parsed_field.typename),
      (module.mojom_namespace, struct.mojom_name))
  field.ordinal = parsed_field.ordinal.value if parsed_field.ordinal else None
  field.default = _FixupExpression(
      module, parsed_field.default_value,
      (module.mojom_namespace, struct.mojom_name), field.kind)
  field.attributes = _AttributeListToDict(parsed_field.attribute_list)
  return field

def _UnionField(module, parsed_field, union):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_field: {ast.UnionField} Parsed union field.
    union: {mojom.Union} Union this fields belong to.

  Returns:
    {mojom.UnionField} AST union.
  """
  field = mojom.UnionField()
  field.mojom_name = parsed_field.mojom_name
  field.kind = _Kind(
      module.kinds, _MapKind(parsed_field.typename),
      (module.mojom_namespace, union.mojom_name))
  field.ordinal = parsed_field.ordinal.value if parsed_field.ordinal else None
  field.default = _FixupExpression(
      module, None, (module.mojom_namespace, union.mojom_name), field.kind)
  field.attributes = _AttributeListToDict(parsed_field.attribute_list)
  return field

def _Parameter(module, parsed_param, interface):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_param: {ast.Parameter} Parsed parameter.
    union: {mojom.Interface} Interface this parameter belongs to.

  Returns:
    {mojom.Parameter} AST parameter.
  """
  parameter = mojom.Parameter()
  parameter.mojom_name = parsed_param.mojom_name
  parameter.kind = _Kind(
      module.kinds, _MapKind(parsed_param.typename),
      (module.mojom_namespace, interface.mojom_name))
  parameter.ordinal = (
      parsed_param.ordinal.value if parsed_param.ordinal else None)
  parameter.default = None  # TODO(tibell): We never have these. Remove field?
  parameter.attributes = _AttributeListToDict(parsed_param.attribute_list)
  return parameter

def _Method(module, parsed_method, interface):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_method: {ast.Method} Parsed method.
    interface: {mojom.Interface} Interface this method belongs to.

  Returns:
    {mojom.Method} AST method.
  """
  method = mojom.Method(
      interface, parsed_method.mojom_name,
      ordinal=parsed_method.ordinal.value if parsed_method.ordinal else None)
  method.parameters = map(
      lambda parameter: _Parameter(module, parameter, interface),
      parsed_method.parameter_list)
  if parsed_method.response_parameter_list is not None:
    method.response_parameters = map(
        lambda parameter: _Parameter(module, parameter, interface),
                          parsed_method.response_parameter_list)
  method.attributes = _AttributeListToDict(parsed_method.attribute_list)

  # Enforce that only methods with response can have a [Sync] attribute.
  if method.sync and method.response_parameters is None:
    raise Exception("Only methods with response can include a [Sync] "
                    "attribute. If no response parameters are needed, you "
                    "could use an empty response parameter list, i.e., "
                    "\"=> ()\".")

  return method

def _Interface(module, parsed_iface):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_iface: {ast.Interface} Parsed interface.

  Returns:
    {mojom.Interface} AST interface.
  """
  interface = mojom.Interface(module=module)
  interface.mojom_name = parsed_iface.mojom_name
  interface.spec = 'x:' + module.mojom_namespace + '.' + interface.mojom_name
  module.kinds[interface.spec] = interface
  interface.enums = map(
      lambda enum: _Enum(module, enum, interface),
      _ElemsOfType(parsed_iface.body, ast.Enum, parsed_iface.mojom_name))
  interface.constants = map(
      lambda constant: _Constant(module, constant, interface),
      _ElemsOfType(parsed_iface.body, ast.Const, parsed_iface.mojom_name))
  # Stash methods parsed_iface here temporarily.
  interface.methods_data = _ElemsOfType(
      parsed_iface.body, ast.Method, parsed_iface.mojom_name)
  interface.attributes = _AttributeListToDict(parsed_iface.attribute_list)
  return interface

def _EnumField(module, enum, parsed_field, parent_kind):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    enum: {mojom.Enum} Enum this field belongs to.
    parsed_field: {ast.EnumValue} Parsed enum value.
    parent_kind: {mojom.Kind} The enclosing type.

  Returns:
    {mojom.EnumField} AST enum field.
  """
  field = mojom.EnumField()
  field.mojom_name = parsed_field.mojom_name
  # TODO(mpcomplete): FixupExpression should be done in the second pass,
  # so constants and enums can refer to each other.
  # TODO(mpcomplete): But then, what if constants are initialized to an enum? Or
  # vice versa?
  if parent_kind:
    field.value = _FixupExpression(
        module, parsed_field.value,
        (module.mojom_namespace, parent_kind.mojom_name), enum)
  else:
    field.value = _FixupExpression(
        module, parsed_field.value, (module.mojom_namespace, ), enum)
  field.attributes = _AttributeListToDict(parsed_field.attribute_list)
  value = mojom.EnumValue(module, enum, field)
  module.values[value.GetSpec()] = value
  return field

def _ResolveNumericEnumValues(enum_fields):
  """
  Given a reference to a list of mojom.EnumField, resolves and assigns their
  values to EnumField.numeric_value.

  Returns:
    A tuple of the lowest and highest assigned enumerator value or None, None
    if no enumerator values were assigned.
  """

  # map of <mojom_name> -> integral value
  resolved_enum_values = {}
  prev_value = -1
  min_value = None
  max_value = None
  for field in enum_fields:
    # This enum value is +1 the previous enum value (e.g: BEGIN).
    if field.value is None:
      prev_value += 1

    # Integral value (e.g: BEGIN = -0x1).
    elif type(field.value) is str:
      prev_value = int(field.value, 0)

    # Reference to a previous enum value (e.g: INIT = BEGIN).
    elif type(field.value) is mojom.EnumValue:
      prev_value = resolved_enum_values[field.value.mojom_name]
    else:
      raise Exception("Unresolved enum value.")

    resolved_enum_values[field.mojom_name] = prev_value
    field.numeric_value = prev_value
    if min_value is None or prev_value < min_value:
      min_value = prev_value
    if max_value is None or prev_value > max_value:
      max_value = prev_value

  return min_value, max_value

def _Enum(module, parsed_enum, parent_kind):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_enum: {ast.Enum} Parsed enum.

  Returns:
    {mojom.Enum} AST enum.
  """
  enum = mojom.Enum(module=module)
  enum.mojom_name = parsed_enum.mojom_name
  enum.native_only = parsed_enum.enum_value_list is None
  mojom_name = enum.mojom_name
  if parent_kind:
    mojom_name = parent_kind.mojom_name + '.' + mojom_name
  enum.spec = 'x:%s.%s' % (module.mojom_namespace, mojom_name)
  enum.parent_kind = parent_kind
  enum.attributes = _AttributeListToDict(parsed_enum.attribute_list)
  if not enum.native_only:
    enum.fields = map(
        lambda field: _EnumField(module, enum, field, parent_kind),
        parsed_enum.enum_value_list)
    enum.min_value, enum.max_value = _ResolveNumericEnumValues(enum.fields)

  module.kinds[enum.spec] = enum

  # Enforce that a [Native] attribute is set to make native-only enum
  # declarations more explicit.
  if enum.native_only:
    if not enum.attributes or not enum.attributes.get('Native', False):
      raise Exception("Native-only enum declarations must include a " +
                      "Native attribute.")

  return enum

def _Constant(module, parsed_const, parent_kind):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    parsed_const: {ast.Const} Parsed constant.

  Returns:
    {mojom.Constant} AST constant.
  """
  constant = mojom.Constant()
  constant.mojom_name = parsed_const.mojom_name
  if parent_kind:
    scope = (module.mojom_namespace, parent_kind.mojom_name)
  else:
    scope = (module.mojom_namespace, )
  # TODO(mpcomplete): maybe we should only support POD kinds.
  constant.kind = _Kind(module.kinds, _MapKind(parsed_const.typename), scope)
  constant.parent_kind = parent_kind
  constant.value = _FixupExpression(module, parsed_const.value, scope, None)

  value = mojom.ConstantValue(module, parent_kind, constant)
  module.values[value.GetSpec()] = value
  return constant


def _CollectReferencedKinds(module, all_defined_kinds):
  """
  Takes a {mojom.Module} object and a list of all defined kinds within that
  module, and enumerates the complete dict of user-defined mojom types
  (as {mojom.Kind} objects) referenced by the module's own defined kinds (i.e.
  as types of struct or union or interface parameters. The returned dict is
  keyed by kind spec.
  """

  def extract_referenced_user_kinds(kind):
    if mojom.IsArrayKind(kind):
      return extract_referenced_user_kinds(kind.kind)
    if mojom.IsMapKind(kind):
      return (extract_referenced_user_kinds(kind.key_kind) +
          extract_referenced_user_kinds(kind.value_kind))
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsAssociatedKind(kind):
      return [kind.kind]
    if mojom.IsStructKind(kind):
      return [kind]
    if (mojom.IsInterfaceKind(kind) or mojom.IsEnumKind(kind) or
        mojom.IsUnionKind(kind)):
      return [kind]
    return []

  def sanitize_kind(kind):
    """Removes nullability from a kind"""
    if kind.spec.startswith('?'):
      return _Kind(module.kinds, kind.spec[1:],
                   (module.mojom_namespace, ''))
    return kind

  referenced_user_kinds = {}
  for defined_kind in all_defined_kinds:
    if mojom.IsStructKind(defined_kind) or mojom.IsUnionKind(defined_kind):
      for field in defined_kind.fields:
        for referenced_kind in extract_referenced_user_kinds(field.kind):
          sanitized_kind = sanitize_kind(referenced_kind)
          referenced_user_kinds[sanitized_kind.spec] = sanitized_kind

  # Also scan for references in parameter lists
  for interface in module.interfaces:
    for method in interface.methods:
      for param in itertools.chain(method.parameters or [],
                                   method.response_parameters or []):
        if (mojom.IsStructKind(param.kind) or mojom.IsUnionKind(param.kind) or
            mojom.IsEnumKind(param.kind) or
            mojom.IsAnyInterfaceKind(param.kind)):
          for referenced_kind in extract_referenced_user_kinds(param.kind):
            sanitized_kind = sanitize_kind(referenced_kind)
            referenced_user_kinds[sanitized_kind.spec] = sanitized_kind

  return referenced_user_kinds


def _Module(tree, path, imports):
  """
  Args:
    tree: {ast.Mojom} The parse tree.
    path: {str} The path to the mojom file.
    imports: {Dict[str, mojom.Module]} Mapping from filenames, as they appear in
        the import list, to already processed modules. Used to process imports.

  Returns:
    {mojom.Module} An AST for the mojom.
  """
  module = mojom.Module(path=path)
  module.kinds = {}
  for kind in mojom.PRIMITIVES:
    module.kinds[kind.spec] = kind

  module.values = {}

  module.mojom_namespace = tree.module.mojom_namespace[1] if tree.module else ''
  # Imports must come first, because they add to module.kinds which is used
  # by by the others.
  module.imports = [
      _Import(module, imports[imp.import_filename])
      for imp in tree.import_list]
  if tree.module and tree.module.attribute_list:
    assert isinstance(tree.module.attribute_list, ast.AttributeList)
    # TODO(vtl): Check for duplicate keys here.
    module.attributes = dict((attribute.key, attribute.value)
                             for attribute in tree.module.attribute_list)

  filename = os.path.basename(path)
  # First pass collects kinds.
  module.enums = map(
      lambda enum: _Enum(module, enum, None),
      _ElemsOfType(tree.definition_list, ast.Enum, filename))
  module.structs = map(
      lambda struct: _Struct(module, struct),
      _ElemsOfType(tree.definition_list, ast.Struct, filename))
  module.unions = map(
      lambda union: _Union(module, union),
      _ElemsOfType(tree.definition_list, ast.Union, filename))
  module.interfaces = map(
      lambda interface: _Interface(module, interface),
      _ElemsOfType(tree.definition_list, ast.Interface, filename))
  module.constants = map(
      lambda constant: _Constant(module, constant, None),
      _ElemsOfType(tree.definition_list, ast.Const, filename))

  # Second pass expands fields and methods. This allows fields and parameters
  # to refer to kinds defined anywhere in the mojom.
  all_defined_kinds = {}
  for struct in module.structs:
    struct.fields = map(lambda field:
        _StructField(module, field, struct), struct.fields_data)
    del struct.fields_data
    all_defined_kinds[struct.spec] = struct
    for enum in struct.enums:
      all_defined_kinds[enum.spec] = enum
  for union in module.unions:
    union.fields = map(lambda field:
        _UnionField(module, field, union), union.fields_data)
    del union.fields_data
    all_defined_kinds[union.spec] = union
  for interface in module.interfaces:
    interface.methods = map(lambda method:
        _Method(module, method, interface), interface.methods_data)
    del interface.methods_data
    all_defined_kinds[interface.spec] = interface
    for enum in interface.enums:
      all_defined_kinds[enum.spec] = enum
  for enum in module.enums:
    all_defined_kinds[enum.spec] = enum

  all_referenced_kinds = _CollectReferencedKinds(module,
                                                 all_defined_kinds.values())
  imported_kind_specs = set(all_referenced_kinds.keys()).difference(
      set(all_defined_kinds.keys()))
  module.imported_kinds = dict((spec, all_referenced_kinds[spec])
                               for spec in imported_kind_specs)

  return module

def OrderedModule(tree, path, imports):
  """Convert parse tree to AST module.

  Args:
    tree: {ast.Mojom} The parse tree.
    path: {str} The path to the mojom file.
    imports: {Dict[str, mojom.Module]} Mapping from filenames, as they appear in
        the import list, to already processed modules. Used to process imports.

  Returns:
    {mojom.Module} An AST for the mojom.
  """
  module = _Module(tree, path, imports)
  return module
