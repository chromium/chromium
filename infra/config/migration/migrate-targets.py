#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Migrate tests for builders from //testing/buildbot to starlark.

Run this from the infra/config directory that should be modified.
"""

import argparse
import glob
import json
import os
import pathlib
import subprocess
import sys
import typing

from lib import migrate_targets
from lib import pyl

_INFRA_CONFIG_DIR = pathlib.Path(os.getcwd())
_TESTING_BUILDBOT_DIR = (_INFRA_CONFIG_DIR / '../../testing/buildbot').resolve()


def _get_literal(path: pathlib.Path) -> pyl.Value:
  with open(path, encoding='utf-8') as f:
    nodes = pyl.parse(path, f.read())
  nodes = [n for n in nodes if isinstance(n, pyl.Value)]
  assert len(nodes) == 1
  return nodes[0]


def main(argv: list[str]):
  parser = argparse.ArgumentParser()
  parser.add_argument('builder_group')
  parser.add_argument('builder', nargs='*', default=None)
  parser.add_argument('--star-file', default=None)
  args = parser.parse_args(argv)

  builders = set(args.builder) or None

  waterfalls = _get_literal(_TESTING_BUILDBOT_DIR / 'waterfalls.pyl')
  test_suite_exceptions = _get_literal(_TESTING_BUILDBOT_DIR /
                                       'test_suite_exceptions.pyl')

  try:
    edits = migrate_targets.process_waterfall(
        args.builder_group,
        builders,
        typing.cast(pyl.List[pyl.Dict[pyl.Str, pyl.Value]], waterfalls),
        typing.cast(pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
                    test_suite_exceptions),
    )
  except migrate_targets.WaterfallError as e:
    print(e, file=sys.stderr)
    sys.exit(1)

  if args.star_file:
    if not os.path.exists(args.star_file):
      print(f'The given starlark file does not exist: "{args.star_file}"',
            file=sys.stderr)
      sys.exit(1)
    star_file = pathlib.Path(args.star_file)
  else:
    bucket = 'try' if args.builder_group.startswith('tryserver.') else 'ci'
    star_file = (_INFRA_CONFIG_DIR /
                 f'subprojects/chromium/{bucket}/{args.builder_group}.star')

  migrate_targets.update_starlark(
      args.builder_group,
      star_file,
      edits,
  )

  subprocess.check_call(['lucicfg', 'fmt'], cwd=_INFRA_CONFIG_DIR)

  # Regenerate the configs
  subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])
  subprocess.check_call([_INFRA_CONFIG_DIR / 'dev.star'])

  # Copy the relevant portions of the testing/buildbot json files to the
  # newly-generated json files to make it easy to compare what's different
  unmigrated_jsons = {
      name: None
      for name in glob.glob('*.json', root_dir=_TESTING_BUILDBOT_DIR)
  }
  not_present = object()
  for json_file in glob.glob('generated/*/*/*/targets/*.json',
                             root_dir=_INFRA_CONFIG_DIR):
    json_file = _INFRA_CONFIG_DIR / json_file

    unmigrated_json = unmigrated_jsons.get(json_file.name, not_present)
    if unmigrated_json is not_present:
      continue
    if unmigrated_json is None:
      with open(_TESTING_BUILDBOT_DIR / json_file.name) as f:
        unmigrated_json = unmigrated_jsons[json_file.name] = json.load(f)

    with open(json_file) as f:
      migrated_json = json.load(f)

    for builder in migrated_json:
      if builder in unmigrated_json:
        migrated_json[builder] = unmigrated_json[builder]

    with open(json_file, 'w') as f:
      json.dump(migrated_json, f, indent=2, sort_keys=True)

  # Add the files to the git index, then regenerate the configs, this will make
  # it easy to check what is different between the test definitions between
  # starlark and generate_buildbot_json.py
  subprocess.check_call(['git', 'add', '.'], cwd=_INFRA_CONFIG_DIR)

  # Regenerate the configs
  subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])
  subprocess.check_call([_INFRA_CONFIG_DIR / 'dev.star'])


if __name__ == '__main__':
  main(sys.argv[1:])
