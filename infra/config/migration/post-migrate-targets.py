#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to do the end work of migrating targets to starlark.

Run this from the infra/config directory that should be modified.

After running migrate-targets.py, some manual work is necessary to
remove references to the migrated builders from waterfalls.pyl and
test_suite_exceptions.pyl. Once that is done, it's possible that there
are now test suites in test_suites.pyl or mixins in mixins.pyl are no
longer referenced by any builders in waterfalls.pyl. This will cause an
error in the presubmit builder, which checks that all test suites and
mixins are referenced.

This script will perform the necessary modifications to the starlark to
remove these errors.
"""

import ast
import bisect
import dataclasses
import os
import pathlib
import re
import subprocess
import sys
import typing

from lib import buildozer
from lib import post_migrate_targets
from lib import pyl

_INFRA_CONFIG_DIR = pathlib.Path(os.getcwd())
_TESTING_BUILDBOT_DIR = (_INFRA_CONFIG_DIR / '../../testing/buildbot').resolve()
_TARGETS_DIR = _INFRA_CONFIG_DIR / 'targets'


def _get_literal(path: pathlib.Path) -> pyl.Value:
  with open(path, encoding='utf-8') as f:
    nodes = pyl.parse(path, f.read())
  nodes = [n for n in nodes if isinstance(n, pyl.Value)]
  assert len(nodes) == 1
  return nodes[0]


def _escape_spaces(s: str) -> str:
  return s.replace(' ', '\\ ').replace('\n', '\\\n')


@dataclasses.dataclass
class _SuiteToMigrate:
  suite_type: str
  attrs: dict[str, str | None]


def _update_suites(suites_to_migrate: dict[str, _SuiteToMigrate]) -> None:
  bundles_star = _TARGETS_DIR / 'bundles.star'

  # Find the existing bundles so that we can determine where each bundle should
  # go to maintain sorted order (the file is already sorted)
  output = buildozer.run('print name', f'{bundles_star}:%targets.bundle')
  bundles = output.strip().split('\n')
  if sorted(bundles) != bundles:
    import difflib
    for l in difflib.unified_diff(sorted(bundles), bundles):
      print(l)
    raise Exception("rules in bundles.star aren't sorted")

  def get_before(bundle):
    idx = bisect.bisect(bundles, bundle)
    if idx == len(bundles):
      return ''
    return f'before {bundles[idx]}'

  for suite_name, suite in sorted(suites_to_migrate.items()):
    suites_star = _TARGETS_DIR / f'{suite.suite_type}.star'
    buildozer.run(f'new targets.bundle {suite_name} {get_before(suite_name)}',
                  f'{bundles_star}:__pkg__')
    for key, value in suite.attrs.items():
      if value is not None:
        buildozer.run(f'set {key} {_escape_spaces(value)}',
                      f'{bundles_star}:{suite_name}')
    buildozer.run('delete', f'{suites_star}:{suite_name}')


_SUITE_TYPE_HANDLERS = {
    'basic_suites': post_migrate_targets.convert_basic_suite,
    'compound_suites': post_migrate_targets.convert_compound_suite,
    'matrix_compound_suites':
    post_migrate_targets.convert_matrix_compound_suite,
}

# generate_buildbot_json.py outputs the unreferenced names like
# {'name1', 'name2', 'name3'} which can be conveniently evaluated as a set, so
# we capture everything inside the braces.
_UNREFERENCED_SUITES_RE = re.compile(
    'The following test suites were unreferenced by bots on the waterfalls: '
    r'(\{.+\})')

_UNREFERENCED_MIXINS_RE = re.compile(
    r'The following mixins are unreferenced: (\{.+\})')

_UNREFERENCED_VARIANTS_RE = re.compile(
    r'The following variants were unreferenced: (\{.+\})')


# check.py outputs the referenced names like name1, name2, name3
_UNREFERENCED_ISOLATES_RE = re.compile(
    '^(.+) (is|are) listed in gn_isolate_map.pyl but not in any .json files$')


def main():

  subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

  # Regenerate testing/buildbot .json files
  subprocess.check_call([_TESTING_BUILDBOT_DIR / 'generate_buildbot_json.py'])

  def check_testing_buildbot_generation() -> subprocess.CompletedProcess:
    return subprocess.run(
        [_TESTING_BUILDBOT_DIR / 'generate_buildbot_json.py', '--check'],
        capture_output=True,
        encoding='utf-8')

  ret = check_testing_buildbot_generation()

  try:
    match = _UNREFERENCED_SUITES_RE.search(ret.stderr)
    if match:
      unreferenced_suite_names = ast.literal_eval(match.group(1))
      test_suites = typing.cast(
          pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
          _get_literal(_INFRA_CONFIG_DIR / 'generated/testing/test_suites.pyl'))

      suites_to_migrate = {}
      for suite_type, handler in _SUITE_TYPE_HANDLERS.items():
        for suite_name, suite in test_suites.items:
          if suite_name.value in unreferenced_suite_names:
            suites_to_migrate[suite_name.value] = _SuiteToMigrate(
                suite_type=suite_type, attrs=handler(suite))

      _update_suites(suites_to_migrate)

      # Regenerating the configs updates test_suites.pyl so that
      # generate_buildbot_json.py --check should no longer complain about
      # unreferenced suites and allow us to see if there's any other errors
      subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

      ret = check_testing_buildbot_generation()

      if _UNREFERENCED_SUITES_RE.search(ret.stderr):
        raise Exception('unreferenced suites still exist after update')

    match = _UNREFERENCED_VARIANTS_RE.search(ret.stderr)
    if match:
      unreferenced_variant_names = ast.literal_eval(match.group(1))
      variants_star = _TARGETS_DIR / 'variants.star'
      buildozer.run(
          'set generate_pyl_entry False',
          *(f'{variants_star}:{variant_name}'
            for variant_name in unreferenced_variant_names))

      # Regenerating the configs updates variants.pyl so that
      # generate_buildbot_json.py --check should no longer complain about
      # unreferenced variants and allow us to see if there's any other errors
      subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

      ret = check_testing_buildbot_generation()

      if _UNREFERENCED_VARIANTS_RE.search(ret.stderr):
        raise Exception('unreferenced variants still exist after update')

    match = _UNREFERENCED_MIXINS_RE.search(ret.stderr)
    if match:
      unreferenced_mixin_names = ast.literal_eval(match.group(1))
      mixins_star = _TARGETS_DIR / 'mixins.star'
      buildozer.run(
          'set generate_pyl_entry False',
          *(f'{mixins_star}:{mixin_name}'
            for mixin_name in unreferenced_mixin_names))

      # Regenerating the configs updates mixins.pyl so that
      # generate_buildbot_json.py --check should no longer complain about
      # unreferenced mixins and allow us to see if there's any other errors
      subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

      ret = check_testing_buildbot_generation()

      if _UNREFERENCED_MIXINS_RE.search(ret.stderr):
        raise Exception('unreferenced mixins still exist after update')

    def check_check():
      return subprocess.run([_TESTING_BUILDBOT_DIR / 'check.py'],
                            capture_output=True,
                            encoding='utf-8')

    ret = check_check()

    match = _UNREFERENCED_ISOLATES_RE.match(ret.stderr)
    if match:
      unreferenced_isolate_names = sorted(match.group(1).split(', '))
      # Isolate entries can be created by binary, compile target or junit test
      # declarations and we don't know what declaration produced the entry just
      # from the name, so try them until we find one. Do tests last since there
      # are some tests that have the same name as binaries that wouldn't support
      # the necessary argument.
      files = [
          _TARGETS_DIR / 'binaries.star',
          _TARGETS_DIR / 'compile_targets.star',
          _TARGETS_DIR / 'tests.star',
      ]

      comment = 'All references have been moved to starlark'.replace(' ', r'\ ')
      for isolate_name in unreferenced_isolate_names:
        for file in files:
          success = buildozer.try_run('set skip_usage_check True',
                                      f'comment skip_usage_check {comment}',
                                      f'{file}:{isolate_name}')
          if success:
            break

      # Regenerating the configs updates gn_usolate_map.pyl so that check.py
      # should no longer complain about unreferenced isolates and allow us to
      # see if there's any other errors
      subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

      ret = check_check()

      if _UNREFERENCED_ISOLATES_RE.match(ret.stderr):
        raise Exception('unreferenced isolates still exist after update')

  finally:
    subprocess.check_call(['lucicfg', 'fmt'], cwd=_INFRA_CONFIG_DIR)

  subprocess.check_call([_INFRA_CONFIG_DIR / 'scripts/sync-pyl-files.py'])

  sys.stderr.write(ret.stderr)
  sys.exit(ret.returncode)


if __name__ == '__main__':
  main()
