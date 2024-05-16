#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""

from __future__ import print_function

import argparse

import hashlib
import importlib
import json
import os
import pprint
import re
import struct
import sys

# Disable lint check for finding modules:
# pylint: disable=F0401

def _GetDirAbove(dirname):
  """Returns the directory "above" this file containing |dirname| (which must
  also be "above" this file)."""
  path = os.path.abspath(__file__)
  while True:
    path, tail = os.path.split(path)
    assert tail
    if tail == dirname:
      return path


sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mojom"))

from mojom.error import Error
import mojom.fileutil as fileutil
from mojom.generate.module import Module
from mojom.generate import template_expander
from mojom.generate import translate
from mojom.generate.generator import WriteFile

sys.path.append(
    os.path.join(_GetDirAbove("mojo"), "tools", "diagnosis"))
import crbug_1001171


_BUILTIN_GENERATORS = {
    "c++": "mojom_cpp_generator",
    "javascript": "mojom_js_generator",
    "java": "mojom_java_generator",
    "mojolpm": "mojom_mojolpm_generator",
    "rust": "mojom_rust_generator",
    "typescript": "mojom_ts_generator",
}

_BUILTIN_CHECKS = {
    "attributes": "mojom_attributes_check",
    "definitions": "mojom_definitions_check",
    "features": "mojom_interface_feature_check",
    "restrictions": "mojom_restrictions_check",
}


def LoadGenerators(generators_string):
  if not generators_string:
    return {}  # No generators.

  generators = {}
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    language = generator_name.lower()
    if language not in _BUILTIN_GENERATORS:
      print("Unknown generator name %s" % generator_name)
      sys.exit(1)
    generator_module = importlib.import_module(
        "generators.%s" % _BUILTIN_GENERATORS[language])
    generators[language] = generator_module
  return generators


def LoadChecks(checks_string):
  if not checks_string:
    return {}  # No checks.

  checks = {}
  for check_name in [s.strip() for s in checks_string.split(",")]:
    check = check_name.lower()
    if check not in _BUILTIN_CHECKS:
      print("Unknown check name %s" % check_name)
      sys.exit(1)
    check_module = importlib.import_module("checks.%s" % _BUILTIN_CHECKS[check])
    checks[check] = check_module
  return checks


def MakeImportStackMessage(imported_filename_stack):
  """Make a (human-readable) message listing a chain of imports. (Returned
  string begins with a newline (if nonempty) and does not end with one.)"""
  return ''.join(
      reversed(["\n  %s was imported by %s" % (a, b) for (a, b) in \
                    zip(imported_filename_stack[1:], imported_filename_stack)]))


class RelativePath:
  """Represents a path relative to the source tree or generated output dir."""

  def __init__(self, path, source_root, output_dir):
    self.path = path
    if path.startswith(source_root):
      self.root = source_root
    elif path.startswith(output_dir):
      self.root = output_dir
    else:
      raise Exception("Invalid input path %s" % path)

  def relative_path(self):
    return os.path.relpath(
        os.path.abspath(self.path), os.path.abspath(self.root))


def _GetModulePath(path, output_dir):
  return os.path.join(output_dir, path.relative_path() + '-module')


def ScrambleMethodOrdinals(interfaces, salt):
  already_generated = set()
  for interface in interfaces:
    i = 0
    already_generated.clear()
    for method in interface.methods:
      if method.explicit_ordinal is not None:
        continue
      while True:
        i = i + 1
        if i == 1000000:
          raise Exception("Could not generate %d method ordinals for %s" %
              (len(interface.methods), interface.mojom_name))
        # Generate a scrambled method.ordinal value. The algorithm doesn't have
        # to be very strong, cryptographically. It just needs to be non-trivial
        # to guess the results without the secret salt, in order to make it
        # harder for a compromised process to send fake Mojo messages.
        sha256 = hashlib.sha256(salt)
        sha256.update(interface.mojom_name.encode('utf-8'))
        sha256.update(str(i).encode('utf-8'))
        # Take the first 4 bytes as a little-endian uint32.
        ordinal = struct.unpack('<L', sha256.digest()[:4])[0]
        # Trim to 31 bits, so it always fits into a Java (signed) int.
        ordinal = ordinal & 0x7fffffff
        if ordinal in already_generated:
          continue
        already_generated.add(ordinal)
        method.ordinal = ordinal
        method.ordinal_comment = (
            'The %s value is based on sha256(salt + "%s%d").' %
            (ordinal, interface.mojom_name, i))
        break


def ReadFileContents(filename):
  with open(filename, 'rb') as f:
    return f.read()


