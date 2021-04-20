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
import sys

from mojom.generate import generator
from mojom.generate import module as mojom
from mojom.parse import ast


def _IsStrOrUnicode(x):
  if sys.version_info[0] < 3:
    return isinstance(x, (unicode, str))
  return isinstance(x, str)


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


def _ProcessElements(scope, elements, operations_by_type):
  """Iterates over the given elements, running a function from
  operations_by_type for any element that matches a key in that dict. The scope
  is the name of the surrounding scope, such as a filename or struct name, used
  only in error messages."""
  names_in_this_scope = set()
  for element in elements:
    # pylint: disable=unidiomatic-typecheck
    element_type = type(element)
    if element_type in operations_by_type:
      if element.mojom_name in names_in_this_scope:
        raise Exception('Names must be unique within a scope. The name "%s" is '
                        'used more than once within the scope "%s".' %
                        (duplicate_name, scope))
      operations_by_type[element_type](element)


def _MapKind(kind):
  map_to_kind = {
      'bool': 'b',
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
      'handle<shared_buffer>': 'h:s',
      'handle<platform>': 'h:p'
  }
  if kind.endswith('?'):
    base_kind = _MapKind(kind[0:-1])
    # NOTE: This doesn't rule out enum types. Those will be detected later, when
    # cross-reference is established.
    reference_kinds = ('m', 's', 'h', 'a', 'r', 'x', 'asso', 'rmt', 'rcv',
                       'rma', 'rca')
    if re.split('[^a-z]', base_kind, 1)[0] not in reference_kinds:
      raise Exception('A type (spec "%s") cannot be made nullable' % base_kind)
    return '?' + base_kind
  if kind.endswith('}'):
    lbracket = kind.rfind('{')
    value = kind[0:lbracket]
    return 'm[' + _MapKind(kind[lbracket + 1:-1]) + '][' + _MapKind(value) + ']'
  if kind.endswith(']'):
    lbracket = kind.rfind('[')
    typename = kind[0:lbracket]
    return 'a' + kind[lbracket + 1:-1] + ':' + _MapKind(typename)
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
  return dict(
      [(attribute.key, attribute.value) for attribute in attribute_list])


builtin_values = frozenset([
    "double.INFINITY", "double.NEGATIVE_INFINITY", "double.NAN",
    "float.INFINITY", "float.NEGATIVE_INFINITY", "float.NAN"
])


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


def _GetScopeForKind(module, kind):
  """For a given kind, returns a tuple of progressively more specific names
  used to qualify the kind. For example if kind is an enum named Bar nested in a
  struct Foo within module 'foo', this would return ('foo', 'Foo', 'Bar')"""
  if isinstance(kind, mojom.Enum) and kind.parent_kind:
    # Enums may be nested in other kinds.
    return _GetScopeForKind(module, kind.parent_kind) + (kind.mojom_name, )

  module_fragment = (module.mojom_namespace, ) if module.mojom_namespace else ()
  kind_fragment = (kind.mojom_name, ) if kind else ()
  return module_fragment + kind_fragment


def _LookupValueInScope(module, kind, identifier):
  """Given a kind and an identifier, this attempts to resolve the given
  identifier to a concrete NamedValue within the scope of the given kind."""
  scope = _GetScopeForKind(module, kind)
  for i in reversed(range(len(scope) + 1)):
    qualified_name = '.'.join(scope[:i] + (identifier, ))
    value = module.values.get(qualified_name)
    if value:
      return value
  return None


