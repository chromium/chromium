# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja, UseJinjaForImportedTemplate

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

  def _GetNameForKind(self, kind):
    "::".join(kind.module.namespace.split("."))

  def _GetRustFieldType(self, kind):
    if mojom.IsEnumKind(kind) or mojom.IsStructKind(kind) or mojom.IsUnionKind(
        kind):
      return self._GetNameForKind(kind)
    if mojom.IsArrayKind(kind):
      return f"Vec<{self._GetRustFieldType(kind.kind)}>"
    if mojom.IsMapKind(kind):
      return (f"HashMap<"
              f"{self._GetRustFieldType(kind.key_kind)}, "
              f"{self._GetRustFieldType(kind.value_kind)}>")
    if mojom.IsStringKind(kind):
      return "String"
    if mojom.IsAnyHandleKind(kind):
      return "::mojo::UntypedHandle"
    if mojom.IsReferenceKind(kind):
      return "usize"
    return _kind_to_rust_type[kind]

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
    return {"module": self.module}

  def GenerateFiles(self, args):
    self.module.Stylize(generator.Stylizer())

    self.WriteWithComment(self._GenerateModule(), f"{self.module.path}.rs")
