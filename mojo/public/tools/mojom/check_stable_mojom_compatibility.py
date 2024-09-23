#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies backward-compatibility of mojom type changes.

Given a set of pre- and post-diff mojom file contents, and a root directory
for a project, this tool verifies that any changes to [Stable] mojom types are
backward-compatible with the previous version.

This can be used e.g. by a presubmit check to prevent developers from making
breaking changes to stable mojoms."""

import argparse
import io
import json
import os
import os.path
import sys

from mojom.generate import compatibility_checker
from mojom.generate import module
from mojom.generate import translate
from mojom.parse import parser

# pylint: disable=raise-missing-from


class ParseError(Exception):
  pass


def _ValidateDelta(root, delta):
  """Parses all modified mojoms (including all transitive mojom dependencies,
  even if unmodified) to perform backward-compatibility checks on any types
  marked with the [Stable] attribute.

  Note that unlike the normal build-time parser in mojom_parser.py, this does
  not produce or rely on cached module translations, but instead parses the full
  transitive closure of a mojom's input dependencies all at once.
  """

  translate.is_running_backwards_compatibility_check_hack = True

  # First build a map of all files covered by the delta
  affected_files = set()
  old_files = {}
  new_files = {}
  for change in delta:
    # TODO(crbug.com/40623602): Use pathlib once we're migrated fully to
    # Python 3.
    filename = change['filename'].replace('\\', '/')
    affected_files.add(filename)
    if change['old']:
      old_files[filename] = change['old']
    if change['new']:
      new_files[filename] = change['new']

  # Parse and translate all mojoms relevant to the delta, including transitive
  # imports that weren't modified.
  unmodified_modules = {}

  def parseMojom(mojom, file_overrides, override_modules):
    if mojom in unmodified_modules or mojom in override_modules:
      return

    contents = file_overrides.get(mojom)
    if contents:
      modules = override_modules
    else:
      modules = unmodified_modules
      with io.open(os.path.join(root, mojom), encoding='utf-8') as f:
        contents = f.read()

    try:
      ast = parser.Parse(contents, mojom)
    except Exception as e:
      raise ParseError('encountered exception {0} while parsing {1}'.format(
          e, mojom))

    # Files which are generated at compile time can't be checked by this script
    # (at the moment) since they may not exist in the output directory.
    generated_files_to_skip = {
        ('third_party/blink/public/mojom/runtime_feature_state/'
         'runtime_feature.mojom'),
        ('third_party/blink/public/mojom/origin_trial_feature/'
         'origin_trial_feature.mojom'),
    }

    ast.import_list.items = [
        x for x in ast.import_list.items
        if x.import_filename not in generated_files_to_skip
    ]

    for imp in ast.import_list:
      if (not file_overrides.get(imp.import_filename)
          and not os.path.exists(os.path.join(root, imp.import_filename))):
        # Speculatively construct a path prefix to locate the import_filename
        mojom_path = os.path.dirname(os.path.normpath(mojom)).split(os.sep)
        test_prefix = ''
        for path_component in mojom_path:
          test_prefix = os.path.join(test_prefix, path_component)
          test_import_filename = os.path.join(test_prefix, imp.import_filename)
          if os.path.exists(os.path.join(root, test_import_filename)):
            imp.import_filename = test_import_filename
            break
      parseMojom(imp.import_filename, file_overrides, override_modules)

    # Now that the transitive set of dependencies has been imported and parsed
    # above, translate each mojom AST into a Module so that all types are fully
    # defined and can be inspected.
    all_modules = {}
    all_modules.update(unmodified_modules)
    all_modules.update(override_modules)
    modules[mojom] = translate.OrderedModule(ast, mojom, all_modules)

  old_modules = {}
  for mojom in old_files:
    parseMojom(mojom, old_files, old_modules)
  new_modules = {}
  for mojom in new_files:
    parseMojom(mojom, new_files, new_modules)

  # At this point we have a complete set of translated Modules from both the
  # pre- and post-diff mojom contents. Now we can analyze backward-compatibility
  # of the deltas.
  #
  # Note that for backward-compatibility checks we only care about types which
  # were marked [Stable] before the diff. Types newly marked as [Stable] are not
  # checked.
  def collectTypes(modules):
    types = {}
    for m in modules.values():
      for kinds in (m.enums, m.structs, m.unions, m.interfaces):
        for kind in kinds:
          types[kind.qualified_name] = kind
    return types

  old_types = collectTypes(old_modules)
  new_types = collectTypes(new_modules)

  # Collect any renamed types so they can be compared accordingly.
  renamed_types = {}
  for name, kind in new_types.items():
    old_name = kind.attributes and kind.attributes.get('RenamedFrom')
    if old_name:
      renamed_types[old_name] = name

  for qualified_name, kind in old_types.items():
    if not kind.stable:
      continue

    new_name = renamed_types.get(qualified_name, qualified_name)
    if new_name not in new_types:
      raise Exception(
          'Stable type %s appears to be deleted by this change. If it was '
          'renamed, please add a [RenamedFrom] attribute to the new type. This '
          'can be deleted by a subsequent change.' % qualified_name)

    checker = compatibility_checker.BackwardCompatibilityChecker()
    try:
      if not checker.IsBackwardCompatible(new_types[new_name], kind):
        raise Exception(
            'Stable type %s appears to have changed in a way which '
            'breaks backward-compatibility. Please fix!\n\nIf you '
            'believe this assessment to be incorrect, please file a '
            'Chromium bug against the "Internals>Mojo>Bindings" '
            'component.' % qualified_name)
    except Exception as e:
      raise Exception(
          'Stable type %s appears to have changed in a way which '
          'breaks backward-compatibility: \n\n%s.\nPlease fix!\n\nIf you '
          'believe this assessment to be incorrect, please file a '
          'Chromium bug against the "Internals>Mojo>Bindings" '
          'component.' % (qualified_name, e))


def Run(command_line, delta=None):
  """Runs the tool with the given command_line. Normally this will read the
  change description from stdin as a JSON-encoded list, but tests may pass a
  delta directly for convenience."""
  arg_parser = argparse.ArgumentParser(
      description='Verifies backward-compatibility of mojom type changes.',
      epilog="""
This tool reads a change description from stdin and verifies that all modified
[Stable] mojom types will retain backward-compatibility. The change description
must be a JSON-encoded list of objects, each with a "filename" key (path to a
changed mojom file, relative to ROOT); an "old" key whose value is a string of
the full file contents before the change, or null if the file is being added;
and a "new" key whose value is a string of the full file contents after the
change, or null if the file is being deleted.""")
  arg_parser.add_argument(
      '--src-root',
      required=True,
      action='store',
      metavar='ROOT',
      help='The root of the source tree in which the checked mojoms live.')

  args, _ = arg_parser.parse_known_args(command_line)
  if not delta:
    delta = json.load(sys.stdin)
  _ValidateDelta(args.src_root, delta)


if __name__ == '__main__':
  Run(sys.argv[1:])
