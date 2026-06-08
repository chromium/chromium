# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates Rust source files from a mojom.Module."""

import sys
import json
import os

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
    mojom.HANDLE: "system::mojo_types::UntypedHandle",
    mojom.MSGPIPE: "system::message_pipe::MessageEndpoint",
}


def _SameGNTarget(mod1: mojom.Module, mod2: mojom.Module,
                  source_to_target_map: dict) -> bool:
  return source_to_target_map[mod1.path] == source_to_target_map[mod2.path]


# Determine the right path to use to refer to `ty` from the current module.
#
# Each `mojom` GN target gets its own crate, and each `.mojom` file gets its
# own module within that crate. All the elements of a module appear at top
# level (there are no submodules). In order to figure out how to refer to `ty`,
# we need to determine if it was defined in the current file, the current crate,
# or an external crate.
#
# IMPORTANT NOTE:
# Mojom provides its own namespace mechanism using the `module` keyword. These
# are meant to mimic C++ namespaces; items declared inside the same `module` can
# be referred to using the same qualified path, regardless of what file they
# were defined in.
#
# Unfortunately, we can't provide this in rust; if two files are in different
# crates, there's no way we can automatically put them in the same `mod`.
# Therefore, we _completely ignore_ the `module` keyword when generating Rust
# code from a mojom file, and name things using _only_ the file system and GN
# structure.
def _GetLocalName(ty: mojom.Kind) -> str:
  # If the type is nested inside another (like an enum in a struct), we prefix
  # it with the parent's name to avoid collisions and match generated
  # definitions.
  #
  # Note: ty.qualified_name also includes the parent name (e.g. Parent.Child),
  # but it also includes the module namespace, and it's not stylized.
  # Since we want a stylized name joined with underscores, we recurse here.
  if hasattr(ty, 'parent_kind') and ty.parent_kind:
    return f"{_GetLocalName(ty.parent_kind)}_{ty.name}"
  return ty.name


def _GetQualifiedName(ty: mojom.Kind, current_module: mojom.Module,
                      source_to_target_map: dict) -> str:
  local_name = _GetLocalName(ty)

  # If the type was defined in this file, we can use its name unqualified
  if ty.module.path == current_module.path:
    return local_name

  # The module has the same name as the file that defined it, sans extension
  # Map foo/bar/baz.mojom -> baz
  ty_module_name = ty.module.path.split('/')[-1].split('.')[0]

  # If the type was defined as part of the same GN target as this file, then
  # it's in the same crate.
  if _SameGNTarget(ty.module, current_module, source_to_target_map):
    return f"crate::{ty_module_name}::{local_name}"

  # Otherwise, it was defined in a different crate, which has the same name as
  # as the GN target that defined it.
  extern_target_name = source_to_target_map[ty.module.path]
  # Map //foo/bar:baz -> baz, and //foo/bar -> bar
  extern_crate = extern_target_name.split(':')[-1].split('/')[-1]
  return f"{extern_crate}::{ty_module_name}::{local_name}"


def _MojomTypeToRustType(ty: mojom.Kind, current_module: mojom.Module,
                         source_to_target_map: dict, typemap: dict) -> str:
  '''Return the name of the input type in rust syntax'''
  if hasattr(ty, 'qualified_name') and ty.qualified_name in typemap:
    return typemap[ty.qualified_name]['typename']

  if mojom.IsNullableKind(ty):
    inner_ty = _MojomTypeToRustType(ty.MakeUnnullableKind(), current_module,
                                    source_to_target_map, typemap)
    return f"Option<{inner_ty}>"

  if mojom.IsStructKind(ty) or mojom.IsEnumKind(ty) or mojom.IsUnionKind(ty):
    return _GetQualifiedName(ty, current_module, source_to_target_map)

  if mojom.IsArrayKind(ty):
    elt_ty = _MojomTypeToRustType(ty.kind, current_module, source_to_target_map,
                                  typemap)
    if ty.length is not None:
      return f"[{elt_ty}; {ty.length}]"
    else:
      return f"Vec<{elt_ty}>"

  if mojom.IsMapKind(ty):
    key_ty = _MojomTypeToRustType(ty.key_kind, current_module,
                                  source_to_target_map, typemap)
    # Rust requires comparison operators to use floats as keys in a map
    if ty.key_kind == mojom.FLOAT or ty.key_kind == mojom.DOUBLE:
      key_ty = f"OrderedFloat<{key_ty}>"
    value_ty = _MojomTypeToRustType(ty.value_kind, current_module,
                                    source_to_target_map, typemap)
    return f"HashMap<{key_ty}, {value_ty}>"

  if mojom.IsPendingRemoteKind(ty):
    interface_ty = _GetQualifiedName(ty.kind, current_module,
                                     source_to_target_map)
    return f"bindings::remote::PendingRemote<dyn {interface_ty}>"

  if mojom.IsPendingReceiverKind(ty):
    interface_ty = _GetQualifiedName(ty.kind, current_module,
                                     source_to_target_map)
    return f"bindings::receiver::PendingReceiver<dyn {interface_ty}>"

  if mojom.IsPendingAssociatedRemoteKind(ty):
    interface_ty = _GetQualifiedName(ty.kind, current_module,
                                     source_to_target_map)
    return f"bindings::remote::PendingAssociatedRemote<dyn {interface_ty}>"

  if mojom.IsPendingAssociatedReceiverKind(ty):
    interface_ty = _GetQualifiedName(ty.kind, current_module,
                                     source_to_target_map)
    return f"bindings::receiver::PendingAssociatedReceiver<dyn {interface_ty}>"

  if ty not in _mojom_primitive_type_to_rust_type:
    # Raising from a jinja2 call won't display the error message
    print(f"Mojom type {ty} is either undefined, "
          "or not supported by the rust bindings")
    sys.exit(1)

  return _mojom_primitive_type_to_rust_type[ty]


