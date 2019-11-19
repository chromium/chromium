# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
import os
import urllib
from mojom.generate.template_expander import UseJinja

_kind_to_javascript_default_value = {
  mojom.BOOL:                  "false",
  mojom.INT8:                  "0",
  mojom.UINT8:                 "0",
  mojom.INT16:                 "0",
  mojom.UINT16:                "0",
  mojom.INT32:                 "0",
  mojom.UINT32:                "0",
  mojom.FLOAT:                 "0",
  mojom.HANDLE:                "null",
  mojom.DCPIPE:                "null",
  mojom.DPPIPE:                "null",
  mojom.MSGPIPE:               "null",
  mojom.SHAREDBUFFER:          "null",
  mojom.NULLABLE_HANDLE:       "null",
  mojom.NULLABLE_DCPIPE:       "null",
  mojom.NULLABLE_DPPIPE:       "null",
  mojom.NULLABLE_MSGPIPE:      "null",
  mojom.NULLABLE_SHAREDBUFFER: "null",
  mojom.INT64:                 "0",
  mojom.UINT64:                "0",
  mojom.DOUBLE:                "0",
  mojom.STRING:                "null",
  mojom.NULLABLE_STRING:       "null"
}

_kind_to_codec_type = {
  mojom.BOOL:                  "codec.Uint8",
  mojom.INT8:                  "codec.Int8",
  mojom.UINT8:                 "codec.Uint8",
  mojom.INT16:                 "codec.Int16",
  mojom.UINT16:                "codec.Uint16",
  mojom.INT32:                 "codec.Int32",
  mojom.UINT32:                "codec.Uint32",
  mojom.FLOAT:                 "codec.Float",
  mojom.HANDLE:                "codec.Handle",
  mojom.DCPIPE:                "codec.Handle",
  mojom.DPPIPE:                "codec.Handle",
  mojom.MSGPIPE:               "codec.Handle",
  mojom.SHAREDBUFFER:          "codec.Handle",
  mojom.NULLABLE_HANDLE:       "codec.NullableHandle",
  mojom.NULLABLE_DCPIPE:       "codec.NullableHandle",
  mojom.NULLABLE_DPPIPE:       "codec.NullableHandle",
  mojom.NULLABLE_MSGPIPE:      "codec.NullableHandle",
  mojom.NULLABLE_SHAREDBUFFER: "codec.NullableHandle",
  mojom.INT64:                 "codec.Int64",
  mojom.UINT64:                "codec.Uint64",
  mojom.DOUBLE:                "codec.Double",
  mojom.STRING:                "codec.String",
  mojom.NULLABLE_STRING:       "codec.NullableString",
}

_kind_to_closure_type = {
  mojom.BOOL:                  "boolean",
  mojom.INT8:                  "number",
  mojom.UINT8:                 "number",
  mojom.INT16:                 "number",
  mojom.UINT16:                "number",
  mojom.INT32:                 "number",
  mojom.UINT32:                "number",
  mojom.FLOAT:                 "number",
  mojom.INT64:                 "number",
  mojom.UINT64:                "number",
  mojom.DOUBLE:                "number",
  mojom.STRING:                "string",
  mojom.NULLABLE_STRING:       "string",
  mojom.HANDLE:                "MojoHandle",
  mojom.DCPIPE:                "MojoHandle",
  mojom.DPPIPE:                "MojoHandle",
  mojom.MSGPIPE:               "MojoHandle",
  mojom.SHAREDBUFFER:          "MojoHandle",
  mojom.NULLABLE_HANDLE:       "MojoHandle",
  mojom.NULLABLE_DCPIPE:       "MojoHandle",
  mojom.NULLABLE_DPPIPE:       "MojoHandle",
  mojom.NULLABLE_MSGPIPE:      "MojoHandle",
  mojom.NULLABLE_SHAREDBUFFER: "MojoHandle",
}

