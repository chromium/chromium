# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates TypeScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
import itertools
import os
import sys
import urllib.request
from mojom.generate.template_expander import UseJinja
from pathlib import Path

_kind_to_javascript_default_value = {
    mojom.BOOL: "false",
    mojom.INT8: "0",
    mojom.UINT8: "0",
    mojom.INT16: "0",
    mojom.UINT16: "0",
    mojom.INT32: "0",
    mojom.UINT32: "0",
    mojom.FLOAT: "0",
    mojom.HANDLE: "null",
    mojom.DCPIPE: "null",
    mojom.DPPIPE: "null",
    mojom.MSGPIPE: "null",
    mojom.SHAREDBUFFER: "null",
    mojom.PLATFORMHANDLE: "null",
    mojom.NULLABLE_HANDLE: "null",
    mojom.NULLABLE_DCPIPE: "null",
    mojom.NULLABLE_DPPIPE: "null",
    mojom.NULLABLE_MSGPIPE: "null",
    mojom.NULLABLE_SHAREDBUFFER: "null",
    mojom.NULLABLE_PLATFORMHANDLE: "null",
    mojom.INT64: "0",
    mojom.UINT64: "0",
    mojom.DOUBLE: "0",
    mojom.STRING: "null",
    mojom.NULLABLE_STRING: "null"
}

_kind_to_ts_type = {
    mojom.BOOL: "boolean",
    mojom.INT8: "number",
    mojom.UINT8: "number",
    mojom.INT16: "number",
    mojom.UINT16: "number",
    mojom.INT32: "number",
    mojom.UINT32: "number",
    mojom.FLOAT: "number",
    mojom.INT64: "bigint",
    mojom.UINT64: "bigint",
    mojom.DOUBLE: "number",
    mojom.NULLABLE_BOOL: "boolean",
    mojom.NULLABLE_INT8: "number",
    mojom.NULLABLE_UINT8: "number",
    mojom.NULLABLE_INT16: "number",
    mojom.NULLABLE_UINT16: "number",
    mojom.NULLABLE_INT32: "number",
    mojom.NULLABLE_UINT32: "number",
    mojom.NULLABLE_FLOAT: "number",
    mojom.NULLABLE_INT64: "bigint",
    mojom.NULLABLE_UINT64: "bigint",
    mojom.NULLABLE_DOUBLE: "number",
    mojom.STRING: "string",
    mojom.NULLABLE_STRING: "string",
    mojom.HANDLE: "MojoHandle",
    mojom.DCPIPE: "MojoHandle",
    mojom.DPPIPE: "MojoHandle",
    mojom.MSGPIPE: "MojoHandle",
    mojom.SHAREDBUFFER: "MojoHandle",
    mojom.PLATFORMHANDLE: "MojoHandle",
    mojom.NULLABLE_HANDLE: "MojoHandle",
    mojom.NULLABLE_DCPIPE: "MojoHandle",
    mojom.NULLABLE_DPPIPE: "MojoHandle",
    mojom.NULLABLE_MSGPIPE: "MojoHandle",
    mojom.NULLABLE_SHAREDBUFFER: "MojoHandle",
    mojom.NULLABLE_PLATFORMHANDLE: "MojoHandle",
}

