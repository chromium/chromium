#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A script to do the end work of migrating targets to starlark.

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
import pathlib
import re
import subprocess
import sys
import typing

import buildozer
import values

_THIS_DIR = pathlib.Path(__file__).parent
_INFRA_CONFIG_DIR = _THIS_DIR.parent
_TESTING_BUILDBOT_DIR = (_INFRA_CONFIG_DIR / '../../testing/buildbot').resolve()


def _convert_basic_suite(
    suite: dict[str, dict[str, typing.Any]], ) -> dict[str, str | None]:
  targets_builder = values.ListValueBuilder()
  per_test_modifications_builder = values.DictValueBuilder()
  for test_name, test in suite.items():
    targets_builder.append(values.convert_direct(test_name))

    mixin_builder = values.CallValueBuilder('targets.mixin')
    per_test_modifications_builder[test_name] = mixin_builder

    for key, value in test.items():
      match key:
      # These keys are actually filled in by the declaration of the test in
      # starlark, so can't/shouldn't be part of the bundle definition
        case 'results_handler' | 'script' | 'test' | 'test_common':
          pass

        case 'swarming':
          mixin_builder['swarming'] = values.convert_swarming(value)

        case _:
          raise Exception(
              f'unhandled key in basic suite test definition: {key}')

  return {
      'targets': targets_builder.output(),
      'per_test_modifications': per_test_modifications_builder.output(),
  }


def _convert_compound_suite(suite: list[str]) -> dict[str, str | None]:
  return {'targets': values.to_output(values.convert_direct(suite))}


def _convert_matrix_compound_suite(
    suite: dict[str, dict[str, typing.Any]]) -> dict[str, str | None]:
  targets_builder = values.ListValueBuilder()
  for suite_name, matrix_config in suite.items():
    if not matrix_config:
      targets_builder.append(suite_name)
    else:
      bundle_builder = values.CallValueBuilder(
          'targets.bundle', {'targets': values.convert_direct(suite_name)})
      for key, value in matrix_config.items():
        match key:
          case 'mixins' | 'variants':
            bundle_builder[key] = values.convert_direct(value)

          case _:
            raise Exception(f'unhandled key in matrix config: {key}')
      targets_builder.append(bundle_builder)

  return {'targets': targets_builder.output()}


def _escape_spaces(s: str) -> str:
  return s.replace(' ', '\\ ').replace('\n', '\\\n')


@dataclasses.dataclass
class _SuiteToMigrate:
  suite_type: str
  attrs: dict[str, str | None]


def _update_suites(suites_to_migrate: dict[str, _SuiteToMigrate]) -> None:
  bundles_star = _INFRA_CONFIG_DIR / 'targets/bundles.star'

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
    suites_star = _INFRA_CONFIG_DIR / f'targets/{suite.suite_type}.star'
    buildozer.run(f'new targets.bundle {suite_name} {get_before(suite_name)}',
                  f'{bundles_star}:__pkg__')
    for key, value in suite.attrs.items():
      if value is not None:
        buildozer.run(f'set {key} {_escape_spaces(value)}',
                      f'{bundles_star}:{suite_name}')
    buildozer.run('delete', f'{suites_star}:{suite_name}')


_SUITE_TYPE_HANDLERS = {
    'basic_suites': _convert_basic_suite,
    'compound_suites': _convert_compound_suite,
    'matrix_compound_suites': _convert_matrix_compound_suite,
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


def main():

  # Regenerate testing/buildbot .json files
  subprocess.check_call([_TESTING_BUILDBOT_DIR / 'generate_buildbot_json.py'])

  def check_testing_buildbot_generation() -> subprocess.CompletedProcess:
    return subprocess.run(
        [_TESTING_BUILDBOT_DIR / 'generate_buildbot_json.py', '--check'],
        capture_output=True,
        encoding='utf-8')

  ret = check_testing_buildbot_generation()

  match = _UNREFERENCED_SUITES_RE.search(ret.stderr)
  if match:
    unreferenced_suite_names = ast.literal_eval(match.group(1))
    with open(_INFRA_CONFIG_DIR / 'generated/testing/test_suites.pyl',
              encoding='utf-8') as f:
      test_suites = ast.literal_eval(f.read())

    suites_to_migrate = {}
    for suite_type, handler in _SUITE_TYPE_HANDLERS.items():
      for suite_name, suite in test_suites.get(suite_type, {}).items():
        if suite_name in unreferenced_suite_names:
          suites_to_migrate[suite_name] = _SuiteToMigrate(suite_type=suite_type,
                                                          attrs=handler(suite))

    _update_suites(suites_to_migrate)

    # Regenerating the configs updates test_suites.pyl so that
    # generate_buildbot_json.py --check should no longer complain about
    # unreferenced suites and allow us to see if there's any other errors
    subprocess.check_call([_INFRA_CONFIG_DIR / 'main.star'])

    ret = check_testing_buildbot_generation()

    if _UNREFERENCED_SUITES_RE.search(ret.stderr):
      raise Exception('unreferenced suites still exist after update')

  match = _UNREFERENCED_MIXINS_RE.search(ret.stderr)
  if match:
    unreferenced_mixin_names = ast.literal_eval(match.group(1))
    mixins_star = _INFRA_CONFIG_DIR / 'targets/mixins.star'
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

  match = _UNREFERENCED_VARIANTS_RE.search(ret.stderr)
  if match:
    unreferenced_variant_names = ast.literal_eval(match.group(1))
    variants_star = _INFRA_CONFIG_DIR / 'targets/variants.star'
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

  subprocess.check_call([_INFRA_CONFIG_DIR / 'scripts/sync-pyl-files.py'])
  subprocess.check_call(['lucicfg', 'fmt'], cwd=_INFRA_CONFIG_DIR)

  print(ret.stderr, file=sys.stderr)
  sys.exit(ret.returncode)


if __name__ == '__main__':
  main()
