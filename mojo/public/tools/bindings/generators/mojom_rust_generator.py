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

  def _GetNameForKind(self, kind):
    type_name = kind.name

    # If the type is defined in another type (e.g. an Enum defined in a
    # Struct), prepend the enclosing type name. Rust does not support nested
    # type definitions, unlike C++.
    if kind.parent_kind:
      type_name = f"{kind.parent_kind.name}_{kind.name}"

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

  def _GetRustFieldTypeImpl(self, kind):
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

  def _GetRustFieldType(self, kind):
    t = self._GetRustFieldTypeImpl(kind)
    return t

  @staticmethod
  def GetTemplatePrefix():
    return "rust_templates"

  def GetFilters(self):
    rust_filters = {
        "rust_field_type": self._GetRustFieldType,
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