def _GetParseAsType(ty: mojom.Kind, current_module: mojom.Module,
                    source_to_target_map: dict, typemap: dict) -> str:
  '''Return the regular generated type name if ty is typemapped, else None'''
  if hasattr(ty, 'qualified_name') and ty.qualified_name in typemap:
    return _MojomTypeToRustType(ty, current_module, source_to_target_map, {})
  return None


def _ShouldDeriveClone(ty: mojom.Kind) -> bool:
  '''We derive clone as a convenience to the user, but we can't do so if the
     type contains any handles, since those can't be copied.'''
  return not mojom.ContainsHandlesOrInterfaces(ty)


class Generator(generator.Generator):

  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)
    self.source_to_target_map = {}

  @staticmethod
  def GetTemplatePrefix():
    '''Returns the name of the directory storing the rust jinja templates.'''
    return "rust_templates"

  def GetFilters(self):
    '''
    Returns a dictionary of functions that will be callable when processing
    the mojom template, using the syntax `arg|f` to mean `f(arg)`.

    Called by the @UseJinja decorator.
    '''
    return {
        "to_rust_type":
        lambda ty: _MojomTypeToRustType(ty, self.module, self.
                                        source_to_target_map, self.typemap),
        "get_parse_as_type":
        lambda ty: _GetParseAsType(ty, self.module, self.source_to_target_map,
                                   self.typemap),
        "should_derive_clone":
        _ShouldDeriveClone,
    }

  @UseJinja("module.tmpl")
  def _GenerateModule(self):
    '''
    After dectoration, returns the generated rust module as a string, using
    with 'module.tmpl' as the root.

    Before decoration, returns a dictionary of variables that will be bound at
    the top level when processing the jinja template.
    '''

    # Take all our imports and determine the set of GN targets they come from
    imported_targets = set([
        self.source_to_target_map[imprt.path] for imprt in self.module.imports
    ])
    # Remove our own target, since we don't import ourselves
    imported_targets -= {self.source_to_target_map[self.module.path]}

    typemaps_to_include = []
    seen_files = set()

    source_root_abs = os.path.abspath(os.path.join(os.getcwd(), "../../"))
    gen_file_dir = os.path.abspath(
        os.path.join(os.getcwd(), "gen", os.path.dirname(self.module.path)))

    for kind in self.module.structs + self.module.enums + self.module.unions:
      if hasattr(kind,
                 'qualified_name') and kind.qualified_name in self.typemap:
        traits_file = self.typemap[kind.qualified_name].get('traits_file')
        if traits_file and traits_file not in seen_files:
          traits_file_abs = os.path.abspath(
              os.path.join(source_root_abs, traits_file))
          rel_path = os.path.relpath(traits_file_abs, gen_file_dir)
          typemaps_to_include.append({
              'path':
              rel_path,
              'name':
              os.path.splitext(os.path.basename(traits_file))[0]
          })
          seen_files.add(traits_file)

    return {
        "module": self.module,
        "imports": imported_targets,
        "typemaps_to_include": typemaps_to_include,
        "typemap": self.typemap,
    }

  def GenerateFiles(self, unparsed_args):
    '''
    Main function, called by mojom_bindings_generator.py. Takes any arguments
    that were marked as being destined for the rust generator (prefixed with
    GENERATOR_PREFIX, i.e. 'rust').
    '''
    # Make sure all the AST nodes have pretty names
    self.module.Stylize(generator.Stylizer())

    # When GN calls this script, it provides a JSON file with a list of GN
    # targets, and their source files. It contains one entry for the mojom
    # target we're generating now, and one for each of its dependencies.
    # We want to convert it to a map from source files to GN targets.
    for arg in unparsed_args:
      if arg.startswith("--rust_dep_info="):
        dep_info_path = arg.split('=')[1]
        with open(dep_info_path, 'r') as f:
          raw_dep_info = json.load(f)
          for target in raw_dep_info:
            target_name = target['target_name']
            for source in target['mojom_sources']:
              self.source_to_target_map[source] = target_name

    self.WriteWithComment(self._GenerateModule(), f"{self.module.path}.rs")