class MojomProcessor:
  """Takes parsed mojom modules and generates language bindings from them.

  Attributes:
    _processed_files: {Dict[str, mojom.generate.module.Module]} Mapping from
        relative mojom filename paths to the module AST for that mojom file.
  """
  def __init__(self, should_generate):
    self._should_generate = should_generate
    self._processed_files = {}
    self._typemap = {}

  def LoadTypemaps(self, typemaps):
    # Support some very simple single-line comments in typemap JSON.
    comment_expr = r"^\s*//.*$"
    def no_comments(line):
      return not re.match(comment_expr, line)
    for filename in typemaps:
      with open(filename) as f:
        typemaps = json.loads("".join(filter(no_comments, f.readlines())))
        for language, typemap in typemaps.items():
          language_map = self._typemap.get(language, {})
          language_map.update(typemap)
          self._typemap[language] = language_map
    if 'c++' in self._typemap:
      self._typemap['mojolpm'] = self._typemap['c++']

  def _GenerateModule(self, args, remaining_args, check_modules,
                      generator_modules, rel_filename, imported_filename_stack):
    # Return the already-generated module.
    if rel_filename.path in self._processed_files:
      return self._processed_files[rel_filename.path]

    if rel_filename.path in imported_filename_stack:
      print("%s: Error: Circular dependency" % rel_filename.path + \
          MakeImportStackMessage(imported_filename_stack + [rel_filename.path]))
      sys.exit(1)

    module_path = _GetModulePath(rel_filename, args.output_dir)
    with open(module_path, 'rb') as f:
      module = Module.Load(f)

    if args.scrambled_message_id_salt_paths:
      salt = b''.join(
          map(ReadFileContents, args.scrambled_message_id_salt_paths))
      ScrambleMethodOrdinals(module.interfaces, salt)

    if self._should_generate(rel_filename.path):
      # Run checks on module first.
      for check_module in check_modules.values():
        checker = check_module.Check(module)
        checker.CheckModule()
      # Then run generation.
      for language, generator_module in generator_modules.items():
        generator = generator_module.Generator(
            module, args.output_dir, typemap=self._typemap.get(language, {}),
            variant=args.variant, bytecode_path=args.bytecode_path,
            for_blink=args.for_blink,
            js_generate_struct_deserializers=\
                    args.js_generate_struct_deserializers,
            export_attribute=args.export_attribute,
            export_header=args.export_header,
            generate_non_variant_code=args.generate_non_variant_code,
            disallow_native_types=args.disallow_native_types,
            disallow_interfaces=args.disallow_interfaces,
            generate_message_ids=args.generate_message_ids,
            generate_fuzzing=args.generate_fuzzing,
            enable_kythe_annotations=args.enable_kythe_annotations,
            extra_cpp_template_paths=args.extra_cpp_template_paths,
            generate_extra_cpp_only=args.generate_extra_cpp_only)
        filtered_args = []
        if hasattr(generator_module, 'GENERATOR_PREFIX'):
          prefix = '--' + generator_module.GENERATOR_PREFIX + '_'
          filtered_args = [arg for arg in remaining_args
                           if arg.startswith(prefix)]
        generator.GenerateFiles(filtered_args)

    # Save result.
    self._processed_files[rel_filename.path] = module
    return module


def _Generate(args, remaining_args):
  if args.variant == "none":
    args.variant = None

  for idx, import_dir in enumerate(args.import_directories):
    tokens = import_dir.split(":")
    if len(tokens) >= 2:
      args.import_directories[idx] = RelativePath(tokens[0], tokens[1],
                                                  args.output_dir)
    else:
      args.import_directories[idx] = RelativePath(tokens[0], args.depth,
                                                  args.output_dir)
  generator_modules = LoadGenerators(args.generators_string)
  check_modules = LoadChecks(args.checks_string)

  fileutil.EnsureDirectoryExists(args.output_dir)

  processor = MojomProcessor(lambda filename: filename in args.filename)
  processor.LoadTypemaps(set(args.typemaps))

  if args.filelist:
    with open(args.filelist) as f:
      args.filename.extend(f.read().split())

  for filename in args.filename:
    processor._GenerateModule(
        args, remaining_args, check_modules, generator_modules,
        RelativePath(filename, args.depth, args.output_dir), [])

  return 0


def _Precompile(args, _):
  generator_modules = LoadGenerators(",".join(_BUILTIN_GENERATORS.keys()))

  template_expander.PrecompileTemplates(generator_modules, args.output_dir)
  return 0


