# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ source files from a mojom.Module."""
import os
import sys
from functools import partial
from generators.mojom_cpp_generator import _NameFormatter as CppNameFormatter
from generators.mojom_cpp_generator import Generator as CppGenerator
from generators.mojom_cpp_generator import IsNativeOnlyKind, NamespaceToArray
import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja, UseJinjaForImportedTemplate

_kind_to_proto_type = {
    mojom.BOOL: "bool",
    mojom.INT8: "int32",
    mojom.UINT8: "uint32",
    mojom.INT16: "int32",
    mojom.UINT16: "uint32",
    mojom.INT32: "int32",
    mojom.UINT32: "uint32",
    mojom.FLOAT: "float",
    mojom.INT64: "int64",
    mojom.UINT64: "uint64",
    mojom.DOUBLE: "double",
    mojom.NULLABLE_BOOL: "bool",
    mojom.NULLABLE_INT8: "int32",
    mojom.NULLABLE_UINT8: "uint32",
    mojom.NULLABLE_INT16: "int32",
    mojom.NULLABLE_UINT16: "uint32",
    mojom.NULLABLE_INT32: "int32",
    mojom.NULLABLE_UINT32: "uint32",
    mojom.NULLABLE_FLOAT: "float",
    mojom.NULLABLE_INT64: "int64",
    mojom.NULLABLE_UINT64: "uint64",
    mojom.NULLABLE_DOUBLE: "double",
}

_kind_to_cpp_proto_type = {
    mojom.BOOL: "bool",
    mojom.INT8: "::google::protobuf::int32",
    mojom.UINT8: "::google::protobuf::uint32",
    mojom.INT16: "::google::protobuf::int32",
    mojom.UINT16: "::google::protobuf::uint32",
    mojom.INT32: "::google::protobuf::int32",
    mojom.UINT32: "::google::protobuf::uint32",
    mojom.FLOAT: "float",
    mojom.INT64: "::google::protobuf::int64",
    mojom.UINT64: "::google::protobuf::int64",
    mojom.DOUBLE: "double",
    mojom.NULLABLE_BOOL: "bool",
    mojom.NULLABLE_INT8: "::google::protobuf::int32",
    mojom.NULLABLE_UINT8: "::google::protobuf::uint32",
    mojom.NULLABLE_INT16: "::google::protobuf::int32",
    mojom.NULLABLE_UINT16: "::google::protobuf::uint32",
    mojom.NULLABLE_INT32: "::google::protobuf::int32",
    mojom.NULLABLE_UINT32: "::google::protobuf::uint32",
    mojom.NULLABLE_FLOAT: "float",
    mojom.NULLABLE_INT64: "::google::protobuf::int64",
    mojom.NULLABLE_UINT64: "::google::protobuf::int64",
    mojom.NULLABLE_DOUBLE: "double",
}


class _NameFormatter(CppNameFormatter):
  """A formatter for the names of kinds or values."""

  def __init__(self, *args, **kwargs):
    super(_NameFormatter, self).__init__(*args, **kwargs)

  def FormatForProto(self,
                     omit_namespace_for_module=None,
                     flatten_nested_kind=False):
    return self.Format(".",
                       prefixed=True,
                       omit_namespace_for_module=omit_namespace_for_module,
                       flatten_nested_kind=flatten_nested_kind)