_kind_to_lite_js_type = {
    mojom.BOOL: "mojo.internal.Bool",
    mojom.INT8: "mojo.internal.Int8",
    mojom.UINT8: "mojo.internal.Uint8",
    mojom.INT16: "mojo.internal.Int16",
    mojom.UINT16: "mojo.internal.Uint16",
    mojom.INT32: "mojo.internal.Int32",
    mojom.UINT32: "mojo.internal.Uint32",
    mojom.FLOAT: "mojo.internal.Float",
    mojom.NULLABLE_BOOL: "mojo.internal.Bool",
    mojom.NULLABLE_INT8: "mojo.internal.Int8",
    mojom.NULLABLE_UINT8: "mojo.internal.Uint8",
    mojom.NULLABLE_INT16: "mojo.internal.Int16",
    mojom.NULLABLE_UINT16: "mojo.internal.Uint16",
    mojom.NULLABLE_INT32: "mojo.internal.Int32",
    mojom.NULLABLE_UINT32: "mojo.internal.Uint32",
    mojom.NULLABLE_FLOAT: "mojo.internal.Float",
    mojom.HANDLE: "mojo.internal.Handle",
    mojom.DCPIPE: "mojo.internal.Handle",
    mojom.DPPIPE: "mojo.internal.Handle",
    mojom.MSGPIPE: "mojo.internal.Handle",
    mojom.SHAREDBUFFER: "mojo.internal.Handle",
    mojom.PLATFORMHANDLE: "mojo.internal.Handle",
    mojom.NULLABLE_HANDLE: "mojo.internal.Handle",
    mojom.NULLABLE_DCPIPE: "mojo.internal.Handle",
    mojom.NULLABLE_DPPIPE: "mojo.internal.Handle",
    mojom.NULLABLE_MSGPIPE: "mojo.internal.Handle",
    mojom.NULLABLE_SHAREDBUFFER: "mojo.internal.Handle",
    mojom.NULLABLE_PLATFORMHANDLE: "mojo.internal.Handle",
    mojom.INT64: "mojo.internal.Int64",
    mojom.UINT64: "mojo.internal.Uint64",
    mojom.DOUBLE: "mojo.internal.Double",
    mojom.NULLABLE_INT64: "mojo.internal.Int64",
    mojom.NULLABLE_UINT64: "mojo.internal.Uint64",
    mojom.NULLABLE_DOUBLE: "mojo.internal.Double",
    mojom.STRING: "mojo.internal.String",
    mojom.NULLABLE_STRING: "mojo.internal.String",
}

_js_reserved_keywords = [
    'arguments',
    'await',
    'break',
    'case',
    'catch',
    'class',
    'const',
    'continue',
    'debugger',
    'default',
    'delete',
    'do',
    'else',
    'enum',
    'export',
    'extends',
    'finally',
    'for',
    'function',
    'if',
    'implements',
    'import',
    'in',
    'instanceof',
    'interface',
    'let',
    'new',
    'package',
    'private',
    'protected',
    'public',
    'return',
    'static',
    'super',
    'switch',
    'this',
    'throw',
    'try',
    'typeof',
    'var',
    'void',
    'while',
    'with',
    'yield',
]

_CHROME_SCHEME_PREFIX = 'chrome:'
_SHARED_MODULE_PREFIX = '//resources/mojo'


def _IsSharedModulePath(path):
  return path.startswith(_SHARED_MODULE_PREFIX) or \
      path.startswith(_CHROME_SCHEME_PREFIX + _SHARED_MODULE_PREFIX)


def _IsAbsoluteChromeResourcesPath(path):
  return path.startswith('chrome://resources/') or \
      path.startswith('//resources/')


def _GetWebUiModulePath(module):
  """Returns the path to a WebUI module, from the perspective of a WebUI page
  that makes it available. This is based on the corresponding mojom target's
  webui_module_path value. Returns None if the target specifies no module
  path. Otherwise, returned paths always end in a '/' and begin with either
  `chrome://resources/` or a '/'."""
  path = module.metadata.get('webui_module_path')
  if path is None:
    return None
  if path == '' or path == '/':
    return '/'
  if _IsAbsoluteChromeResourcesPath(path):
    return path.rstrip('/') + '/'
  return '/{}/'.format(path.strip('/'))


