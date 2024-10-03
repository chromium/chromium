#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Migrate tests for builders from //testing/buildbot to starlark."""

import argparse
import ast
import pathlib
import subprocess
import sys
import typing

import buildozer
import values

_THIS_DIR = pathlib.Path(__file__).parent
_INFRA_CONFIG_DIR = _THIS_DIR.parent
_TESTING_BUILDBOT_DIR = (_INFRA_CONFIG_DIR / '../../testing/buildbot').resolve()


def _per_test_modifications(
    builder: str,
    test_suite_exceptions: dict[str, typing.Any],
) -> values.ValueBuilder:
  value_builder = values.DictValueBuilder()

  for test_name, exceptions in test_suite_exceptions.items():
    if builder in exceptions.get('remove_from', []):
      value_builder[test_name] = values.CallValueBuilder(
          'targets.remove',
          {
              # Break up the string so that it doesn't get flagged by the
              # presubmit check but the generated code will if not updated
              'reason': f'"DO{""} NOT SUBMIT provide an actual reason"',
          })

    elif modifications := exceptions.get('modifications', {}).get(builder):
      mixin_builder = values.CallValueBuilder('targets.mixin')
      value_builder[test_name] = mixin_builder

      for key, value in modifications.items():
        match key:
          case ('args' | 'ci_only' | 'experiment_percentage'
                | 'isolate_profile_data' | 'retry_only_failed_tests'):
            mixin_builder[key] = values.convert_direct(value)

          case 'swarming':
            mixin_builder['swarming'] = values.convert_swarming(value)

          case _:
            raise Exception(f'unhandled key in modifications: "{key}"')

  return value_builder


class SkylabSuite(Exception):

  def __init__(self, suite_type: str, suite: str):
    return super().__init__(f'skylab suite "{suite}" with'
                            f' suite_type "{suite_type}" is not supported')


_BROWSER_CONFIG_MAPPING = {
    'android-chromium': 'ANDROID_CHROMIUM',
    'android-chromium-monochrome': 'ANDROID_CHROMIUM_MONOCHROME',
    'android-webview': 'ANDROID_WEBVIEW',
    'cros-chrome': 'CROS_CHROME',
    'debug': 'DEBUG',
    'debug_x64': 'DEBUG_X64',
    'lacros-chrome': 'LACROS_CHROME',
    'release': 'RELEASE',
    'release_x64': 'RELEASE_X64',
    'web-engine-shell': 'WEB_ENGINE_SHELL',
}

_OS_TYPE_MAPPING = {
    'android': 'ANDROID',
    'chromeos': 'CROS',
    'fuchsia': 'FUCHSIA',
    'lacros': 'LACROS',
    'linux': 'LINUX',
    'mac': 'MAC',
    'win': 'WINDOWS',
}


def _compute_edits(
    builder: str,
    builder_config: dict[str, typing.Any],
    test_suite_exceptions: dict[str, typing.Any],
) -> dict[str, str] | None:
  anonymous_mixin_builder = values.CallValueBuilder('targets.mixin')
  mixins_builder = values.ListValueBuilder([anonymous_mixin_builder])
  bundle_builder = values.CallValueBuilder('targets.bundle',
                                           {'mixins': mixins_builder},
                                           output_empty=True)
  settings_builder = values.CallValueBuilder('targets.settings')

  for key, value in builder_config.items():
    match key:
      case 'test_suites':
        targets_builder = values.ListValueBuilder()
        bundle_builder['targets'] = targets_builder

        for suite_type, suite in value.items():
          match suite_type:
            case ('android_webview_gpu_telemetry_tests'
                  | 'cast_streaming_tests' | 'gpu_telemetry_tests'
                  | 'gtest_tests' | 'isolated_scripts' | 'scripts'):
              targets_builder.append(f'"{suite}"')

            case 'skylab_tests' | 'skylab_gpu_telemetry_tests':
              raise SkylabSuite(suite_type, suite)

            case 'junit_tests' | _:
              raise Exception(f'unhandled suite type: "{suite}"')

      case 'additional_compile_targets':
        bundle_builder[key] = values.convert_direct(value)

      case 'args':
        anonymous_mixin_builder['args'] = values.convert_direct(value)

      case 'mixins':
        for element in value:
          mixins_builder.append(f'"{element}"')

      case 'browser_config':
        browser_config = _BROWSER_CONFIG_MAPPING[value]
        settings_builder[key] = f'targets.browser_config.{browser_config}'

      case 'os_type':
        settings_builder[key] = f'targets.os_type.{_OS_TYPE_MAPPING[value]}'

      case 'skip_merge_script':
        if value:
          settings_builder['use_android_merge_script_by_default'] = str(False)

      case _:
        raise Exception(f'unhandled key in builder config: "{key}"')

  bundle_builder['per_test_modifications'] = _per_test_modifications(
      builder, test_suite_exceptions)

  edits = {'targets': ''.join(bundle_builder.output())}
  if (settings_output := settings_builder.output()) is not None:
    edits['targets_settings'] = ''.join(settings_output)

  return edits


