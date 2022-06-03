# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Typescript source files from a mojom.Module."""

import argparse
import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja
import os

GENERATOR_PREFIX = 'ts'

_kind_to_typescript_type = {
  mojom.BOOL:                  "boolean",
  mojom.INT8:                  "number",
  mojom.UINT8:                 "number",
  mojom.INT16:                 "number",
  mojom.UINT16:                "number",
  mojom.INT32:                 "number",
  mojom.UINT32:                "number",
  mojom.FLOAT:                 "number",
  mojom.INT64:                 "bigint",
  mojom.UINT64:                "bigint",
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

class TypescriptStylizer(generator.Stylizer):
  def StylizeModule(self, mojom_namespace):
    return '.'.join(generator.ToCamel(word, lower_initial=True)
                        for word in mojom_namespace.split('.'))

  def StylizeField(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeConstant(self, mojom_name):
    return generator.ToUpperSnakeCase(mojom_name)

class Generator(generator.Generator):
  def _GetParameters(self, use_es_modules=False):
    return {
        "module": self.module,
        "use_es_modules": use_es_modules,
        "enums": self.module.enums,
        "structs": self.module.structs,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "ts_templates"

  def GetFilters(self):
    ts_filters = {
        "typescript_type_with_nullability": self._TypescriptTypeWithNullability,
        "constant_value": self._ConstantValue,
        "relative_path": self._RelativePath,
    }
    return ts_filters

  @UseJinja("mojom.tmpl")
  def _GenerateBindings(self):
    return self._GetParameters()

  @UseJinja("mojom.tmpl")
  def _GenerateESModulesBindings(self):
    return self._GetParameters(use_es_modules=True)

  def GenerateFiles(self, unparsed_args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    parser = argparse.ArgumentParser()
    parser.add_argument('--ts_use_es_modules',
                        action="store_true",
                        help="Generate bindings that use ES Modules.")
    args = parser.parse_args(unparsed_args)

    self.module.Stylize(TypescriptStylizer())

    self._SetUniqueAliasesForImports()

    if args.ts_use_es_modules == True:
      self.Write(self._GenerateESModulesBindings(),
                 "%s-lite.m.ts" % self.module.path)
    else:
      self.Write(self._GenerateBindings(), "%s-lite.ts" % self.module.path)

  # Function that sets unique alias names for all imported modules based on
  # their namespace. For example, for two imported modules with namespace
  # "mojo.foo", we would generate two aliases "mojoFoo" and "mojoFoo$0".
  def _SetUniqueAliasesForImports(self):
    used_aliases = set()
    for each_import in self.module.imports:
      # Per the style guide module aliases start with lower case.
      simple_alias = generator.ToCamel(identifier=each_import.namespace,
                                       lower_initial=True,
                                       delimiter='.')
      unique_alias = simple_alias
      counter = 0
      while unique_alias in used_aliases:
        unique_alias = '%s$%d' % (simple_alias, counter)
        counter += 1

      used_aliases.add(unique_alias)
      each_import.unique_alias = unique_alias

  def _TypescriptType(self, kind, use_es_modules):
    if kind in mojom.PRIMITIVES:
      return _kind_to_typescript_type[kind]

    if mojom.IsStructKind(kind):
      if kind.module and kind.module.path != self.module.path:
        namespace = (kind.module.unique_alias
                     if use_es_modules else kind.module.namespace)
        return '.'.join([namespace, kind.name])

      return kind.name

    raise Exception("Type is not supported yet.")

  def _TypescriptTypeWithNullability(self, kind, use_es_modules):
    return (self._TypescriptType(kind, use_es_modules) +
            (" | null" if mojom.IsNullableKind(kind) else ""))

  def _ConstantValue(self, constant):
    value = constant.value
    if isinstance(value, (mojom.EnumValue, mojom.NamedValue)):
      # TODO(crbug.com/1008761): Support EnumValue and NamedValue.
      raise Exception("Constant value not supported yet. %s" % value)

    if isinstance(value, mojom.BuiltinValue):
      if value.value == "double.INFINITY" or value.value == "float.INFINITY":
        return "Infinity";
      if value.value == "double.NEGATIVE_INFINITY" or \
         value.value == "float.NEGATIVE_INFINITY":
        return "-Infinity";
      if value.value == "double.NAN" or value.value == "float.NAN":
        return "NaN";
      raise Exception("Unknown BuiltinValue: %s" % value.value)

    if constant.kind == mojom.INT64 or constant.kind == mojom.UINT64:
      return "BigInt('%s')" % value

    return value

  def _RelativePath(self, path):
    return os.path.relpath(path, os.path.dirname(self.module.path))
