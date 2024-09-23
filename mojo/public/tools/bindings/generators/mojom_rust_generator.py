# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import pathlib

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja, UseJinjaForImportedTemplate

GENERATOR_PREFIX = 'rust'

_kind_to_rust_type = {
    mojom.BOOL: "bool",
    mojom.INT8: "i8",
    mojom.INT16: "i16",
    mojom.INT32: "i32",
    mojom.INT64: "i64",
    mojom.UINT8: "u8",
    mojom.UINT16: "u16",
    mojom.UINT32: "u32",
    mojom.UINT64: "u64",
    mojom.FLOAT: "f32",
    mojom.DOUBLE: "f64",
}


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

    # Lists other generated bindings this module depends on. Unlike C++
    # bindings, the generated Rust code must reference which GN target the
    # bindings come from. Each entry has the GN target (e.g. foo_mojom_rust)
    # and an arbitrary name to import it under.
    self._imports = []

    # Maps each mojom source file to the Rust name its GN target was imported
    # under.
    self._path_import_map = {}

  def _GetNameForKind(self, kind, is_data=False):
    ''' Get the full Rust type name to refer to a generated binding.

    Args:
      is_data: If true, get the name of the wire-format type.
    '''
    type_name = kind.name

    # If the type is defined in another type (e.g. an Enum defined in a
    # Struct), prepend the enclosing type name. Rust does not support nested
    # type definitions, unlike C++.
    if kind.parent_kind:
      type_name = f"{kind.parent_kind.name}_{kind.name}"

    if is_data:
      type_name += "_Data"

    # Use the name as is if it's defined in the current module.
    if kind.module is self.module:
      return type_name

    # Construct the fully qualified path to the type if it's defined in another
    # mojom module.
    source_name = pathlib.Path(kind.module.path).stem
    if kind.module.path in self._path_import_map:
      imp_name = self._path_import_map[kind.module.path]
      return f"{imp_name}::{source_name}::{type_name}"

    return f"crate::{source_name}::{type_name}"

  def _GetRustFieldType(self, kind):
    if mojom.IsNullableKind(kind):
      return f"Option<{self._GetRustFieldType(kind.MakeUnnullableKind())}>"
    if mojom.IsEnumKind(kind):
      return self._GetNameForKind(kind)
    if mojom.IsStructKind(kind) or mojom.IsUnionKind(kind):
      return f"Box<{self._GetNameForKind(kind)}>"
    if mojom.IsArrayKind(kind):
      return f"Vec<{self._GetRustFieldType(kind.kind)}>"
    if mojom.IsMapKind(kind):
      return (f"std::collections::HashMap<"
              f"{self._GetRustFieldType(kind.key_kind)}, "
              f"{self._GetRustFieldType(kind.value_kind)}>")
    if mojom.IsStringKind(kind):
      return "String"
    if mojom.IsAnyHandleKind(kind):
      return "::mojo::UntypedHandle"
    if mojom.IsReferenceKind(kind):
      return "usize"
    if kind not in _kind_to_rust_type:
      return "()"
    return _kind_to_rust_type[kind]

  def _GetRustDataFieldType(self, kind):
    if mojom.IsEnumKind(kind):
      return self._GetNameForKind(kind, is_data=True)
    if mojom.IsStructKind(kind):
      return (f"bindings::data::Pointer<"
              f"{self._GetNameForKind(kind, is_data=True)}>")
    if mojom.IsArrayKind(kind):
      return (f"bindings::data::Pointer<bindings::data::Array<"
              f"{self._GetRustDataFieldType(kind.kind)}>>")
    if mojom.IsStringKind(kind):
      return "bindings::data::Pointer<bindings::data::Array<u8>>"
    if mojom.IsMapKind(kind):
      return (f"bindings::data::Pointer<bindings::data::Map<"
              f"{self._GetRustDataFieldType(kind.key_kind)}, "
              f"{self._GetRustDataFieldType(kind.value_kind)}>>")
    if mojom.IsUnionKind(kind):
      return self._GetNameForKind(kind, is_data=True)
    if mojom.IsInterfaceKind(kind) or mojom.IsPendingRemoteKind(kind):
      return "bindings::data::InterfaceData"
    if mojom.IsPendingReceiverKind(kind):
      return "bindings::data::HandleRef"
    if mojom.IsPendingAssociatedRemoteKind(kind):
      return "bindings::data::InterfaceData"
    if mojom.IsPendingAssociatedReceiverKind(kind):
      return "bindings::data::HandleRef"
    if mojom.IsAnyHandleKind(kind):
      return "bindings::data::HandleRef"
    if kind not in _kind_to_rust_type:
      return "()"
    return _kind_to_rust_type[kind]

  def _GetRustUnionFieldType(self, kind):
    if kind == mojom.BOOL:
      return "u8"
    if mojom.IsUnionKind(kind):
      return (
          f"bindings::data::Pointer<{self._GetNameForKind(kind, is_data=True)}>"
      )
    return self._GetRustDataFieldType(kind)

  def _GetRustReferentDataType(self, kind):
    if mojom.IsStructKind(kind):
      return self._GetNameForKind(kind, is_data=True)
    else:
      print(kind.Repr())
      raise Exception("Not implemented")

  def _ToUpperSnakeCase(self, ident):
    return generator.ToUpperSnakeCase(ident)

  def _ToLowerSnakeCase(self, ident):
    return generator.ToLowerSnakeCase(ident)

  def _GetRustDataFields(self, packed_struct):
    ''' Map pack.PackedStruct to a list of Rust fields.

    Adjacent bitfield members are packed into u8 fields, and explicit padding is
    added so that the resulting type has no possibly uninitialized bits.
    '''
    rust_fields = []
    for i in range(len(packed_struct.packed_fields)):
      packed_field = packed_struct.packed_fields[i]

      # Compute padding needed, if any, between current and previous field.
      if i > 0:
        prev_pf = packed_struct.packed_fields[i - 1]
        pad_start = prev_pf.offset + prev_pf.size
        pad_size = packed_field.offset - pad_start
        if pad_size > 0:
          rust_fields.append({
              "name": f"_pad_{pad_start}",
              "type": f"[u8; {pad_size}]"
          })

      # Bitfields are packed together. Since Rust doesn't have C-style bitfields
      # this must be done manually. On the first bool bit at a given offset,
      # declare a unique u8 member. `packed_struct`'s fields are ordered by byte
      # and bit offset so skip fields other than the first bit.
      if packed_field.field.kind == mojom.BOOL:
        if packed_field.bit == 0:
          rust_fields.append({
              "name": f"_packed_bits_{packed_field.offset}",
              "type": "u8"
          })
        continue

      # Pick the field name. If `pf.original_field` is present, this is a
      # nullable primitive kind so its value component has a name which includes
      # a special character; use the name from the original field. Otherwise use
      # the name as-is.
      if packed_field.original_field:
        name = packed_field.original_field.name
      else:
        name = packed_field.field.name

      rust_fields.append({
          "name":
          name,
          "type":
          self._GetRustDataFieldType(packed_field.field.kind)
      })

    # Create end padding, if needed.
    if len(packed_struct.packed_fields) > 0:
      last_pf = packed_struct.packed_fields[-1]
      pad = pack.GetPad(last_pf.offset + last_pf.size, 8)
      if pad > 0:
        rust_fields.append({"name": "_pad_end", "type": f"[u8; {pad}]"})

    return rust_fields

  def _GetPackedBoolLocation(self, packed_field):
    return {
        "field_name": f"_packed_bits_{packed_field.offset}",
        "bit_offset": f"{packed_field.bit}"
    }

  @staticmethod
  def GetTemplatePrefix():
    return "rust_templates"

  def GetFilters(self):
    rust_filters = {
        "get_packed_bool_location": self._GetPackedBoolLocation,
        "get_pad": pack.GetPad,
        "get_rust_data_fields": self._GetRustDataFields,
        "is_enum_kind": mojom.IsEnumKind,
        "is_pointer_kind": mojom.IsPointerKind,
        "is_nullable_kind": mojom.IsNullableKind,
        "is_struct_kind": mojom.IsStructKind,
        "rust_field_type": self._GetRustFieldType,
        "rust_union_field_type": self._GetRustUnionFieldType,
        "rust_referent_data_type": self._GetRustReferentDataType,
        "to_upper_snake_case": self._ToUpperSnakeCase,
        "to_lower_snake_case": self._ToLowerSnakeCase,
    }
    return rust_filters

  @UseJinja("module.tmpl")
  def _GenerateModule(self):
    return {"module": self.module, "imports": self._imports}

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--rust_dep_info')
    args = parser.parse_args(unparsed_args)

    # Load the list of mojom dependencies: each GN target and its list of
    # mojom source files.
    dep_info = []
    with open(args.rust_dep_info) as dep_info_json:
      dep_info = json.loads(dep_info_json.read())

    import_ndx = 1
    for dep in dep_info:
      use_name = f"dep{import_ndx}"
      self._imports.append({
          "target": dep["target_name"],
          "use_name": use_name,
      })

      for src in dep["mojom_sources"]:
        self._path_import_map[src] = use_name

      import_ndx += 1

    self.module.Stylize(generator.Stylizer())

    self.WriteWithComment(self._GenerateModule(), f"{self.module.path}.rs")