def _LookupValue(module, parent_kind, implied_kind, ast_leaf_node):
  """Resolves a leaf node in the form ('IDENTIFIER', 'x') to a constant value
  identified by 'x' in some mojom definition. parent_kind is used as context
  when resolving the identifier. If the given leaf node is not an IDENTIFIER
  (e.g. already a constant value), it is returned as-is.

  If implied_kind is provided, the parsed identifier may also be resolved within
  its scope as fallback. This can be useful for more concise value references
  when assigning enum-typed constants or field values."""
  if not isinstance(ast_leaf_node, tuple) or ast_leaf_node[0] != 'IDENTIFIER':
    return ast_leaf_node

  # First look for a known user-defined identifier to resolve this within the
  # enclosing scope.
  identifier = ast_leaf_node[1]

  value = _LookupValueInScope(module, parent_kind, identifier)
  if value:
    return value

  # Next look in the scope of implied_kind, if provided.
  value = (implied_kind and implied_kind.module and _LookupValueInScope(
      implied_kind.module, implied_kind, identifier))
  if value:
    return value

  # Fall back on defined builtin symbols
  if _IsBuiltinValue(identifier):
    return mojom.BuiltinValue(identifier)

  raise ValueError('Unknown identifier %s' % identifier)


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
    kind = mojom.Array(_Kind(kinds, spec[colon + 1:], scope), length)
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
    assert spec[key_end + 1] == '[' and spec[-1] == ']'

    first_kind = spec[2:key_end]
    second_kind = spec[key_end + 2:-1]

    kind = mojom.Map(
        _Kind(kinds, first_kind, scope), _Kind(kinds, second_kind, scope))
  else:
    kind = mojom.Kind(spec)

  kinds[spec] = kind
  return kind


def _Import(module, import_module):
  # Copy the struct kinds from our imports into the current module.
  importable_kinds = (mojom.Struct, mojom.Union, mojom.Enum, mojom.Interface)
  for kind in import_module.kinds.values():
    if (isinstance(kind, importable_kinds)
        and kind.module.path == import_module.path):
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
  struct.spec = 'x:' + module.GetNamespacePrefix() + struct.mojom_name
  module.kinds[struct.spec] = struct
  struct.enums = []
  struct.constants = []
  struct.fields_data = []
  if not struct.native_only:
    _ProcessElements(
        parsed_struct.mojom_name, parsed_struct.body, {
            ast.Enum:
            lambda enum: struct.enums.append(_Enum(module, enum, struct)),
            ast.Const:
            lambda const: struct.constants.append(
                _Constant(module, const, struct)),
            ast.StructField:
            struct.fields_data.append,
        })

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
  union.spec = 'x:' + module.GetNamespacePrefix() + union.mojom_name
  module.kinds[union.spec] = union
  # Stash fields parsed_union here temporarily.
  union.fields_data = []
  _ProcessElements(parsed_union.mojom_name, parsed_union.body,
                   {ast.UnionField: union.fields_data.append})
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
  field.kind = _Kind(module.kinds, _MapKind(parsed_field.typename),
                     (module.mojom_namespace, struct.mojom_name))
  field.ordinal = parsed_field.ordinal.value if parsed_field.ordinal else None
  field.default = _LookupValue(module, struct, field.kind,
                               parsed_field.default_value)
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
  field.kind = _Kind(module.kinds, _MapKind(parsed_field.typename),
                     (module.mojom_namespace, union.mojom_name))
  field.ordinal = parsed_field.ordinal.value if parsed_field.ordinal else None
  field.default = None
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
  parameter.kind = _Kind(module.kinds, _MapKind(parsed_param.typename),
                         (module.mojom_namespace, interface.mojom_name))
  parameter.ordinal = (parsed_param.ordinal.value
                       if parsed_param.ordinal else None)
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
      interface,
      parsed_method.mojom_name,
      ordinal=parsed_method.ordinal.value if parsed_method.ordinal else None)
  method.parameters = list(
      map(lambda parameter: _Parameter(module, parameter, interface),
          parsed_method.parameter_list))
  if parsed_method.response_parameter_list is not None:
    method.response_parameters = list(
        map(lambda parameter: _Parameter(module, parameter, interface),
            parsed_method.response_parameter_list))
  method.attributes = _AttributeListToDict(parsed_method.attribute_list)

  # Enforce that only methods with response can have a [Sync] attribute.
  if method.sync and method.response_parameters is None:
    raise Exception("Only methods with response can include a [Sync] "
                    "attribute. If no response parameters are needed, you "
                    "could use an empty response parameter list, i.e., "
                    "\"=> ()\".")
  # And only methods with the [Sync] attribute can specify [NoInterrupt].
  if not method.allow_interrupt and not method.sync:
    raise Exception("Only [Sync] methods can be marked [NoInterrupt].")

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
  interface.spec = 'x:' + module.GetNamespacePrefix() + interface.mojom_name
  module.kinds[interface.spec] = interface
  interface.attributes = _AttributeListToDict(parsed_iface.attribute_list)
  interface.enums = []
  interface.constants = []
  interface.methods_data = []
  _ProcessElements(
      parsed_iface.mojom_name, parsed_iface.body, {
          ast.Enum:
          lambda enum: interface.enums.append(_Enum(module, enum, interface)),
          ast.Const:
          lambda const: interface.constants.append(
              _Constant(module, const, interface)),
          ast.Method:
          interface.methods_data.append,
      })
  return interface