class TypeScriptStylizer(generator.Stylizer):
  def StylizeConstant(self, mojom_name):
    return generator.ToUpperSnakeCase(mojom_name)

  def StylizeField(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeStruct(self, mojom_name):
    return mojom_name

  def StylizeUnion(self, mojom_name):
    return mojom_name

  def StylizeParameter(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeMethod(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeEnumField(self, mojom_name):
    return mojom_name

  def StylizeEnum(self, mojom_name):
    return mojom_name

  def StylizeModule(self, mojom_namespace):
    return '.'.join(
        generator.ToCamel(word, lower_initial=True)
        for word in mojom_namespace.split('.'))


class Generator(generator.Generator):
  def _GetParameters(self):
    return {
        "bindings_library_path": self._GetBindingsLibraryPath(),
        "enums": self.module.enums,
        "for_bindings_internals": self.disallow_native_types,
        "interfaces": self.module.interfaces,
        "js_module_imports": self._GetJsModuleImports(),
        "kinds": self.module.kinds,
        "module": self.module,
        "module_filename": Path(self._GetModuleFilename(filetype='js')).name,
        "converters_filename":
        Path(self._GetConvertersFilename(filetype='js')).name,
        "mojom_namespace": self.module.mojom_namespace,
        "structs": self.module.structs + self._GetStructsFromMethods(),
        "unions": self.module.unions,
        "generate_struct_deserializers": self.js_generate_struct_deserializers,
        "typemapped_structs": self._TypeMappedStructs(),
        "typemap_imports": self._TypeMapImports(),
        "converter_imports": self._ConverterImports(),
    }

  @staticmethod
  def GetTemplatePrefix():
    return "ts_templates"

  def GetFilters(self):
    ts_filters = {
        "is_nullable_value_kind_packed_field":
        pack.IsNullableValueKindPackedField,
        "is_primary_nullable_value_kind_packed_field":
        pack.IsPrimaryNullableValueKindPackedField,
        "constant_value": self._GetConstantValue,
        "default_ts_value": self._GetDefaultValue,
        "imports_for_kind": self._GetImportsForKind,
        "is_bool_kind": mojom.IsBoolKind,
        "spec_type": self._GetSpecType,
        "ts_type": self._TypescriptType,
        "ts_type_maybe_nullable": self._TypescriptTypeMaybeNullable,
        "sanitize_identifier": self._TypeScriptSanitizeIdentifier,
    }
    return ts_filters

  @UseJinja("converter_interface_declarations.tmpl")
  def _GenerateConverterInterfaces(self):
    return self._GetParameters()

  @UseJinja("module_definition.tmpl")
  def _GenerateWebUiModule(self):
    return self._GetParameters()

  def _GetModuleFilename(self, filetype='ts'):
    return f"{self.module.path}-webui.{filetype}"

  def _GetConvertersFilename(self, filetype='ts'):
    return f"{self.module.path}-converters.{filetype}"

  def GenerateFiles(self, args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    self.module.Stylize(TypeScriptStylizer())

    # TODO(crbug.com/41361453): Change the media router extension to not mess
    # with the mojo namespace, so that namespaces such as "mojo.common.mojom"
    # are not affected and we can remove this method.
    self._SetUniqueNameForImports()

    assert(_GetWebUiModulePath(self.module) is not None)
    self.WriteWithComment(self._GenerateWebUiModule(),
                          self._GetModuleFilename())
    self.WriteWithComment(self._GenerateConverterInterfaces(),
                          self._GetConvertersFilename())


  def _GetBindingsLibraryPath(self):
    return "//resources/mojo/mojo/public/js/bindings.js"

  def _SetUniqueNameForImports(self):
    used_names = set()
    for each_import in self.module.imports:
      simple_name = os.path.basename(each_import.path).split(".")[0]

      # Since each import is assigned a variable in JS, they need to have unique
      # names.
      unique_name = simple_name
      counter = 0
      while unique_name in used_names:
        counter += 1
        unique_name = simple_name + str(counter)

      used_names.add(unique_name)
      each_import.unique_name = unique_name + "$"
      counter += 1

  def _IsStringableKind(self, kind):
    # Indicates whether a kind of suitable to stringify and use as an Object
    # property name. This is checked for map key types to allow most kinds of
    # mojom maps to be represented as either a Map or an Object.
    if kind == mojom.INT64 or kind == mojom.UINT64:
      # JS BigInts are not stringable and cannot be used as Object property
      # names.
      return False
    return (mojom.IsIntegralKind(kind) or mojom.IsFloatKind(kind)
            or mojom.IsDoubleKind(kind) or mojom.IsStringKind(kind))

  def _TypescriptType(self, kind, maybe_nullable=False):
    typemap = self._TypeMappedStructs()

    def recurse_nullable(kind):
      return self._TypescriptType(kind, maybe_nullable=True)

    def get_type_name(kind):
      if self._IsPrimitiveKind(kind):
        return _kind_to_ts_type[kind]

      if mojom.IsArrayKind(kind):
        if (mojom.IsNullableKind(kind.kind)):
          return "Array<%s>" % recurse_nullable(kind.kind)
        else:
          return "%s[]" % get_type_name(kind.kind)

      if (mojom.IsMapKind(kind) and not mojom.IsNullableKind(kind.key_kind)):
        if mojom.IsEnumKind(kind.key_kind):
          return "{[key in %s]?: %s}" % (get_type_name(
              kind.key_kind), recurse_nullable(kind.value_kind))
        if self._IsStringableKind(kind.key_kind):
          return "{[key: %s]: %s}" % (get_type_name(
              kind.key_kind), recurse_nullable(kind.value_kind))

      if mojom.IsMapKind(kind):
        return "Map<%s, %s>" % (recurse_nullable(
            kind.key_kind), recurse_nullable(kind.value_kind))

      if (mojom.IsAssociatedKind(kind) or mojom.IsPendingRemoteKind(kind)
          or mojom.IsPendingReceiverKind(kind)
          or mojom.IsPendingAssociatedRemoteKind(kind)
          or mojom.IsPendingAssociatedReceiverKind(kind)):
        named_kind = kind.kind
      else:
        named_kind = kind

      name = []
      qualified = self.module is not named_kind.module
      if qualified and named_kind.module:
        name.append(named_kind.module.namespace)
      if named_kind.parent_kind:
        name.append(named_kind.parent_kind.name)

      if mojom.IsEnumKind(kind) and named_kind.parent_kind:
        name = "_".join(name)
        name += "_" + named_kind.name
      else:
        name.append("" + named_kind.name)
        name = "_".join(name)
      name = name.replace('.', '_')

      if mojom.IsStructKind(kind):
        if kind.qualified_name in self.typemap:
          return self.typemap[kind.qualified_name]['typename']
        return name
      if mojom.IsUnionKind(kind) or mojom.IsEnumKind(kind):
        return name
      if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
        return name + "Remote"
      if mojom.IsPendingReceiverKind(kind):
        return name + "PendingReceiver"
      # TODO(calamity): Support associated interfaces properly.
      if mojom.IsPendingAssociatedRemoteKind(kind):
        return "object"
      # TODO(calamity): Support associated interface requests properly.
      if mojom.IsPendingAssociatedReceiverKind(kind):
        return "object"

      raise Exception("Type is not supported yet.")

    if (maybe_nullable and mojom.IsNullableKind(kind)):
      return "(" + get_type_name(kind) + " | null)"

    return get_type_name(kind)

  def _TypescriptTypeMaybeNullable(self, kind):
    return self._TypescriptType(kind, maybe_nullable=True)

  def _GetNameInJsModule(self, kind):
    qualifier = ""
    if kind.module is not self.module and kind.module.namespace:
      qualifier = kind.module.namespace + '.'
    if kind.parent_kind:
      qualifier += kind.parent_kind.name + '.'
    return (qualifier + kind.name).replace('.', '_')

  def _GetImportsForKind(self, kind):
    qualified_name = self._GetNameInJsModule(kind)

    def make_import(name, suffix=''):
      class ImportInfo(object):
        def __init__(self, name, alias):
          self.name = name
          self.alias = alias

      return ImportInfo(name + suffix, qualified_name + suffix)

    if (mojom.IsEnumKind(kind) or mojom.IsStructKind(kind)
        or mojom.IsUnionKind(kind)):
      return [make_import(kind.name), make_import(kind.name, 'Spec')]
    if mojom.IsInterfaceKind(kind):
      # Collect referenced kinds that may refer to an interface Remote or
      # PendingReceiver.
      referenced_kinds = []
      for interface in self.module.interfaces:
        for method in interface.methods:
          referenced_kinds.extend(method.parameters or [])
          referenced_kinds.extend(method.response_parameters or [])

      # Determine whether Remote and/or PendingReceiver are referenced.
      imports = []
      imported_receiver = False
      imported_remote = False

      for referenced_kind in referenced_kinds:
        # Early return if both references have already been found.
        if imported_remote and imported_receiver:
          return imports
        if (not imported_remote
            and (mojom.IsInterfaceKind(referenced_kind.kind)
                 or mojom.IsPendingRemoteKind(referenced_kind.kind)
                 or mojom.IsPendingAssociatedRemoteKind(referenced_kind.kind))
            and referenced_kind.kind.kind == kind):
          imported_remote = True
          imports.append(make_import(kind.name, 'Remote'))
          continue
        if (not imported_receiver
            and (mojom.IsPendingReceiverKind(referenced_kind.kind)
                 or mojom.IsPendingAssociatedReceiverKind(referenced_kind.kind))
            and referenced_kind.kind.kind == kind):
          imported_receiver = True
          imports.append(make_import(kind.name, 'PendingReceiver'))
      return imports

    assert False, kind.name

  def _GetSpecType(self, kind):
    def get_spec(kind):
      if self._IsPrimitiveKind(kind):
        return _kind_to_lite_js_type[kind]
      if mojom.IsArrayKind(kind):
        return "mojo.internal.Array(%s, %s)" % (get_spec(
            kind.kind), "true" if mojom.IsNullableKind(kind.kind) else "false")
      if mojom.IsMapKind(kind):
        return "mojo.internal.Map(%s, %s, %s)" % (
            get_spec(kind.key_kind), get_spec(kind.value_kind),
            "true" if mojom.IsNullableKind(kind.value_kind) else "false")

      if (mojom.IsAssociatedKind(kind) or mojom.IsPendingRemoteKind(kind)
          or mojom.IsPendingReceiverKind(kind)
          or mojom.IsPendingAssociatedRemoteKind(kind)
          or mojom.IsPendingAssociatedReceiverKind(kind)):
        named_kind = kind.kind
      else:
        named_kind = kind

      name = []
      qualified = self.module is not named_kind.module
      if qualified and named_kind.module:
        name.append(named_kind.module.namespace)
      if named_kind.parent_kind:
        parent_name = named_kind.parent_kind.name
        name.append(parent_name)
      name.append(named_kind.name)
      name = "_".join(name)
      name = name.replace(".", "_")

      if (mojom.IsStructKind(kind) or mojom.IsUnionKind(kind)
          or mojom.IsEnumKind(kind)):
        return "%sSpec.$" % name
      if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
        return "mojo.internal.InterfaceProxy(%sRemote)" % name
      if mojom.IsPendingReceiverKind(kind):
        return "mojo.internal.InterfaceRequest(%sPendingReceiver)" % name
      if mojom.IsPendingAssociatedRemoteKind(kind):
        # TODO(rockot): Implement associated interfaces.
        return "mojo.internal.AssociatedInterfaceProxy(%sRemote)" % (name)
      if mojom.IsPendingAssociatedReceiverKind(kind):
        return "mojo.internal.AssociatedInterfaceRequest(%sPendingReceiver)" % (
            name)

      return name

    return get_spec(kind)

  def _GetDefaultValue(self, field):
    if field.default:
      if mojom.IsStructKind(field.kind):
        assert field.default == "default"
        return "null"
      if ((field.kind == mojom.INT64 or field.kind == mojom.UINT64)
          and not isinstance(
              field.default,
              (mojom.EnumValue, mojom.NamedValue, mojom.BuiltinValue))):
        return "BigInt('{}')".format(int(field.default, 0))
      return self._ExpressionToText(field.default)
    if field.kind == mojom.INT64 or field.kind == mojom.UINT64:
      return "BigInt(0)"
    if field.kind in mojom.PRIMITIVES:
      return _kind_to_javascript_default_value[field.kind]
    if mojom.IsEnumKind(field.kind):
      return "0"
    return "null"

  def _TypeScriptSanitizeIdentifier(self, identifier):
    if identifier in _js_reserved_keywords:
      return identifier + '_'

    return identifier

  def _ExpressionOrConstantToText(self, token):
    if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
      # Both variable and enum constants are constructed like:
      # NamespaceUid.Struct[.Enum].CONSTANT_NAME
      name = []
      if token.module and token.module.path != self.module.path:
        name.append(token.module.unique_name)
      if token.parent_kind:
        name.append(token.parent_kind.name)
      if isinstance(token, mojom.EnumValue):
        name.append(token.enum.name)
      name.append(token.name)
      return ".".join(name)

    if isinstance(token, mojom.BuiltinValue):
      if token.value == "double.INFINITY" or token.value == "float.INFINITY":
        return "Infinity"
      if token.value == "double.NEGATIVE_INFINITY" or \
         token.value == "float.NEGATIVE_INFINITY":
        return "-Infinity"
      if token.value == "double.NAN" or token.value == "float.NAN":
        return "NaN"

    return token

  def _ExpressionToText(self, token):
    if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
      # Generate the following for:
      #  - Enums: NamespaceUid.Enum.CONSTANT_NAME
      #  - Struct: NamespaceUid.Struct_CONSTANT_NAME

      namespace_components = []
      qualified = token.module is not self.module
      if token.module and qualified:
        namespace_components.append(token.module.namespace)
      if token.parent_kind:
        namespace_components.append(token.parent_kind.name)
      name_prefix = '.'.join(namespace_components)
      name_prefix = name_prefix.replace('.', '_')

      name = []
      if isinstance(token, mojom.EnumValue):
        name.append(token.enum.name)
      name.append(token.name)

      if len(name_prefix) > 0:
        return f"{name_prefix}_{'.'.join(name)}"

      return ".".join(name)

    return self._ExpressionOrConstantToText(token)

  def _GetConstantValue(self, constant):
    assert isinstance(constant, mojom.Constant)
    text = self._ExpressionOrConstantToText(constant.value)
    if constant.kind == mojom.INT64 or constant.kind == mojom.UINT64:
      return "BigInt('{}')".format(int(text, 0))
    return text

  def _GetJsModuleImports(self):
    this_module_path = _GetWebUiModulePath(self.module)
    this_module_is_shared = bool(this_module_path
                                 and _IsSharedModulePath(this_module_path))
    imports = dict()

    def strip_prefix(s, prefix):
      if s.startswith(prefix):
        return s[len(prefix):]
      return s

    for spec, kind in self.module.imported_kinds.items():
      assert this_module_path is not None
      base_path = _GetWebUiModulePath(kind.module)
      assert base_path is not None, \
          "No WebUI bindings found for dependency {0!r} imported by " \
              "{1!r}".format(kind.module, self.module)
      import_path = '{}{}-webui.js'.format(base_path,
                                           os.path.basename(kind.module.path))

      import_module_is_shared = _IsSharedModulePath(import_path)
      if this_module_is_shared:
        assert import_module_is_shared, \
            'Shared WebUI module "{}" cannot depend on non-shared WebUI ' \
                'module "{}"'.format(self.module.path, kind.module.path)

      # Some Mojo JS files are served from chrome://resources/, but not from
      # chrome://resources/mojo/, for example from
      # chrome://resources/cr_components/. Need to use absolute paths when
      # referring to such files from other modules, so that TypeScript can
      # correctly resolve them since they belong to a different ts_library()
      # target compared to |this_module_path|.
      import_module_is_in_chrome_resources = _IsAbsoluteChromeResourcesPath(
          import_path)
      use_absolute_path = import_module_is_in_chrome_resources and not \
              import_module_is_shared

      # Either we're a non-shared resource importing another non-shared
      # resource, or we're a shared resource importing another shared
      # resource. In both cases, we assume a relative import path will
      # suffice.
      use_relative_path = not use_absolute_path and \
              import_module_is_shared == this_module_is_shared

      if use_relative_path:
        import_path = urllib.request.pathname2url(
            os.path.relpath(
                strip_prefix(strip_prefix(import_path, _CHROME_SCHEME_PREFIX),
                             _SHARED_MODULE_PREFIX),
                strip_prefix(
                    strip_prefix(this_module_path, _CHROME_SCHEME_PREFIX),
                    _SHARED_MODULE_PREFIX)))
        if (not import_path.startswith('.')
            and not import_path.startswith('/')):
          import_path = './' + import_path
      else:
        # Import absolute imports from scheme-relative paths.
        import_path = strip_prefix(import_path, _CHROME_SCHEME_PREFIX)

      if import_path not in imports:
        imports[import_path] = []
      imports[import_path].append(kind)
    return imports

  def _GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(method.param_struct)
        if method.response_param_struct is not None:
          result.append(method.response_param_struct)
    return result

  def _IsPrimitiveKind(self, kind):
    return kind in mojom.PRIMITIVES

  def _TypeMappedStructs(self):
    if len(self.typemap) == 0:
      return {}

    mapped_structs = {}
    for struct in self.module.structs:
      if struct.qualified_name in self.typemap:
        mapped_structs[struct] = self.typemap[struct.qualified_name]
    return mapped_structs

  # Returns a list of imports in the format:
  #   {
  #      <import path>: [list of types],
  #      ...
  #   }
  def _TypeMapImports(self):
    imports = {}
    typemaps = self._TypeMappedStructs()

    for typemap in typemaps.values():
      type_import = typemap['type_import']
      # The typemapping could just be a native type, in which case there would
      # not be any import.
      if not type_import:
        continue

      imports.setdefault(type_import, []).append(typemap['typename'])

    return imports

  # Returns a list of imports in the format:
  #   {
  #      <import path>: [list of types],
  #      ...
  #   }
  def _ConverterImports(self):

    def needs_import(kind):
      return mojom.IsStructKind(kind) or mojom.IsUnionKind(kind)

    class Import:

      def __init__(self, typename, path, alias=None):
        self.typename = typename
        self.path = path
        self.alias = alias

      def import_name(self):
        if self.alias:
          return f"{self.typename} as {self.alias}"
        return self.typename

    # A dictionary keyed by the qualified type name to an import configuration.
    qualified_type_to_import = {}

    # Build up our import repository.
    # Add the mojom module imports first.
    for import_path, kinds in self._GetJsModuleImports().items():
      for kind in kinds:
        if needs_import(kind):
          qualified_type_to_import[kind.qualified_name] = Import(
              kind.name, import_path, self._GetNameInJsModule(kind))

    # Then add the typemap imports.
    for qualified_name, typemap in self.typemap.items():
      qualified_type_to_import[qualified_name] = Import(typemap['typename'],
                                                        typemap['type_import'])

    # Now we create the list of imports, based on the struct deps.
    imports = {}
    for struct in self._TypeMappedStructs():
      for field in struct.fields:
        if needs_import(field.kind):
          qualified = field.kind.qualified_name
          type_import = qualified_type_to_import[qualified]
          # We should have an entry for all non-primitive types
          assert type_import != None

          imports.setdefault(type_import.path,
                             []).append(type_import.import_name())

    return imports