_kind_to_lite_js_type = {
  mojom.BOOL:                  "mojo.internal.Bool",
  mojom.INT8:                  "mojo.internal.Int8",
  mojom.UINT8:                 "mojo.internal.Uint8",
  mojom.INT16:                 "mojo.internal.Int16",
  mojom.UINT16:                "mojo.internal.Uint16",
  mojom.INT32:                 "mojo.internal.Int32",
  mojom.UINT32:                "mojo.internal.Uint32",
  mojom.FLOAT:                 "mojo.internal.Float",
  mojom.HANDLE:                "mojo.internal.Handle",
  mojom.DCPIPE:                "mojo.internal.Handle",
  mojom.DPPIPE:                "mojo.internal.Handle",
  mojom.MSGPIPE:               "mojo.internal.Handle",
  mojom.SHAREDBUFFER:          "mojo.internal.Handle",
  mojom.NULLABLE_HANDLE:       "mojo.internal.Handle",
  mojom.NULLABLE_DCPIPE:       "mojo.internal.Handle",
  mojom.NULLABLE_DPPIPE:       "mojo.internal.Handle",
  mojom.NULLABLE_MSGPIPE:      "mojo.internal.Handle",
  mojom.NULLABLE_SHAREDBUFFER: "mojo.internal.Handle",
  mojom.INT64:                 "mojo.internal.Int64",
  mojom.UINT64:                "mojo.internal.Uint64",
  mojom.DOUBLE:                "mojo.internal.Double",
  mojom.STRING:                "mojo.internal.String",
  mojom.NULLABLE_STRING:       "mojo.internal.String",
}