class Generator(CppGenerator):
  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)
    self.needs_mojolpm_proto = False
    self.enum_name_cache = dict()

  def _GetAllExtraTraitsHeaders(self):
    extra_headers = set()
    for typemap in self._GetAllTypemaps():
      extra_headers.update(typemap.get("traits_headers", []))
    extra_headers.update(self._GetExtraTraitsHeaders())
    return sorted(extra_headers)

  def _GetAllTypemaps(self):
    """Returns the typemaps for types needed in this module.
    """
    all_typemaps = []
    seen_types = set()

    def AddKind(kind):
      if (mojom.IsIntegralKind(kind) or mojom.IsStringKind(kind)
          or mojom.IsDoubleKind(kind) or mojom.IsFloatKind(kind)
          or mojom.IsAnyHandleKind(kind) or mojom.IsInterfaceKind(kind)
          or mojom.IsAssociatedKind(kind) or mojom.IsPendingRemoteKind(kind)
          or mojom.IsPendingReceiverKind(kind)):
        pass
      elif mojom.IsArrayKind(kind):
        AddKind(kind.kind)
      elif mojom.IsMapKind(kind):
        AddKind(kind.key_kind)
        AddKind(kind.value_kind)
      else:
        name = self._GetFullMojomNameForKind(kind)
        if name in seen_types:
          return
        seen_types.add(name)

        typemap = self.typemap.get(name, None)
        if typemap:
          all_typemaps.append(typemap)
        if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
          for field in kind.fields:
            AddKind(field.kind)

    for kind in self.module.enums + self.module.structs + self.module.unions:
      AddKind(kind)

    return all_typemaps

  def _ProtoImports(self):
    """Scans all of the types used in this module to check which of the imports
    are needed for the generated proto files. This is somewhat different to the
    general case, since the generated proto files don't reference response
    parameters.
    """
    all_imports = self.module.imports
    seen_imports = set()
    seen_types = set()

    def AddKind(kind):
      if (mojom.IsIntegralKind(kind) or mojom.IsStringKind(kind)
          or mojom.IsDoubleKind(kind) or mojom.IsFloatKind(kind)):
        pass
      elif (mojom.IsAnyHandleKind(kind)):
        self.needs_mojolpm_proto = True
      elif mojom.IsArrayKind(kind):
        AddKind(kind.kind)
      elif mojom.IsMapKind(kind):
        AddKind(kind.key_kind)
        AddKind(kind.value_kind)
      elif (mojom.IsStructKind(kind) or mojom.IsUnionKind(kind)
            or mojom.IsEnumKind(kind)):
        name = self._GetFullMojomNameForKind(kind)
        if name in seen_types:
          return
        seen_types.add(name)
        if kind.module in all_imports:
          seen_imports.add(kind.module)
      elif (mojom.IsPendingRemoteKind(kind) or mojom.IsPendingReceiverKind(kind)
            or mojom.IsPendingAssociatedRemoteKind(kind)
            or mojom.IsPendingAssociatedReceiverKind(kind)):
        AddKind(kind.kind)

    for kind in self.module.structs + self.module.unions:
      for field in kind.fields:
        AddKind(field.kind)

    for interface in self.module.interfaces:
      for method in interface.methods:
        for parameter in method.parameters:
          AddKind(parameter.kind)
        if method.response_parameters:
          for parameter in method.response_parameters:
            AddKind(parameter.kind)

    import_files = list(
        map(lambda x: '{}.mojolpm.proto'.format(x.path), seen_imports))
    if self.needs_mojolpm_proto:
      import_files.append('mojo/public/tools/fuzzers/mojolpm.proto')
    import_files.sort()

    return import_files

  def _GetJinjaExports(self):
    all_enums = list(self.module.enums)
    for struct in self.module.structs:
      all_enums.extend(struct.enums)
    for interface in self.module.interfaces:
      all_enums.extend(interface.enums)

    return {
        "all_enums": all_enums,
        "all_extra_traits_headers": self._GetAllExtraTraitsHeaders(),
        "enums": self.module.enums,
        "extra_public_headers": self._GetExtraPublicHeaders(),
        "extra_traits_headers": self._GetExtraTraitsHeaders(),
        "imports": self.module.imports,
        "interfaces": self.module.interfaces,
        "module": self.module,
        "module_namespace": self.module.namespace,
        "namespaces_as_array": NamespaceToArray(self.module.namespace),
        "proto_imports": self._ProtoImports(),
        "structs": self.module.structs,
        "unions": self.module.unions,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "mojolpm_templates"

  def GetFilters(self):
    cpp_filters = {
        "camel_to_under": generator.ToLowerSnakeCase,
        "contains_handles_or_interfaces": mojom.ContainsHandlesOrInterfaces,
        "cpp_wrapper_call_type": self._GetCppWrapperCallType,
        "cpp_wrapper_param_type": self._GetCppWrapperParamType,
        "cpp_wrapper_proto_type": self._GetCppWrapperProtoType,
        "cpp_wrapper_type": self._GetCppWrapperType,
        "default_value": self._DefaultValue,
        "default_constructor_args": self._DefaultConstructorArgs,
        "enum_field_name": self._EnumFieldName,
        "get_name_for_kind": self._GetNameForKind,
        "get_qualified_name_for_kind": self._GetQualifiedNameForKind,
        "has_duplicate_values": self._EnumHasDuplicateValues,
        "nullable_is_same_kind": self._NullableIsSameKind,
        "proto_field_type": self._GetProtoFieldType,
        "proto_id": self._GetProtoId,
        "is_array_kind": mojom.IsArrayKind,
        "is_bool_kind": mojom.IsBoolKind,
        "is_default_constructible": self._IsDefaultConstructible,
        "is_value_kind": mojom.IsValueKind,
        "is_enum_kind": mojom.IsEnumKind,
        "is_double_kind": mojom.IsDoubleKind,
        "is_float_kind": mojom.IsFloatKind,
        "is_integral_kind": mojom.IsIntegralKind,
        "is_interface_kind": mojom.IsInterfaceKind,
        "is_nullable_value_kind_packed_field":
        pack.IsNullableValueKindPackedField,
        "is_primary_nullable_value_kind_packed_field":
        pack.IsPrimaryNullableValueKindPackedField,
        "is_receiver_kind": self._IsReceiverKind,
        "is_pending_associated_receiver_kind":
        mojom.IsPendingAssociatedReceiverKind,
        "is_pending_receiver_kind": mojom.IsPendingReceiverKind,
        "is_pending_associated_remote_kind":
        mojom.IsPendingAssociatedRemoteKind,
        "is_pending_remote_kind": mojom.IsPendingRemoteKind,
        "is_platform_handle_kind": mojom.IsPlatformHandleKind,
        "is_native_only_kind": IsNativeOnlyKind,
        "is_any_handle_kind": mojom.IsAnyHandleKind,
        "is_any_interface_kind": mojom.IsAnyInterfaceKind,
        "is_any_handle_or_interface_kind": mojom.IsAnyHandleOrInterfaceKind,
        "is_associated_kind": mojom.IsAssociatedKind,
        "is_float_kind": mojom.IsFloatKind,
        "is_hashable": self._IsHashableKind,
        "is_map_kind": mojom.IsMapKind,
        "is_move_only_kind": self._IsMoveOnlyKind,
        "is_non_const_ref_kind": self._IsNonConstRefKind,
        "is_nullable_kind": mojom.IsNullableKind,
        "is_object_kind": mojom.IsObjectKind,
        "is_reference_kind": mojom.IsReferenceKind,
        "is_string_kind": mojom.IsStringKind,
        "is_struct_kind": mojom.IsStructKind,
        "is_typemapped_kind": self._IsTypemappedKind,
        "is_union_kind": mojom.IsUnionKind,
        "to_unnullable_kind": self._ToUnnullableKind,
        "under_to_camel": partial(self._UnderToCamel, digits_split=True)
    }
    return cpp_filters

  @UseJinja("mojolpm.proto.tmpl")
  def _GenerateMojolpmProto(self):
    return self._GetJinjaExports()

  @UseJinja("mojolpm.h.tmpl")
  def _GenerateMojolpmHeader(self):
    return self._GetJinjaExports()

  @UseJinja("mojolpm.cc.tmpl")
  def _GenerateMojolpmSource(self):
    return self._GetJinjaExports()

  def GenerateFiles(self, args):
    self.module.Stylize(generator.Stylizer())

    if self.generate_non_variant_code:
      self.WriteWithComment(self._GenerateMojolpmProto(),
                            "%s.mojolpm.proto" % self.module.path)
    else:
      self.WriteWithComment(self._GenerateMojolpmHeader(),
                            "%s-mojolpm.h" % self.module.path)
      self.WriteWithComment(self._GenerateMojolpmSource(),
                            "%s-mojolpm.cc" % self.module.path)

  def _GetCppProtoNameForKind(self,
                              kind,
                              flatten_nested_kind=False,
                              add_same_module_namespaces=False):
    name = _NameFormatter(kind, self.variant).FormatForCpp(
        flatten_nested_kind=flatten_nested_kind,
        omit_namespace_for_module=(None if add_same_module_namespaces else
                                   self.module))
    if name.startswith('::'):
      name = 'mojolpm' + name
    return name

  def _GetProtoNameForKind(self,
                           kind,
                           flatten_nested_kind=False,
                           add_same_module_namespaces=False):
    name = _NameFormatter(kind, self.variant).FormatForProto(
        flatten_nested_kind=flatten_nested_kind,
        omit_namespace_for_module=(None if add_same_module_namespaces else
                                   self.module))
    if name.startswith('.'):
      name = 'mojolpm' + name
    return name

  def _IsMoveOnlyKind(self, kind):
    if self._IsTypemappedKind(kind):
      if mojom.IsEnumKind(kind):
        return False
      return self.typemap[self._GetFullMojomNameForKind(kind)]["move_only"]
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return True
    if mojom.IsArrayKind(kind):
      return self._IsMoveOnlyKind(kind.kind)
    if mojom.IsMapKind(kind):
      return (self._IsMoveOnlyKind(kind.value_kind)
              or self._IsMoveOnlyKind(kind.key_kind))
    if mojom.IsAnyHandleOrInterfaceKind(kind):
      return True
    return False

  def _GetNativeTypeName(self, typemapped_kind):
    return self.typemap[self._GetFullMojomNameForKind(
        typemapped_kind)]["typename"]

  def _FormatConstantDeclaration(self, constant, nested=False):
    if mojom.IsStringKind(constant.kind):
      if nested:
        return "const char %s[]" % constant.name
      return "%sextern const char %s[]" % \
          ((self.export_attribute + " ") if self.export_attribute else "",
           constant.name)
    return "constexpr %s %s = %s" % (GetCppPodType(
        constant.kind), constant.name, self._ConstantValue(constant))

  def _IsMoveOnlyKind(self, kind):
    if self._IsTypemappedKind(kind):
      if mojom.IsEnumKind(kind):
        return False
      return self.typemap[self._GetFullMojomNameForKind(kind)]["move_only"]
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return True
    if mojom.IsArrayKind(kind):
      return self._IsMoveOnlyKind(kind.kind)
    if mojom.IsMapKind(kind):
      return (self._IsMoveOnlyKind(kind.value_kind)
              or self._IsMoveOnlyKind(kind.key_kind))
    if mojom.IsAnyHandleOrInterfaceKind(kind):
      return True
    return False

  def _IsNonConstRefKind(self, kind):
    if self._IsTypemappedKind(kind):
      return self.typemap[self._GetFullMojomNameForKind(kind)]["non_const_ref"]
    return False

  def _GetCppWrapperProtoType(self, kind, add_same_module_namespaces=False):
    if (mojom.IsEnumKind(kind) or mojom.IsStructKind(kind)
        or mojom.IsUnionKind(kind)):
      return self._GetCppProtoNameForKind(
          kind, add_same_module_namespaces=add_same_module_namespaces)
    elif (mojom.IsPendingRemoteKind(kind) or mojom.IsPendingReceiverKind(kind)
          or mojom.IsPendingAssociatedRemoteKind(kind)
          or mojom.IsPendingAssociatedReceiverKind(kind)):
      return "uint32_t"
    elif mojom.IsStringKind(kind):
      return "std::string"
    elif mojom.IsGenericHandleKind(kind):
      return "mojolpm::Handle"
    elif mojom.IsDataPipeConsumerKind(kind):
      return "mojolpm::DataPipeConsumerHandle"
    elif mojom.IsDataPipeProducerKind(kind):
      return "mojolpm::DataPipeProducerHandle"
    elif mojom.IsMessagePipeKind(kind):
      return "mojolpm::MessagePipeHandle"
    elif mojom.IsSharedBufferKind(kind):
      return "mojolpm::SharedBufferHandle"
    elif mojom.IsPlatformHandleKind(kind):
      return "mojolpm::PlatformHandle"

    if not kind in _kind_to_cpp_proto_type:
      raise Exception("Unrecognized kind %s" % kind.spec)
    return _kind_to_cpp_proto_type[kind]

  def _GetProtoFieldType(self, kind, quantified=True):
    # TODO(markbrand): This will not handle array<array> or array<map>
    # TODO(markbrand): This also will not handle array<x, 10>
    unquantified = ''
    if (mojom.IsEnumKind(kind) or mojom.IsStructKind(kind)
        or mojom.IsUnionKind(kind)):
      unquantified = self._GetProtoNameForKind(kind)
    elif mojom.IsArrayKind(kind):
      return "repeated %sEntry" % self._GetProtoFieldType(kind.kind,
                                                          quantified=False)
    elif mojom.IsMapKind(kind):
      return ("map<%sKey, %sValue>" %
              (self._GetProtoFieldType(kind.key_kind, quantified=False),
               self._GetProtoFieldType(kind.value_kind, quantified=False)))
    elif (mojom.IsPendingRemoteKind(kind) or mojom.IsPendingReceiverKind(kind)
          or mojom.IsPendingAssociatedRemoteKind(kind)
          or mojom.IsPendingAssociatedReceiverKind(kind)):
      unquantified = "uint32"
    elif mojom.IsStringKind(kind):
      unquantified = "string"
    elif mojom.IsGenericHandleKind(kind):
      unquantified = "mojolpm.Handle"
    elif mojom.IsDataPipeConsumerKind(kind):
      unquantified = "mojolpm.DataPipeConsumerHandle"
    elif mojom.IsDataPipeProducerKind(kind):
      unquantified = "mojolpm.DataPipeProducerHandle"
    elif mojom.IsMessagePipeKind(kind):
      unquantified = "mojolpm.MessagePipeHandle"
    elif mojom.IsSharedBufferKind(kind):
      unquantified = "mojolpm.SharedBufferHandle"
    elif mojom.IsPlatformHandleKind(kind):
      unquantified = "mojolpm.PlatformHandle"
    else:
      unquantified = _kind_to_proto_type[kind]

    if quantified and mojom.IsNullableKind(kind):
      return 'optional %s' % unquantified
    elif quantified:
      return 'required %s' % unquantified
    else:
      return unquantified

  def _GetProtoId(self, name, kind=''):
    # We reserve ids [0,15]
    # Protobuf implementation reserves [19000,19999]
    # Max proto id is 2^29-1
    string = '{}@{}'.format(name, self._GetProtoFieldType(kind, False)).lower()
    # 32-bit fnv-1a
    fnv = 2166136261
    for c in string:
      fnv = fnv ^ ord(c)
      fnv = (fnv * 16777619) & 0xffffffff
    # xor-fold to 29-bits
    fnv = (fnv >> 29) ^ (fnv & 0x1fffffff)
    # now use a modulo to reduce to [0,2^29-1 - 1016]
    fnv = fnv % 536869895
    # now we move out the disallowed ranges
    fnv = fnv + 15
    if fnv >= 19000:
      fnv += 1000
    return fnv

  def _NullableIsSameKind(self, kind):
    if self._IsTypemappedKind(kind):
      if not self.typemap[self._GetFullMojomNameForKind(
          kind)]["nullable_is_same_type"]:
        return False
    if mojom.IsArrayKind(kind):
      return False
    if mojom.IsMapKind(kind):
      return False
    if mojom.IsStringKind(kind):
      return False
    if mojom.IsValueKind(kind):
      return False
    return True

  def _EnumHasDuplicateValues(self, kind):
    values = dict()
    i = 0
    for field in kind.fields:
      value = None
      if field.value:
        if isinstance(field.value, str):
          # field.value is an integer value stored as a string
          value = int(field.value, 0)
        else:
          # field.value is a direct reference to another enum value, so it has
          # to be a duplicate
          assert isinstance(field.value, mojom.EnumValue)
          return True
      else:
        # If there is no provided value, then the value is simply the next one
        value = i

      assert (value != None)
      # If the value appears in the enum already, then it's a duplicate.
      if value in values.values():
        return True
      values[field.name] = value
      i = value + 1

    return False

  def _DefaultConstructorArgs(self, kind):
    if mojom.IsNullableKind(kind) or self._IsDefaultConstructible(kind):
      return ""
    return "mojo::internal::DefaultConstructTag()"

  def _EnumFieldName(self, name, kind):
    # The WebFeature enum has entries that differ only by the casing of the
    # names. Protobuf doesn't support this, so we add the value to the end of
    # the name in these cases to disambiguate.
    if kind not in self.enum_name_cache:
      field_names = dict()
      lower_field_names = set()
      for field in kind.fields:
        new_field_name = field.name
        if new_field_name.lower() in lower_field_names:
          new_field_name = '{}_{}'.format(new_field_name, field.numeric_value)
        lower_field_names.add(new_field_name.lower())
        field_names[field.name] = new_field_name
      self.enum_name_cache[kind] = field_names
    return self.enum_name_cache[kind][name]

  def _ToUnnullableKind(self, kind):
    assert mojom.IsNullableKind(kind)
    return kind.MakeUnnullableKind()