def _EnumField(module, enum, parsed_field):
  """
  Args:
    module: {mojom.Module} Module currently being constructed.
    enum: {mojom.Enum} Enum this field belongs to.
    parsed_field: {ast.EnumValue} Parsed enum value.

  Returns:
    {mojom.EnumField} AST enum field.
  """
  field = mojom.EnumField()
  field.mojom_name = parsed_field.mojom_name
  field.value = _LookupValue(module, enum, None, parsed_field.value)
  field.attributes = _AttributeListToDict(parsed_field.attribute_list)
  value = mojom.EnumValue(module, enum, field)
  module.values[value.GetSpec()] = value
  return field


def _ResolveNumericEnumValues(enum):
  """
  Given a reference to a mojom.Enum, resolves and assigns the numeric value of
  each field, and also computes the min_value and max_value of the enum.
  """

  # map of <mojom_name> -> integral value
  prev_value = -1
  min_value = None
  max_value = None
  for field in enum.fields:
    # This enum value is +1 the previous enum value (e.g: BEGIN).
    if field.value is None:
      prev_value += 1

    # Integral value (e.g: BEGIN = -0x1).
    elif _IsStrOrUnicode(field.value):
      prev_value = int(field.value, 0)

    # Reference to a previous enum value (e.g: INIT = BEGIN).
    elif isinstance(field.value, mojom.EnumValue):
      prev_value = field.value.field.numeric_value
    elif isinstance(field.value, mojom.ConstantValue):
      constant = field.value.constant
      kind = constant.kind
      if not mojom.IsIntegralKind(kind) or mojom.IsBoolKind(kind):
        raise ValueError('Enum values must be integers. %s is not an integer.' %
                         constant.mojom_name)
      prev_value = int(constant.value, 0)
    else:
      raise Exception('Unresolved enum value for %s' % field.value.GetSpec())

    #resolved_enum_values[field.mojom_name] = prev_value
    field.numeric_value = prev_value
    if min_value is None or prev_value < min_value:
      min_value = prev_value
    if max_value is None or prev_value > max_value:
      max_value = prev_value

  enum.min_value = min_value
  enum.max_value = max_value


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
    enum.fields = list(
        map(lambda field: _EnumField(module, enum, field),
            parsed_enum.enum_value_list))
    _ResolveNumericEnumValues(enum)
    # TODO(https://crbug.com/731893): Require a default value to be
    # specified.
    for field in enum.fields:
      if field.default:
        if not enum.extensible:
          raise Exception('Non-extensible enums may not specify a default')
        if enum.default_field is not None:
          raise Exception(
              'Only one enumerator value may be specified as the default')
        enum.default_field = field

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
  constant.value = _LookupValue(module, parent_kind, constant.kind,
                                parsed_const.value)

  # Iteratively resolve this constant reference to a concrete value
  while isinstance(constant.value, mojom.ConstantValue):
    constant.value = constant.value.constant.value

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
    if (mojom.IsInterfaceRequestKind(kind) or mojom.IsAssociatedKind(kind)
        or mojom.IsPendingRemoteKind(kind)
        or mojom.IsPendingReceiverKind(kind)):
      return [kind.kind]
    if mojom.IsStructKind(kind):
      return [kind]
    if (mojom.IsInterfaceKind(kind) or mojom.IsEnumKind(kind)
        or mojom.IsUnionKind(kind)):
      return [kind]
    return []

  def sanitize_kind(kind):
    """Removes nullability from a kind"""
    if kind.spec.startswith('?'):
      return _Kind(module.kinds, kind.spec[1:], (module.mojom_namespace, ''))
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
        for referenced_kind in extract_referenced_user_kinds(param.kind):
          sanitized_kind = sanitize_kind(referenced_kind)
          referenced_user_kinds[sanitized_kind.spec] = sanitized_kind

  return referenced_user_kinds