_js_reserved_keywords = [
    'arguments',
    'await',
    'break'
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

_primitive_kind_to_fuzz_type = {
  mojom.BOOL:                  "Bool",
  mojom.INT8:                  "Int8",
  mojom.UINT8:                 "Uint8",
  mojom.INT16:                 "Int16",
  mojom.UINT16:                "Uint16",
  mojom.INT32:                 "Int32",
  mojom.UINT32:                "Uint32",
  mojom.FLOAT:                 "Float",
  mojom.INT64:                 "Int64",
  mojom.UINT64:                "Uint64",
  mojom.DOUBLE:                "Double",
  mojom.STRING:                "String",
  mojom.NULLABLE_STRING:       "String",
  mojom.HANDLE:                "Handle",
  mojom.DCPIPE:                "DataPipeConsumer",
  mojom.DPPIPE:                "DataPipeProducer",
  mojom.MSGPIPE:               "MessagePipe",
  mojom.SHAREDBUFFER:          "SharedBuffer",
  mojom.NULLABLE_HANDLE:       "Handle",
  mojom.NULLABLE_DCPIPE:       "DataPipeConsumer",
  mojom.NULLABLE_DPPIPE:       "DataPipeProducer",
  mojom.NULLABLE_MSGPIPE:      "MessagePipe",
  mojom.NULLABLE_SHAREDBUFFER: "SharedBuffer",
}


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = pack.GetPad(offset, 8)
  return offset + pad


def JavaScriptFieldOffset(packed_field):
  return "offset + codec.kStructHeaderSize + %s" % packed_field.offset


def GetArrayExpectedDimensionSizes(kind):
  expected_dimension_sizes = []
  while mojom.IsArrayKind(kind):
    expected_dimension_sizes.append(generator.ExpectedArraySize(kind) or 0)
    kind = kind.kind
  # Strings are serialized as variable-length arrays.
  if (mojom.IsStringKind(kind)):
    expected_dimension_sizes.append(0)
  return expected_dimension_sizes


def GetRelativeUrl(module, base_module):
  return urllib.pathname2url(
      os.path.relpath(module.path, os.path.dirname(base_module.path)))


class JavaScriptStylizer(generator.Stylizer):
  def StylizeConstant(self, mojom_name):
    return generator.ToConstantCase(mojom_name)

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
    return '.'.join(generator.ToCamel(word, lower_initial=True)
                        for word in mojom_namespace.split('.'))


class Generator(generator.Generator):
  def _GetParameters(self, for_compile=False):
    return {
      "enums": self.module.enums,
      "html_imports": self._GenerateHtmlImports(),
      "imports": self.module.imports,
      "interfaces": self.module.interfaces,
      "kinds": self.module.kinds,
      "module": self.module,
      "mojom_filename": os.path.basename(self.module.path),
      "mojom_namespace": self.module.mojom_namespace,
      "structs": self.module.structs + self._GetStructsFromMethods(),
      "unions": self.module.unions,
      "generate_fuzzing": self.generate_fuzzing,
      "generate_closure_exports": for_compile,
      "generate_struct_deserializers": self.js_generate_struct_deserializers,
   }

  @staticmethod
  def GetTemplatePrefix():
    return "js_templates"

  def GetFilters(self):
    js_filters = {
      "closure_type": self._ClosureType,
      "decode_snippet": self._JavaScriptDecodeSnippet,
      "default_value": self._JavaScriptDefaultValue,
      "encode_snippet": self._JavaScriptEncodeSnippet,
      "expression_to_text": self._ExpressionToText,
      "expression_to_text_lite": self._ExpressionToTextLite,
      "field_offset": JavaScriptFieldOffset,
      "get_relative_url": GetRelativeUrl,
      "has_callbacks": mojom.HasCallbacks,
      "is_any_handle_or_interface_kind": mojom.IsAnyHandleOrInterfaceKind,
      "is_array_kind": mojom.IsArrayKind,
      "is_associated_interface_kind": mojom.IsAssociatedInterfaceKind,
      "is_pending_associated_remote_kind": mojom.IsPendingAssociatedRemoteKind,
      "is_associated_interface_request_kind":
          mojom.IsAssociatedInterfaceRequestKind,
      "is_pending_associated_receiver_kind":
          mojom.IsPendingAssociatedReceiverKind,
      "is_bool_kind": mojom.IsBoolKind,
      "is_enum_kind": mojom.IsEnumKind,
      "is_any_handle_kind": mojom.IsAnyHandleKind,
      "is_any_interface_kind": mojom.IsAnyInterfaceKind,
      "is_interface_kind": mojom.IsInterfaceKind,
      "is_pending_remote_kind": mojom.IsPendingRemoteKind,
      "is_interface_request_kind": mojom.IsInterfaceRequestKind,
      "is_pending_receiver_kind": mojom.IsPendingReceiverKind,
      "is_map_kind": mojom.IsMapKind,
      "is_object_kind": mojom.IsObjectKind,
      "is_reference_kind": mojom.IsReferenceKind,
      "is_string_kind": mojom.IsStringKind,
      "is_struct_kind": mojom.IsStructKind,
      "is_union_kind": mojom.IsUnionKind,
      "js_type": self._JavaScriptType,
      "lite_default_value": self._LiteJavaScriptDefaultValue,
      "lite_js_type": self._LiteJavaScriptType,
      "lite_js_import_name": self._LiteJavaScriptImportName,
      "method_passes_associated_kinds": mojom.MethodPassesAssociatedKinds,
      "namespace_declarations": self._NamespaceDeclarations,
      "closure_type_with_nullability": self._ClosureTypeWithNullability,
      "lite_closure_param_type": self._LiteClosureParamType,
      "lite_closure_type": self._LiteClosureType,
      "lite_closure_type_with_nullability":
          self._LiteClosureTypeWithNullability,
      "lite_closure_field_type": self._LiteClosureFieldType,
      "payload_size": JavaScriptPayloadSize,
      "to_camel": generator.ToCamel,
      "union_decode_snippet": self._JavaScriptUnionDecodeSnippet,
      "union_encode_snippet": self._JavaScriptUnionEncodeSnippet,
      "validate_array_params": self._JavaScriptValidateArrayParams,
      "validate_enum_params": self._JavaScriptValidateEnumParams,
      "validate_map_params": self._JavaScriptValidateMapParams,
      "validate_nullable_params": self._JavaScriptNullableParam,
      "validate_struct_params": self._JavaScriptValidateStructParams,
      "validate_union_params": self._JavaScriptValidateUnionParams,
      "sanitize_identifier": self._JavaScriptSanitizeIdentifier,
      "contains_handles_or_interfaces": mojom.ContainsHandlesOrInterfaces,
      "fuzz_handle_name": self._FuzzHandleName,
      "is_primitive_kind": self._IsPrimitiveKind,
      "primitive_to_fuzz_type": self._PrimitiveToFuzzType,
      "to_js_boolean": self._ToJsBoolean,
    }
    return js_filters

  @UseJinja("module.amd.tmpl")
  def _GenerateAMDModule(self):
    return self._GetParameters()

  @UseJinja("externs/module.externs.tmpl")
  def _GenerateExterns(self):
    return self._GetParameters()

  @UseJinja("lite/mojom.html.tmpl")
  def _GenerateLiteHtml(self):
    return self._GetParameters()

  @UseJinja("lite/mojom-lite.js.tmpl")
  def _GenerateLiteBindings(self):
    return self._GetParameters()

  @UseJinja("lite/mojom-lite.js.tmpl")
  def _GenerateLiteBindingsForCompile(self):
    return self._GetParameters(for_compile=True)

  def GenerateFiles(self, args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    self.module.Stylize(JavaScriptStylizer())

    # TODO(crbug.com/795977): Change the media router extension to not mess with
    # the mojo namespace, so that namespaces such as "mojo.common.mojom" are not
    # affected and we can remove this method.
    self._SetUniqueNameForImports()

    self.WriteWithComment(self._GenerateAMDModule(), "%s.js" % self.module.path)
    self.WriteWithComment(self._GenerateExterns(),
                          "%s.externs.js" % self.module.path)
    if self.js_bindings_mode == "new":
      self.WriteWithComment(self._GenerateLiteHtml(),
                            "%s.html" % self.module.path)
      self.WriteWithComment(self._GenerateLiteBindings(),
                            "%s-lite.js" % self.module.path)
      self.WriteWithComment(self._GenerateLiteBindingsForCompile(),
                            "%s-lite-for-compile.js" % self.module.path)

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

  def _ClosureType(self, kind):
    if kind in mojom.PRIMITIVES:
      return _kind_to_closure_type[kind]
    if mojom.IsInterfaceKind(kind):
      return kind.module.namespace + "." + kind.name + "Ptr"
    if mojom.IsPendingRemoteKind(kind):
      return kind.kind.module.namespace + "." + kind.kind.name + "Ptr"
    if (mojom.IsStructKind(kind) or
        mojom.IsEnumKind(kind)):
      return kind.module.namespace + "." + kind.name
    # TODO(calamity): Support unions properly.
    if mojom.IsUnionKind(kind):
      return "Object"
    if mojom.IsArrayKind(kind):
      return "Array<%s>" % self._ClosureType(kind.kind)
    if mojom.IsMapKind(kind):
      return "Map<%s, %s>" % (
          self._ClosureType(kind.key_kind), self._ClosureType(kind.value_kind))
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return "mojo.InterfaceRequest"
    # TODO(calamity): Support associated interfaces properly.
    if (mojom.IsAssociatedInterfaceKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind)):
      return "mojo.AssociatedInterfacePtrInfo"
    # TODO(calamity): Support associated interface requests properly.
    if (mojom.IsAssociatedInterfaceRequestKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      return "mojo.AssociatedInterfaceRequest"
    # TODO(calamity): Support enums properly.

    raise Exception("No valid closure type: %s" % kind)

  def _IsStringableKind(self, kind):
    # Indicates whether a kind of suitable to stringify and use as an Object
    # property name. This is checked for map key types to allow most kinds of
    # mojom maps to be represented as either a Map or an Object.
    return (mojom.IsIntegralKind(kind) or mojom.IsFloatKind(kind) or
        mojom.IsDoubleKind(kind) or mojom.IsStringKind(kind) or
        mojom.IsEnumKind(kind))

  def _LiteClosureType(self, kind):
    if kind in mojom.PRIMITIVES:
      return _kind_to_closure_type[kind]
    if mojom.IsArrayKind(kind):
      return "Array<%s>" % self._LiteClosureTypeWithNullability(kind.kind)
    if mojom.IsMapKind(kind) and self._IsStringableKind(kind.key_kind):
      return "Object<%s, %s>" % (
          self._LiteClosureTypeWithNullability(kind.key_kind),
          self._LiteClosureTypeWithNullability(kind.value_kind))
    if mojom.IsMapKind(kind):
      return "Map<%s, %s>" % (
          self._LiteClosureTypeWithNullability(kind.key_kind),
          self._LiteClosureTypeWithNullability(kind.value_kind))

    if (mojom.IsAssociatedKind(kind) or mojom.IsInterfaceRequestKind(kind) or
        mojom.IsPendingRemoteKind(kind) or mojom.IsPendingReceiverKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      named_kind = kind.kind
    else:
      named_kind = kind

    name = []
    if named_kind.module:
      name.append(named_kind.module.namespace)
    if named_kind.parent_kind:
      name.append(named_kind.parent_kind.name)

    if mojom.IsEnumKind(kind) and named_kind.parent_kind:
      name = ".".join(name)
      name += "_" + named_kind.name
    else:
      name.append("" + named_kind.name)
      name = ".".join(name)

    if (mojom.IsStructKind(kind) or mojom.IsUnionKind(kind) or
        mojom.IsEnumKind(kind)):
      return name
    if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
      return name + "Remote"
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return name + "PendingReceiver"
    # TODO(calamity): Support associated interfaces properly.
    if (mojom.IsAssociatedInterfaceKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind)):
      return "Object"
    # TODO(calamity): Support associated interface requests properly.
    if (mojom.IsAssociatedInterfaceRequestKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      return "Object"

    raise Exception("No valid closure type: %s" % kind)

  def _ClosureTypeWithNullability(self, kind):
    return ("" if mojom.IsNullableKind(kind) else "!") + self._ClosureType(kind)

  def _LiteClosureParamType(self, kind):
    if mojom.IsEnumKind(kind):
      return "number"
    prefix = "" if mojom.IsNullableKind(kind) else "!"
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return prefix + "Object"
    if mojom.IsArrayKind(kind):
      return prefix + ("Array<%s>" %
                       self._LiteClosureParamType(kind.kind))
    if mojom.IsMapKind(kind):
      return "%sMap<%s, %s>|%sObject<%s, %s>" % (
          prefix, self._LiteClosureParamType(kind.key_kind),
          self._LiteClosureParamType(kind.value_kind),
          prefix, self._LiteClosureParamType(kind.key_kind),
          self._LiteClosureParamType(kind.value_kind))

    return prefix + self._LiteClosureType(kind)

  def _LiteClosureTypeWithNullability(self, kind):
    return (("?" if mojom.IsNullableKind(kind) else "!") +
        self._LiteClosureType(kind))

  def _LiteClosureFieldType(self, kind):
    if mojom.IsNullableKind(kind):
      return "(" + self._LiteClosureType(kind) + "|undefined)"
    else:
      return "!" + self._LiteClosureType(kind)

  def _NamespaceDeclarations(self, namespace):
    pieces = namespace.split('.')
    declarations = []
    declaration = []
    for p in pieces:
      declaration.append(p)
      declarations.append('.'.join(declaration))
    return declarations

  def _JavaScriptType(self, kind):
    name = []
    if kind.module and kind.module.path != self.module.path:
      name.append(kind.module.unique_name)
    if kind.parent_kind:
      name.append(kind.parent_kind.name)
    name.append(kind.name)
    return ".".join(name)

  def _LiteJavaScriptType(self, kind):
    if self._IsPrimitiveKind(kind):
      return _kind_to_lite_js_type[kind]
    if mojom.IsArrayKind(kind):
      return "mojo.internal.Array(%s, %s)" % (
          self._LiteJavaScriptType(kind.kind),
          "true" if mojom.IsNullableKind(kind.kind) else "false")
    if mojom.IsMapKind(kind):
      return "mojo.internal.Map(%s, %s, %s)" % (
          self._LiteJavaScriptType(kind.key_kind),
          self._LiteJavaScriptType(kind.value_kind),
          "true" if mojom.IsNullableKind(kind.value_kind) else "false")

    if (mojom.IsAssociatedKind(kind) or mojom.IsInterfaceRequestKind(kind) or
        mojom.IsPendingRemoteKind(kind) or mojom.IsPendingReceiverKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      named_kind = kind.kind
    else:
      named_kind = kind

    name = []
    if named_kind.module:
      name.append(named_kind.module.namespace)
    if named_kind.parent_kind:
      parent_name = named_kind.parent_kind.name
      if mojom.IsStructKind(named_kind.parent_kind):
        parent_name += "Spec"
      name.append(parent_name)
    name.append(named_kind.name)
    name = ".".join(name)

    if (mojom.IsStructKind(kind) or mojom.IsUnionKind(kind) or
        mojom.IsEnumKind(kind)):
      return "%sSpec.$" % name
    if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
      return "mojo.internal.InterfaceProxy(%sRemote)" % name
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return "mojo.internal.InterfaceRequest(%sPendingReceiver)" % name
    if (mojom.IsAssociatedInterfaceKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind)):
      # TODO(rockot): Implement associated interfaces.
      return "mojo.internal.AssociatedInterfaceProxy(%sRemote)" % (name)
    if (mojom.IsAssociatedInterfaceRequestKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      return "mojo.internal.AssociatedInterfaceRequest(%sPendingReceiver)" % (
        name)

    return name

  def _LiteJavaScriptImportName(self, kind):
    name = []
    if kind.parent_kind:
      name.append(self._LiteJavaScriptImportName(kind.parent_kind))
    elif kind.module:
      name.append(kind.module.namespace)
    name.append(kind.name)
    return '.'.join(name)

  def _JavaScriptDefaultValue(self, field):
    if field.default:
      if mojom.IsStructKind(field.kind):
        assert field.default == "default"
        return "new %s()" % self._JavaScriptType(field.kind)
      return self._ExpressionToText(field.default)
    if field.kind in mojom.PRIMITIVES:
      return _kind_to_javascript_default_value[field.kind]
    if mojom.IsStructKind(field.kind):
      return "null"
    if mojom.IsUnionKind(field.kind):
      return "null"
    if mojom.IsArrayKind(field.kind):
      return "null"
    if mojom.IsMapKind(field.kind):
      return "null"
    if mojom.IsInterfaceKind(field.kind):
      return "new %sPtr()" % self._JavaScriptType(field.kind)
    if mojom.IsPendingRemoteKind(field.kind):
      return "new %sPtr()" % self._JavaScriptType(field.kind.kind)
    if (mojom.IsInterfaceRequestKind(field.kind) or
        mojom.IsPendingReceiverKind(field.kind)):
      return "new bindings.InterfaceRequest()"
    if (mojom.IsAssociatedInterfaceKind(field.kind) or
        mojom.IsPendingAssociatedRemoteKind(field.kind)):
      return "new associatedBindings.AssociatedInterfacePtrInfo()"
    if (mojom.IsAssociatedInterfaceRequestKind(field.kind) or
        mojom.IsPendingAssociatedReceiverKind(field.kind)):
      return "new associatedBindings.AssociatedInterfaceRequest()"
    if mojom.IsEnumKind(field.kind):
      return "0"
    raise Exception("No valid default: %s" % field)

  def _LiteJavaScriptDefaultValue(self, field):
    if field.default:
      if mojom.IsStructKind(field.kind):
        assert field.default == "default"
        return "null";
      return self._ExpressionToTextLite(field.default)
    if field.kind in mojom.PRIMITIVES:
      return _kind_to_javascript_default_value[field.kind]
    if mojom.IsEnumKind(field.kind):
      return "0"
    return "null"

  def _CodecType(self, kind):
    if kind in mojom.PRIMITIVES:
      return _kind_to_codec_type[kind]
    if mojom.IsStructKind(kind):
      pointer_type = "NullablePointerTo" if mojom.IsNullableKind(kind) \
          else "PointerTo"
      return "new codec.%s(%s)" % (pointer_type, self._JavaScriptType(kind))
    if mojom.IsUnionKind(kind):
      return self._JavaScriptType(kind)
    if mojom.IsArrayKind(kind):
      array_type = ("NullableArrayOf" if mojom.IsNullableKind(kind)
                                      else "ArrayOf")
      array_length = "" if kind.length is None else ", %d" % kind.length
      element_type = self._ElementCodecType(kind.kind)
      return "new codec.%s(%s%s)" % (array_type, element_type, array_length)
    if mojom.IsInterfaceKind(kind):
      return "new codec.%s(%sPtr)" % (
          "NullableInterface" if mojom.IsNullableKind(kind) else "Interface",
          self._JavaScriptType(kind))
    if mojom.IsPendingRemoteKind(kind):
      return "new codec.%s(%sPtr)" % (
          "NullableInterface" if mojom.IsNullableKind(kind) else "Interface",
          self._JavaScriptType(kind.kind))
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return "codec.%s" % (
          "NullableInterfaceRequest" if mojom.IsNullableKind(kind)
                                     else "InterfaceRequest")
    if (mojom.IsAssociatedInterfaceKind(kind) or
        mojom.IsPendingAssociatedRemoteKind(kind)):
      return "codec.%s" % ("NullableAssociatedInterfacePtrInfo"
          if mojom.IsNullableKind(kind) else "AssociatedInterfacePtrInfo")
    if (mojom.IsAssociatedInterfaceRequestKind(kind) or
        mojom.IsPendingAssociatedReceiverKind(kind)):
      return "codec.%s" % ("NullableAssociatedInterfaceRequest"
          if mojom.IsNullableKind(kind) else "AssociatedInterfaceRequest")
    if mojom.IsEnumKind(kind):
      return "new codec.Enum(%s)" % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      map_type = "NullableMapOf" if mojom.IsNullableKind(kind) else "MapOf"
      key_type = self._ElementCodecType(kind.key_kind)
      value_type = self._ElementCodecType(kind.value_kind)
      return "new codec.%s(%s, %s)" % (map_type, key_type, value_type)
    raise Exception("No codec type for %s" % kind)

  def _ElementCodecType(self, kind):
    return ("codec.PackedBool" if mojom.IsBoolKind(kind)
                               else self._CodecType(kind))

  def _JavaScriptDecodeSnippet(self, kind):
    if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
        mojom.IsAnyInterfaceKind(kind)):
      return "decodeStruct(%s)" % self._CodecType(kind)
    if mojom.IsStructKind(kind):
      return "decodeStructPointer(%s)" % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      return "decodeMapPointer(%s, %s)" % (
          self._ElementCodecType(kind.key_kind),
          self._ElementCodecType(kind.value_kind))
    if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
      return "decodeArrayPointer(codec.PackedBool)"
    if mojom.IsArrayKind(kind):
      return "decodeArrayPointer(%s)" % self._CodecType(kind.kind)
    if mojom.IsUnionKind(kind):
      return "decodeUnion(%s)" % self._CodecType(kind)
    if mojom.IsEnumKind(kind):
      return self._JavaScriptDecodeSnippet(mojom.INT32)
    raise Exception("No decode snippet for %s" % kind)

  def _JavaScriptEncodeSnippet(self, kind):
    if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
        mojom.IsAnyInterfaceKind(kind)):
      return "encodeStruct(%s, " % self._CodecType(kind)
    if mojom.IsUnionKind(kind):
      return "encodeStruct(%s, " % self._JavaScriptType(kind)
    if mojom.IsStructKind(kind):
      return "encodeStructPointer(%s, " % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      return "encodeMapPointer(%s, %s, " % (
          self._ElementCodecType(kind.key_kind),
          self._ElementCodecType(kind.value_kind))
    if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
      return "encodeArrayPointer(codec.PackedBool, ";
    if mojom.IsArrayKind(kind):
      return "encodeArrayPointer(%s, " % self._CodecType(kind.kind)
    if mojom.IsEnumKind(kind):
      return self._JavaScriptEncodeSnippet(mojom.INT32)
    raise Exception("No encode snippet for %s" % kind)

  def _JavaScriptUnionDecodeSnippet(self, kind):
    if mojom.IsUnionKind(kind):
      return "decodeStructPointer(%s)" % self._JavaScriptType(kind)
    return self._JavaScriptDecodeSnippet(kind)

  def _JavaScriptUnionEncodeSnippet(self, kind):
    if mojom.IsUnionKind(kind):
      return "encodeStructPointer(%s, " % self._JavaScriptType(kind)
    return self._JavaScriptEncodeSnippet(kind)

  def _JavaScriptNullableParam(self, field):
    return "true" if mojom.IsNullableKind(field.kind) else "false"

  def _JavaScriptValidateArrayParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    element_kind = field.kind.kind
    element_size = pack.PackedField.GetSizeForKind(element_kind)
    expected_dimension_sizes = GetArrayExpectedDimensionSizes(
        field.kind)
    element_type = self._ElementCodecType(element_kind)
    return "%s, %s, %s, %s, 0" % \
        (element_size, element_type, nullable,
         expected_dimension_sizes)

  def _JavaScriptValidateEnumParams(self, field):
    return self._JavaScriptType(field.kind)

  def _JavaScriptValidateStructParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    struct_type = self._JavaScriptType(field.kind)
    return "%s, %s" % (struct_type, nullable)

  def _JavaScriptValidateUnionParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    union_type = self._JavaScriptType(field.kind)
    return "%s, %s" % (union_type, nullable)

  def _JavaScriptValidateMapParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    keys_type = self._ElementCodecType(field.kind.key_kind)
    values_kind = field.kind.value_kind;
    values_type = self._ElementCodecType(values_kind)
    values_nullable = "true" if mojom.IsNullableKind(values_kind) else "false"
    return "%s, %s, %s, %s" % \
        (nullable, keys_type, values_type, values_nullable)

  def _JavaScriptSanitizeIdentifier(self, identifier):
    if identifier in _js_reserved_keywords:
      return identifier + '_'

    return identifier

  def _ExpressionToText(self, token):
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
        return "Infinity";
      if token.value == "double.NEGATIVE_INFINITY" or \
         token.value == "float.NEGATIVE_INFINITY":
        return "-Infinity";
      if token.value == "double.NAN" or token.value == "float.NAN":
        return "NaN";

    return token

  def _ExpressionToTextLite(self, token):
    if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
      # Both variable and enum constants are constructed like:
      # NamespaceUid.Struct[.Enum].CONSTANT_NAME
      name = []
      if token.module:
        name.append(token.module.namespace)
      if token.parent_kind:
        name.append(token.parent_kind.name)
      if isinstance(token, mojom.EnumValue):
        name.append(token.enum.name)
      name.append(token.name)
      return ".".join(name)

    return self._ExpressionToText(token)

  def _GenerateHtmlImports(self):
    result = []
    for full_import in self.module.imports:
      result.append(os.path.relpath(full_import.path,
                                    os.path.dirname(self.module.path)))
    return result

  def _GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(method.param_struct)
        if method.response_param_struct is not None:
          result.append(method.response_param_struct)
    return result

  def _FuzzHandleName(self, kind):
    if mojom.IsInterfaceRequestKind(kind) or mojom.IsPendingReceiverKind(kind):
      return '{0}.{1}Request'.format(kind.kind.module.namespace,
                                     kind.kind.name)
    elif mojom.IsInterfaceKind(kind):
      return '{0}.{1}Ptr'.format(kind.module.namespace,
                                 kind.name)
    elif mojom.IsPendingRemoteKind(kind):
      return '{0}.{1}Ptr'.format(kind.kind.module.namespace,
                                 kind.kind.name)
    elif (mojom.IsAssociatedInterfaceRequestKind(kind) or
          mojom.IsPendingAssociatedReceiverKind(kind)):
      return '{0}.{1}AssociatedRequest'.format(kind.kind.module.namespace,
                                               kind.kind.name)
    elif (mojom.IsAssociatedInterfaceKind(kind) or
          mojom.IsPendingAssociatedRemoteKind(kind)):
      return '{0}.{1}AssociatedPtr'.format(kind.kind.module.namespace,
                                           kind.kind.name)
    elif mojom.IsSharedBufferKind(kind):
      return 'handle<shared_buffer>'
    elif mojom.IsDataPipeConsumerKind(kind):
      return 'handle<data_pipe_consumer>'
    elif mojom.IsDataPipeProducerKind(kind):
      return 'handle<data_pipe_producer>'
    elif mojom.IsMessagePipeKind(kind):
      return 'handle<message_pipe>'

  def _ToJsBoolean(self, value):
    if value:
      return 'true'

    return 'false'

  def _IsPrimitiveKind(self, kind):
    return kind in mojom.PRIMITIVES

  def _PrimitiveToFuzzType(self, kind):
    return _primitive_kind_to_fuzz_type[kind]