def main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("--use_bundled_pylibs", action="store_true",
                      help="use Python modules bundled in the SDK")
  parser.add_argument(
      "-o",
      "--output_dir",
      dest="output_dir",
      default=".",
      help="output directory for generated files")

  subparsers = parser.add_subparsers()

  generate_parser = subparsers.add_parser(
      "generate", description="Generate bindings from mojom files.")
  generate_parser.add_argument("filename", nargs="*",
                               help="mojom input file")
  generate_parser.add_argument("--filelist", help="mojom input file list")
  generate_parser.add_argument("-d", "--depth", dest="depth", default=".",
                               help="depth from source root")
  generate_parser.add_argument("-g",
                               "--generators",
                               dest="generators_string",
                               metavar="GENERATORS",
                               default="c++,javascript,java,mojolpm",
                               help="comma-separated list of generators")
  generate_parser.add_argument("-c",
                               "--checks",
                               dest="checks_string",
                               metavar="CHECKS",
                               default=",".join(_BUILTIN_CHECKS.keys()),
                               help="comma-separated list of checks")
  generate_parser.add_argument(
      "--gen_dir", dest="gen_directories", action="append", metavar="directory",
      default=[], help="add a directory to be searched for the syntax trees.")
  generate_parser.add_argument(
      "-I", dest="import_directories", action="append", metavar="directory",
      default=[],
      help="add a directory to be searched for import files. The depth from "
           "source root can be specified for each import by appending it after "
           "a colon")
  generate_parser.add_argument("--typemap", action="append", metavar="TYPEMAP",
                               default=[], dest="typemaps",
                               help="apply TYPEMAP to generated output")
  generate_parser.add_argument("--variant", dest="variant", default=None,
                               help="output a named variant of the bindings")
  generate_parser.add_argument(
      "--bytecode_path", required=True, help=(
          "the path from which to load template bytecode; to generate template "
          "bytecode, run %s precompile BYTECODE_PATH" % os.path.basename(
              sys.argv[0])))
  generate_parser.add_argument("--for_blink", action="store_true",
                               help="Use WTF types as generated types for mojo "
                               "string/array/map.")
  generate_parser.add_argument(
      "--js_generate_struct_deserializers", action="store_true",
      help="Generate javascript deserialize methods for structs in "
      "mojom-lite.js file")
  generate_parser.add_argument(
      "--export_attribute", default="",
      help="Optional attribute to specify on class declaration to export it "
      "for the component build.")
  generate_parser.add_argument(
      "--export_header", default="",
      help="Optional header to include in the generated headers to support the "
      "component build.")
  generate_parser.add_argument(
      "--generate_non_variant_code", action="store_true",
      help="Generate code that is shared by different variants.")
  generate_parser.add_argument(
      "--scrambled_message_id_salt_path",
      dest="scrambled_message_id_salt_paths",
      help="If non-empty, the path to a file whose contents should be used as"
      "a salt for generating scrambled message IDs. If this switch is specified"
      "more than once, the contents of all salt files are concatenated to form"
      "the salt value.", default=[], action="append")
  generate_parser.add_argument(
      "--extra_cpp_template_paths",
      dest="extra_cpp_template_paths",
      action="append",
      metavar="path_to_template",
      default=[],
      help="Provide a path to a new template (.tmpl) that is used to generate "
      "additional C++ source/header files ")
  generate_parser.add_argument(
      "--generate_extra_cpp_only",
      help="If set and extra_cpp_template_paths provided, will only generate"
      "extra_cpp_template related C++ bindings",
      action="store_true")
  generate_parser.add_argument(
      "--disallow_native_types",
      help="Disallows the [Native] attribute to be specified on structs or "
      "enums within the mojom file.", action="store_true")
  generate_parser.add_argument(
      "--disallow_interfaces",
      help="Disallows interface definitions within the mojom file. It is an "
      "error to specify this flag when processing a mojom file which defines "
      "any interface.", action="store_true")
  generate_parser.add_argument(
      "--generate_message_ids",
      help="Generates only the message IDs header for C++ bindings. Note that "
      "this flag only matters if --generate_non_variant_code is also "
      "specified.", action="store_true")
  generate_parser.add_argument(
      "--generate_fuzzing",
      action="store_true",
      help="Generates additional bindings for fuzzing in JS.")
  generate_parser.add_argument(
      "--enable_kythe_annotations",
      action="store_true",
      help="Adds annotations for kythe metadata generation.")

  generate_parser.set_defaults(func=_Generate)

  precompile_parser = subparsers.add_parser("precompile",
      description="Precompile templates for the mojom bindings generator.")
  precompile_parser.set_defaults(func=_Precompile)

  args, remaining_args = parser.parse_known_args()
  return args.func(args, remaining_args)


if __name__ == "__main__":
  with crbug_1001171.DumpStateOnLookupError():
    ret = main()
    # Exit without running GC, which can save multiple seconds due to the large
    # number of object created. But flush is necessary as os._exit doesn't do
    # that.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(ret)