def _escape_spaces(s: str) -> str:
  return s.replace(' ', '\\ ').replace('\n', '\\\n')


def _update_starlark(
    builder_group: str,
    star_file: pathlib.Path,
    targets_builder_defaults: dict[str, str],
    edits_by_builder: dict[str, dict[str, str]],
):

  # Ideally all modifications would be done with a single buildozer invocation
  # using a commands file, but escaped newlines aren't supported with commands
  # files.
  #
  # Pull request to fix: https://github.com/bazelbuild/buildtools/pull/1296

  # Buildozer is geared towards manipulating build targets, actions that operate
  # on the file level, such as adding a new rule, use __pkg__ as the target name
  file_target = f'{star_file}:__pkg__'

  buildozer.run('new_load //lib/targets.star targets', file_target)

  defaults_rule_kind = 'targets.builder_defaults.set'
  # %{kind} as the pattern tells it to operate on all rules of that kind. There
  # shouldn't be more than one targets.builder_defaults.set "rule".
  defaults_target = f'{star_file}:%{defaults_rule_kind}'
  # Check if a targets.builder_defaults.set declaration already exists, any
  # print operation will result in output if there is already a rule
  if not buildozer.run('print kind', defaults_target):
    # It's not possible to add an arbitrary function call, only new rules, which
    # require a name, so create a rule with a temporary name and then remove the
    # name attribute, then we can just use the kind filter for modifying it
    temp_name = 'NO_DECLARATION_SHOULD_EXIST_WITH_THIS_NAME'
    buildozer.run(
        f'new {defaults_rule_kind} {temp_name} before {builder_group}',
        file_target)
    buildozer.run('remove name', f'{star_file}:{temp_name}')

  for attr, value in targets_builder_defaults.items():
    buildozer.run(f'set {attr} {_escape_spaces(value)}', defaults_target)

  for builder, edits in edits_by_builder.items():
    for attr, value in edits.items():
      buildozer.run(f'set {attr} {_escape_spaces(value)}',
                    f'{star_file}:{builder}')


def main(argv: list[str]):
  parser = argparse.ArgumentParser()
  parser.add_argument('builder_group')
  parser.add_argument('builder', nargs='*', default=None)
  args = parser.parse_args(argv)

  builders = set(args.builder) or None

  with open(_TESTING_BUILDBOT_DIR / 'waterfalls.pyl', encoding='utf-8') as f:
    waterfalls = ast.literal_eval(f.read())
  for waterfall in waterfalls:
    if waterfall['name'] == args.builder_group:
      break
  else:
    print(f'builder_group "{args.builder_group}" not found', file=sys.stderr)
    sys.exit(1)

  with open(
      _TESTING_BUILDBOT_DIR / 'test_suite_exceptions.pyl',
      encoding='utf-8',
  ) as f:
    test_suite_exceptions = ast.literal_eval(f.read())

  targets_builder_defaults = {}
  edits_by_builder = {}
  for key, value in waterfall.items():
    match key:
      case 'project' | 'bucket' | 'name':
        pass

      case 'mixins':
        mixins_default_builder = values.ListValueBuilder(
            [f'"{m}"' for m in value])
        targets_builder_defaults['mixins'] = mixins_default_builder.output()

      case 'machines':
        for builder_name, builder_config in value.items():
          if builders is not None and builder_name not in builders:
            continue

          edits_by_builder[builder_name] = _compute_edits(
              builder_name,
              builder_config,
              test_suite_exceptions,
          )

          if builders is not None:
            builders.remove(builder_name)
            if not builders:
              break

      case 'forbid_script_tests':
        # TODO: crbug.com/40258588: Implement support for forbid_script_test in
        # starlark and handle this
        pass

      case _:
        raise Exception(f'unhandled key in waterfall: "{key}"')

  if builders:
    builder_message = ', '.join(f'"{b}"' for b in sorted(builders))
    print(("the following builders don't exist in builder group "
           f'"{args.builder_group}": {builder_message}'),
          file=sys.stderr)
    sys.exit(1)

  bucket = 'try' if args.builder_group.startswith('tryserver.') else 'ci'
  star_file = (_INFRA_CONFIG_DIR /
               f'subprojects/chromium/{bucket}/{args.builder_group}.star')

  _update_starlark(
      args.builder_group,
      star_file,
      targets_builder_defaults,
      edits_by_builder,
  )

  subprocess.check_call(['lucicfg', 'fmt'], cwd=_INFRA_CONFIG_DIR)


if __name__ == '__main__':
  main(sys.argv[1:])