def _AssignDefaultOrdinals(items):
  """Assigns default ordinal values to a sequence of items if necessary."""
  next_ordinal = 0
  for item in items:
    if item.ordinal is not None:
      next_ordinal = item.ordinal + 1
    else:
      item.ordinal = next_ordinal
      next_ordinal += 1


def _AssertTypeIsStable(kind):
  """Raises an error if a type is not stable, meaning it is composed of at least
  one type that is not marked [Stable]."""

  def assertDependencyIsStable(dependency):
    if (mojom.IsEnumKind(dependency) or mojom.IsStructKind(dependency)
        or mojom.IsUnionKind(dependency) or mojom.IsInterfaceKind(dependency)):
      if not dependency.stable:
        raise Exception(
            '%s is marked [Stable] but cannot be stable because it depends on '
            '%s, which is not marked [Stable].' %
            (kind.mojom_name, dependency.mojom_name))
    elif mojom.IsArrayKind(dependency) or mojom.IsAnyInterfaceKind(dependency):
      assertDependencyIsStable(dependency.kind)
    elif mojom.IsMapKind(dependency):
      assertDependencyIsStable(dependency.key_kind)
      assertDependencyIsStable(dependency.value_kind)

  if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
    for field in kind.fields:
      assertDependencyIsStable(field.kind)
  elif mojom.IsInterfaceKind(kind):
    for method in kind.methods:
      for param in method.param_struct.fields:
        assertDependencyIsStable(param.kind)
      if method.response_param_struct:
        for response_param in method.response_param_struct.fields:
          assertDependencyIsStable(response_param.kind)


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
      _Import(module, imports[imp.import_filename]) for imp in tree.import_list
  ]
  if tree.module and tree.module.attribute_list:
    assert isinstance(tree.module.attribute_list, ast.AttributeList)
    # TODO(vtl): Check for duplicate keys here.
    module.attributes = dict((attribute.key, attribute.value)
                             for attribute in tree.module.attribute_list)

  filename = os.path.basename(path)
  # First pass collects kinds.
  module.constants = []
  module.enums = []
  module.structs = []
  module.unions = []
  module.interfaces = []
  _ProcessElements(
      filename, tree.definition_list, {
          ast.Const:
          lambda const: module.constants.append(_Constant(module, const, None)),
          ast.Enum:
          lambda enum: module.enums.append(_Enum(module, enum, None)),
          ast.Struct:
          lambda struct: module.structs.append(_Struct(module, struct)),
          ast.Union:
          lambda union: module.unions.append(_Union(module, union)),
          ast.Interface:
          lambda interface: module.interfaces.append(
              _Interface(module, interface)),
      })

  # Second pass expands fields and methods. This allows fields and parameters
  # to refer to kinds defined anywhere in the mojom.
  all_defined_kinds = {}
  for struct in module.structs:
    struct.fields = list(
        map(lambda field: _StructField(module, field, struct),
            struct.fields_data))
    _AssignDefaultOrdinals(struct.fields)
    del struct.fields_data
    all_defined_kinds[struct.spec] = struct
    for enum in struct.enums:
      all_defined_kinds[enum.spec] = enum

  for union in module.unions:
    union.fields = list(
        map(lambda field: _UnionField(module, field, union), union.fields_data))
    _AssignDefaultOrdinals(union.fields)
    del union.fields_data
    all_defined_kinds[union.spec] = union

  for interface in module.interfaces:
    interface.methods = list(
        map(lambda method: _Method(module, method, interface),
            interface.methods_data))
    _AssignDefaultOrdinals(interface.methods)
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
  module.imported_kinds = dict(
      (spec, all_referenced_kinds[spec]) for spec in imported_kind_specs)

  generator.AddComputedData(module)
  for iface in module.interfaces:
    for method in iface.methods:
      if method.param_struct:
        _AssignDefaultOrdinals(method.param_struct.fields)
      if method.response_param_struct:
        _AssignDefaultOrdinals(method.response_param_struct.fields)

  # Ensure that all types marked [Stable] are actually stable. Enums are
  # automatically OK since they don't depend on other definitions.
  for kinds in (module.structs, module.unions, module.interfaces):
    for kind in kinds:
      if kind.stable:
        _AssertTypeIsStable(kind)

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
