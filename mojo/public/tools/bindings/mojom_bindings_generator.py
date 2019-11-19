#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""

from __future__ import print_function

import argparse

try:
  import cPickle as pickle
except ImportError:
  import pickle

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

# Manually check for the command-line flag. (This isn't quite right, since it
# ignores, e.g., "--", but it's close enough.)
if "--use_bundled_pylibs" in sys.argv[1:]:
  sys.path.insert(0, os.path.join(_GetDirAbove("mojo"), "third_party"))

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "pylib"))

from mojom.error import Error
import mojom.fileutil as fileutil
from mojom.generate import template_expander
from mojom.generate import translate
from mojom.generate.generator import AddComputedData, WriteFile
from mojom.parse.conditional_features import RemoveDisabledDefinitions
from mojom.parse.parser import Parse

sys.path.append(
    os.path.join(_GetDirAbove("mojo"), "tools", "diagnosis"))
import crbug_1001171


_BUILTIN_GENERATORS = {
  "c++": "mojom_cpp_generator",
  "javascript": "mojom_js_generator",
  "java": "mojom_java_generator",
  "typescript": "mojom_ts_generator",
}


def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

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


def MakeImportStackMessage(imported_filename_stack):
  """Make a (human-readable) message listing a chain of imports. (Returned
  string begins with a newline (if nonempty) and does not end with one.)"""
  return ''.join(
      reversed(["\n  %s was imported by %s" % (a, b) for (a, b) in \
                    zip(imported_filename_stack[1:], imported_filename_stack)]))


class RelativePath(object):
  """Represents a path relative to the source tree."""
  def __init__(self, path, source_root):
    self.path = path
    self.source_root = source_root

  def relative_path(self):
    return os.path.relpath(os.path.abspath(self.path),
                           os.path.abspath(self.source_root))


def FindImportFile(rel_dir, file_name, search_rel_dirs):
  """Finds |file_name| in either |rel_dir| or |search_rel_dirs|. Returns a
  RelativePath with first file found, or an arbitrary non-existent file
  otherwise."""
  for rel_search_dir in [rel_dir] + search_rel_dirs:
    path = os.path.join(rel_search_dir.path, file_name)
    if os.path.isfile(path):
      return RelativePath(path, rel_search_dir.source_root)
  return RelativePath(os.path.join(rel_dir.path, file_name),
                      rel_dir.source_root)


def ScrambleMethodOrdinals(interfaces, salt):
  already_generated = set()
  for interface in interfaces:
    i = 0
    already_generated.clear()
    for method in interface.methods:
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
        sha256.update(interface.mojom_name)
        sha256.update(str(i))
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


