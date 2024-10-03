#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs checks on the files defining tests.

This performs the following checks:
* Checks that any entry in gn_isolate_map.pyl is referenced by some
  builder (modulo targets known to be used by builders in other projects
  or via other mechanisms).
* Checks that any target referenced by a builder is defined in
  gn_isolate_map.pyl (module magic targets).
"""

import argparse
import ast
import glob
import json
import os
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))


SKIP_GN_ISOLATE_MAP_TARGETS = {
    # This target is magic and not present in gn_isolate_map.pyl.
    'all',
    'remoting/client:client',
    'remoting/host:host',

    # These targets are only used by script tests
    'traffic_annotation_proto',

    # These targets are listed only in build-side recipes.
    'captured_sites_interactive_tests',
    'chrome_official_builder_no_unittests',
    'mini_installer',
    'previous_version_mini_installer',

    # These are used elsewhere.
    'media_router_e2e_tests',
    'traffic_annotation_auditor_dependencies',
    'vr_common_perftests',
    'vrcore_fps_test',

    # These are only run on V8 CI.
    'postmortem-metadata',

    # These are only for developer convenience and not on any bots.
    'telemetry_gpu_integration_test_scripts_only',

    # These are defined by an android internal gn_isolate_map.pyl file.
    'resource_sizes_monochrome_minimal_apks',
    'resource_sizes_trichrome_google',
    'resource_sizes_system_webview_google_bundle',
    'trichrome_google_64_32_minimal_apks',

    # These are used by https://www.chromium.org/developers/cluster-telemetry.
    'ct_telemetry_perf_tests_without_chrome',
}


class Error(Exception):
  """Processing error."""


def check_file(filepath, ninja_targets, ninja_targets_seen):
  """Processes a json file describing what tests should be run for each recipe.

  Raises an Error if the file doesn't pass checks.
  """
  filename = os.path.basename(filepath)
  with open(filepath) as f:
    content = f.read()
  try:
    config = json.loads(content)
  except ValueError as e:
    raise Error('Exception raised while checking %s: %s' % (filepath, e)) from e

  for builder, data in sorted(config.items()):
    if not isinstance(data, dict):
      raise Error('%s: %s is broken: %s' % (filename, builder, data))
    if ('gtest_tests' not in data and
        'isolated_scripts' not in data and
        'additional_compile_targets' not in data and
        'instrumentation_tests' not in data):
      continue

    for d in data.get('junit_tests', []):
      test = d['test']
      if (test not in ninja_targets and
          test not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl' %
                    (filename, builder, test))
      if test in ninja_targets:
        ninja_targets_seen.add(test)

    for target in data.get('additional_compile_targets', []):
      if (target not in ninja_targets and
          target not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl' %
                    (filename, builder, target))
      if target in ninja_targets:
        ninja_targets_seen.add(target)

    seen = set()
    for d in data.get('gtest_tests', []):
      test = d['test']
      if (test not in ninja_targets and
          test not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, test))
      if test in ninja_targets:
        ninja_targets_seen.add(test)

      name = d.get('name', d['test'])
      if name in seen:
        raise Error('%s: %s / %s is listed multiple times.' %
                    (filename, builder, name))
      seen.add(name)

    for d in data.get('isolated_scripts', []):
      name = d['test']
      if (name not in ninja_targets and
          name not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, name))
      if name in ninja_targets:
        ninja_targets_seen.add(name)

    for d in (data.get('instrumentation_tests', []) +
              data.get('skylab_tests', [])):
      name = d['test']
      if (name not in ninja_targets and
          name not in SKIP_GN_ISOLATE_MAP_TARGETS):
        raise Error('%s: %s / %s is not listed in gn_isolate_map.pyl.' %
                    (filename, builder, name))
      if name in ninja_targets:
        ninja_targets_seen.add(name)


def main():
  parser = argparse.ArgumentParser(description=sys.modules[__name__].__doc__)
  parser.parse_args()

  gn_isolate_map_pyl_path = os.path.normpath(
      os.path.join(THIS_DIR, '..', '..', 'infra', 'config', 'generated',
                   'testing', 'gn_isolate_map.pyl'))
  with open(gn_isolate_map_pyl_path) as fp:
    gn_isolate_map = ast.literal_eval(fp.read())
    ninja_targets = {k: v['label'] for k, v in gn_isolate_map.items()}

  try:
    ninja_targets_seen = set()
    for filepath in glob.glob(os.path.join(THIS_DIR, '*.json')):
      # This file is formatted differently from other json files
      if 'autoshard_exceptions' in filepath:
        continue
      check_file(filepath, ninja_targets, ninja_targets_seen)

    skip_targets = [k for k, v in gn_isolate_map.items() if
                    ('skip_usage_check' in v and v['skip_usage_check'])]
    extra_targets = (set(ninja_targets) - set(skip_targets) -
                     ninja_targets_seen - SKIP_GN_ISOLATE_MAP_TARGETS)
    if extra_targets:
      if len(extra_targets) > 1:
        extra_targets_str = ', '.join(extra_targets) + ' are'
      else:
        extra_targets_str = list(extra_targets)[0] + ' is'
      raise Error('%s listed in gn_isolate_map.pyl but not in any .json '
                  'files' % extra_targets_str)

    return 0
  except Error as e:
    sys.stderr.write('%s\n' % e)
    return 1


if __name__ == '__main__':
  sys.exit(main())
