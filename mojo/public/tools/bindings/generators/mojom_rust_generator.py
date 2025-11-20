# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates Rust source files from a mojom.Module."""

import sys

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja

GENERATOR_PREFIX = 'rust'

_mojom_primitive_type_to_rust_type = {
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
    mojom.STRING: "String",
}


def _MojomTypeToRustType(ty: mojom.Kind) -> str:
  '''Return the name of the input type in rust syntax'''
  # FOR_RELEASE: We don't support nested enums yet
  if mojom.IsStructKind(ty) or mojom.IsEnumKind(ty) or mojom.IsUnionKind(ty):
    return ty.name

  if mojom.IsArrayKind(ty):
    elt_ty = _MojomTypeToRustType(ty.kind)
    if ty.length is not None:
      return f"[{elt_ty}; {ty.length}]"
    else:
      return f"Vec<{elt_ty}>"

  if ty not in _mojom_primitive_type_to_rust_type:
    # Raising from a jinja2 call won't display the error message
    print(f"Mojom type {ty} is either undefined, "
          "or not supported by the rust bindings")
    sys.exit(1)

  return _mojom_primitive_type_to_rust_type[ty]


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

  @staticmethod
  def GetTemplatePrefix():
    '''Returns the name of the directory storing the rust jinja templates.'''
    return "rust_templates"

  @staticmethod
  def GetFilters():
    '''
    Returns a dictionary of functions that will be callable when processing
    the mojom template, using the syntax `arg|f` to mean `f(arg)`.

    Called by the @UseJinja decorator.
    '''
    return {"to_rust_type": _MojomTypeToRustType}

  @UseJinja("module.tmpl")
  def _GenerateModule(self):
    '''
    After dectoration, returns the generated rust module as a string, using
    with 'module.tmpl' as the root.

    Before decoration, returns a dictionary of variables that will be bound at
    the top level when processing the jinja template.
    '''
    return {"module": self.module}

  def GenerateFiles(self, unparsed_args):
    '''
    Main function, called by mojom_bindings_generator.py. Takes any arguments
    that were marked as being destined for the rust generator (prefixed with
    GENERATOR_PREFIX, i.e. 'rust').
    '''
    # Make sure all the AST nodes have pretty names
    self.module.Stylize(generator.Stylizer())

    self.WriteWithComment(self._GenerateModule(), f"{self.module.path}.rs")