class MojomProcessor(object):
  """Parses mojom files and creates ASTs for them.

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

  def _GenerateModule(self, args, remaining_args, generator_modules,
                      rel_filename, imported_filename_stack):
    # Return the already-generated module.
    if rel_filename.path in self._processed_files:
      return self._processed_files[rel_filename.path]

    if rel_filename.path in imported_filename_stack:
      print("%s: Error: Circular dependency" % rel_filename.path + \
          MakeImportStackMessage(imported_filename_stack + [rel_filename.path]))
      sys.exit(1)

    tree = _UnpickleAST(_FindPicklePath(rel_filename, args.gen_directories +
                                        [args.output_dir]))
    dirname = os.path.dirname(rel_filename.path)

    # Process all our imports first and collect the module object for each.
    # We use these to generate proper type info.
    imports = {}
    for parsed_imp in tree.import_list:
      rel_import_file = FindImportFile(
          RelativePath(dirname, rel_filename.source_root),
          parsed_imp.import_filename, args.import_directories)
      imports[parsed_imp.import_filename] = self._GenerateModule(
          args, remaining_args, generator_modules, rel_import_file,
          imported_filename_stack + [rel_filename.path])

    # Set the module path as relative to the source root.
    # Normalize to unix-style path here to keep the generators simpler.
    module_path = rel_filename.relative_path().replace('\\', '/')

    module = translate.OrderedModule(tree, module_path, imports)

    if args.scrambled_message_id_salt_paths:
      salt = ''.join(
          map(ReadFileContents, args.scrambled_message_id_salt_paths))
      ScrambleMethodOrdinals(module.interfaces, salt)

    if self._should_generate(rel_filename.path):
      AddComputedData(module)
      for language, generator_module in generator_modules.items():
        generator = generator_module.Generator(
            module, args.output_dir, typemap=self._typemap.get(language, {}),
            variant=args.variant, bytecode_path=args.bytecode_path,
            for_blink=args.for_blink,
            js_bindings_mode=args.js_bindings_mode,
            js_generate_struct_deserializers=\
                    args.js_generate_struct_deserializers,
            export_attribute=args.export_attribute,
            export_header=args.export_header,
            generate_non_variant_code=args.generate_non_variant_code,
            support_lazy_serialization=args.support_lazy_serialization,
            disallow_native_types=args.disallow_native_types,
            disallow_interfaces=args.disallow_interfaces,
            generate_message_ids=args.generate_message_ids,
            generate_fuzzing=args.generate_fuzzing,
            enable_kythe_annotations=args.enable_kythe_annotations)
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
      args.import_directories[idx] = RelativePath(tokens[0], tokens[1])
    else:
      args.import_directories[idx] = RelativePath(tokens[0], args.depth)
  generator_modules = LoadGenerators(args.generators_string)

  fileutil.EnsureDirectoryExists(args.output_dir)

  processor = MojomProcessor(lambda filename: filename in args.filename)
  processor.LoadTypemaps(set(args.typemaps))

  if args.filelist:
    with open(args.filelist) as f:
      args.filename.extend(f.read().split())

  for filename in args.filename:
    processor._GenerateModule(args, remaining_args, generator_modules,
                              RelativePath(filename, args.depth), [])

  return 0


def _FindPicklePath(rel_filename, search_dirs):
  filename, _ = os.path.splitext(rel_filename.relative_path())
  pickle_path = filename + '.p'
  for search_dir in search_dirs:
    path = os.path.join(search_dir, pickle_path)
    if os.path.isfile(path):
      return path
  raise Exception("%s: Error: Could not find file in %r" %
                  (pickle_path, search_dirs))


def _GetPicklePath(rel_filename, output_dir):
  filename, _ = os.path.splitext(rel_filename.relative_path())
  pickle_path = filename + '.p'
  return os.path.join(output_dir, pickle_path)


def _PickleAST(ast, output_file):
  full_dir = os.path.dirname(output_file)
  fileutil.EnsureDirectoryExists(full_dir)

  try:
    WriteFile(pickle.dumps(ast), output_file)
  except (IOError, pickle.PicklingError) as e:
    print("%s: Error: %s" % (output_file, str(e)))
    sys.exit(1)

def _UnpickleAST(input_file):
  try:
    with open(input_file, "rb") as f:
      return pickle.load(f)
  except (IOError, pickle.UnpicklingError) as e:
    print("%s: Error: %s" % (input_file, str(e)))
    sys.exit(1)


def _ParseFile(args, rel_filename):
  try:
    with open(rel_filename.path) as f:
      source = f.read()
  except IOError as e:
    print("%s: Error: %s" % (rel_filename.path, e.strerror))
    sys.exit(1)

  try:
    tree = Parse(source, rel_filename.path)
    RemoveDisabledDefinitions(tree, args.enabled_features)
  except Error as e:
    print("%s: Error: %s" % (rel_filename.path, str(e)))
    sys.exit(1)
  _PickleAST(tree, _GetPicklePath(rel_filename, args.output_dir))


def _Parse(args, _):
  fileutil.EnsureDirectoryExists(args.output_dir)

  if args.filelist:
    with open(args.filelist) as f:
      args.filename.extend(f.read().split())

  for filename in args.filename:
    _ParseFile(args, RelativePath(filename, args.depth))
  return 0


def _Precompile(args, _):
  generator_modules = LoadGenerators(",".join(_BUILTIN_GENERATORS.keys()))

  template_expander.PrecompileTemplates(generator_modules, args.output_dir)
  return 0

def GetSourcesList(target_prefix, sources_list, gen_dir):
  deps_list_path = target_prefix + ".deps_sources_list"
  f_deps_list = open(deps_list_path, 'r')
  for deps_sources_path in f_deps_list:
    target_name_with_dir = deps_sources_path.split(".sources_list")[0]
    if (target_name_with_dir == target_prefix):
      # add files from the target itself
      deps_sources_path = deps_sources_path.rstrip('\n')
      f_sources = open(deps_sources_path, 'r')
      for source_file in f_sources:
        full_source_path = os.path.dirname(target_name_with_dir.split(gen_dir \
        + "/", 1)[1]) + "/" + source_file
        sources_list.add(full_source_path.rstrip('\n'))
    else:
      # recurse into target's dependencies to get their lists of files
      sources_list = GetSourcesList(target_name_with_dir, sources_list, gen_dir)
  return sources_list

def _VerifyImportDeps(args, __):
  fileutil.EnsureDirectoryExists(args.gen_dir)

  if args.filelist:
    with open(args.filelist) as f:
      args.filename.extend(f.read().split())

  for filename in args.filename:
    rel_path = RelativePath(filename, args.depth)
    tree = _UnpickleAST(_GetPicklePath(rel_path, args.gen_dir))

    mojom_imports = set(
      parsed_imp.import_filename for parsed_imp in tree.import_list
      )

    sources = set()

    target_prefix = args.deps_file.split(".deps_sources_list")[0]
    sources = GetSourcesList(target_prefix, sources, args.gen_dir)

    if (not sources.issuperset(mojom_imports)):
      target_name = target_prefix.rsplit("/", 1)[1]
      target_prefix_without_gen_dir = target_prefix.split(
        args.gen_dir + "/", 1)[1]
      full_target_name = "//" + target_prefix_without_gen_dir.rsplit(
        "/", 1)[0] + ":" + target_name

      print(">>> File \"%s\"" % filename)
      print(">>> from target \"%s\"" % full_target_name)
      print(">>> is missing dependencies for the following imports:\n%s" % list(
          mojom_imports.difference(sources)))
      sys.exit(1)

    source_filename, _ = os.path.splitext(rel_path.relative_path())
    output_file = source_filename + '.v'
    output_file_path = os.path.join(args.gen_dir, output_file)
    WriteFile(b"", output_file_path)

  return 0

def main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("--use_bundled_pylibs", action="store_true",
                      help="use Python modules bundled in the SDK")

  subparsers = parser.add_subparsers()

  parse_parser = subparsers.add_parser(
      "parse", description="Parse mojom to AST and remove disabled definitions."
                           " Pickle pruned AST into output_dir.")
  parse_parser.add_argument("filename", nargs="*", help="mojom input file")
  parse_parser.add_argument("--filelist", help="mojom input file list")
  parse_parser.add_argument(
      "-o",
      "--output_dir",
      dest="output_dir",
      default=".",
      help="output directory for generated files")
  parse_parser.add_argument(
      "-d", "--depth", dest="depth", default=".", help="depth from source root")
  parse_parser.add_argument(
      "--enable_feature",
      dest = "enabled_features",
      default=[],
      action="append",
      help="Controls which definitions guarded by an EnabledIf attribute "
      "will be enabled. If an EnabledIf attribute does not specify a value "
      "that matches one of the enabled features, it will be disabled.")
  parse_parser.set_defaults(func=_Parse)

  generate_parser = subparsers.add_parser(
      "generate", description="Generate bindings from mojom files.")
  generate_parser.add_argument("filename", nargs="*",
                               help="mojom input file")
  generate_parser.add_argument("--filelist", help="mojom input file list")
  generate_parser.add_argument("-d", "--depth", dest="depth", default=".",
                               help="depth from source root")
  generate_parser.add_argument("-o", "--output_dir", dest="output_dir",
                               default=".",
                               help="output directory for generated files")
  generate_parser.add_argument("-g", "--generators",
                               dest="generators_string",
                               metavar="GENERATORS",
                               default="c++,javascript,java",
                               help="comma-separated list of generators")
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
      "--js_bindings_mode", choices=["new", "old"], default="old",
      help="This option only affects the JavaScript bindings. The value could "
      "be \"new\" to generate new-style lite JS bindings in addition to the "
      "old, or \"old\" to only generate old bindings.")
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
      "--support_lazy_serialization",
      help="If set, generated bindings will serialize lazily when possible.",
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
  precompile_parser.add_argument(
      "-o", "--output_dir", dest="output_dir", default=".",
      help="output directory for precompiled templates")
  precompile_parser.set_defaults(func=_Precompile)

  verify_parser = subparsers.add_parser("verify", description="Checks "
      "the set of imports against the set of dependencies.")
  verify_parser.add_argument("filename", nargs="*",
      help="mojom input file")
  verify_parser.add_argument("--filelist", help="mojom input file list")
  verify_parser.add_argument("-f", "--file", dest="deps_file",
      help="file containing paths to the sources files for "
      "dependencies")
  verify_parser.add_argument("-g", "--gen_dir",
      dest="gen_dir",
      help="directory with the syntax tree")
  verify_parser.add_argument(
      "-d", "--depth", dest="depth",
      help="depth from source root")

  verify_parser.set_defaults(func=_VerifyImportDeps)

  args, remaining_args = parser.parse_known_args()
  return args.func(args, remaining_args)


if __name__ == "__main__":
  with crbug_1001171.DumpStateOnLookupError():
    sys.exit(main())
