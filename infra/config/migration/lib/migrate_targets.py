#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Library of code for migrating builder's targets from waterfalls.pyl."""

import collections
import dataclasses
import pathlib
import typing

from . import buildozer
from . import pyl
from . import starlark_conversions
from . import values


def _per_test_modifications(
    builder: str,
    test_suite_exceptions: pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
) -> values.Value:

  def mod_builder_factory():
    return values.CallValueBuilder('targets.per_test_modification',
                                   elide_param='mixins')

  mod_builders = collections.defaultdict(mod_builder_factory)

  for test_name, exceptions in test_suite_exceptions.items:
    test_name = starlark_conversions.convert_direct(test_name)
    for key, value in exceptions.items:
      match key.value:
        case 'remove_from':
          value = typing.cast(pyl.List[pyl.Str], value)
          for builder_name in value.elements:
            if builder_name.value == builder:
              mod_builders[test_name] = values.CallValueBuilder(
                  'targets.remove',
                  # Break up the string so that it doesn't get flagged by the
                  # presubmit check but the generated code will if not updated
                  {'reason': f'"DO{""} NOT SUBMIT provide an actual reason"'})
              break

        case 'modifications':
          value = typing.cast(pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
                              value)
          for builder_name, mods in value.items:
            if builder_name.value == builder:
              break
          else:
            continue

          mixin_builder = values.CallValueBuilder('targets.mixin')
          mod_builders[test_name]['mixins'] = mixin_builder

          for mod_key, mod_value in mods.items:
            match mod_key.value:
              case ('ci_only' | 'experiment_percentage'
                    | 'isolate_profile_data' | 'retry_only_failed_tests'):
                mixin_builder[mod_key.value] = (
                    starlark_conversions.convert_direct(mod_value))

              case 'args':
                mod_value = typing.cast(pyl.List[pyl.Str], mod_value)
                mixin_builder[mod_key.value] = (
                    starlark_conversions.convert_args(mod_value))

              case 'swarming':
                mod_value = typing.cast(pyl.Dict[pyl.Str, pyl.Value], mod_value)
                mixin_builder[mod_key.value] = (
                    starlark_conversions.convert_swarming(mod_value))

              case _:
                raise Exception(
                    f'{mod_key.start}: unhandled key in modifications: "{mod_key.value}"'
                )

        case 'replacements':
          value = typing.cast(
              pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Str]]],
              value)
          for builder_name, replacements in value.items:
            if builder_name.value == builder:
              break
          else:
            continue

          replacements_builder = (
              values.CallValueBuilder('targets.replacements'))
          mod_builders[test_name]['replacements'] = replacements_builder

          for replace_key, replace_value in replacements.items:
            match replace_key.value:
              case 'args' | 'precommit_args' | 'non_precommit_args':
                args_builder = values.DictValueBuilder()
                for arg_name, arg_value in replace_value.items:
                  args_builder[starlark_conversions.convert_arg(arg_name)] = (
                      starlark_conversions.convert_direct(arg_value))
                replacements_builder[replace_key.value] = args_builder

              case _:
                raise Exception(
                    f'{replace_key.start}: unhandled key in replacements: "{replace_key.value}"'
                )

        case _:
          raise Exception(
              f'{key.start}: unhandled key in test_suite_exceptions: "{key.value}"'
          )

  return values.DictValueBuilder(mod_builders)


_BROWSER_CONFIG_MAPPING = {
    'android-chromium': 'ANDROID_CHROMIUM',
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
    builder_config: pyl.Dict[pyl.Str, pyl.Value],
    test_suite_exceptions: pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
) -> dict[str, str]:
  anonymous_mixin_builder = values.CallValueBuilder('targets.mixin')
  mixins_builder = values.ListValueBuilder([anonymous_mixin_builder])
  bundle_builder = values.CallValueBuilder('targets.bundle',
                                           {'mixins': mixins_builder},
                                           output_empty=True)

  skylab_mixin_builder = values.CallValueBuilder('targets.mixin')
  skylab_bundle_builder = values.CallValueBuilder(
      'targets.bundle', {'mixins': skylab_mixin_builder})

  settings_builder = values.CallValueBuilder('targets.settings')

  for key, value in builder_config.items:
    match key.value:
      case 'test_suites':
        value = typing.cast(pyl.Dict[pyl.Str, pyl.Str], value)

        skylab_targets_builder = values.ListValueBuilder()
        skylab_bundle_builder['targets'] = skylab_targets_builder

        targets_builder = values.ListValueBuilder([skylab_bundle_builder])
        bundle_builder['targets'] = targets_builder

        for suite_type, suite in value.items:
          match suite_type.value:
            case ('android_webview_gpu_telemetry_tests'
                  | 'cast_streaming_tests' | 'gpu_telemetry_tests'
                  | 'gtest_tests' | 'isolated_scripts' | 'scripts'):
              targets_builder.append(starlark_conversions.convert_direct(suite))

            case 'skylab_tests' | 'skylab_gpu_telemetry_tests':
              skylab_targets_builder.append(
                  starlark_conversions.convert_direct(suite))
              settings_builder['use_swarming'] = 'False'

            case 'junit_tests' | _:
              raise Exception(
                  f'{suite_type.start}: unhandled suite type: "{suite_type.value}"'
              )

      case 'additional_compile_targets':
        bundle_builder[key.value] = starlark_conversions.convert_direct(value)

      case 'args':
        value = typing.cast(pyl.List[pyl.Str], value)
        anonymous_mixin_builder['args'] = (
            starlark_conversions.convert_args(value))

      case 'mixins':
        value = typing.cast(pyl.List[pyl.Str], value)
        for element in value.elements:
          mixins_builder.append(starlark_conversions.convert_direct(element))

      case 'cros_board':
        skylab_mixin_builder['skylab'] = values.CallValueBuilder(
            'targets.skylab',
            {key.value: starlark_conversions.convert_direct(value)})

      case 'browser_config':
        value = typing.cast(pyl.Str, value)
        browser_config = _BROWSER_CONFIG_MAPPING[value.value]
        settings_builder[key.value] = f'targets.browser_config.{browser_config}'

      case 'os_type':
        value = typing.cast(pyl.Str, value)
        settings_builder[key.value] = (
            f'targets.os_type.{_OS_TYPE_MAPPING[value.value]}')

      case 'skip_merge_script':
        value = typing.cast(pyl.Bool, value)
        if value.value:
          settings_builder['use_android_merge_script_by_default'] = str(False)

      case 'swarming':
        value = typing.cast(pyl.Dict[pyl.Str, pyl.Value], value)
        anonymous_mixin_builder['swarming'] = (
            starlark_conversions.convert_swarming(value))

      case 'use_swarming':
        settings_builder[key.value] = starlark_conversions.convert_direct(value)

      case _:
        raise Exception(
            f'{key.start}: unhandled key in builder config: "{key.value}"')

  bundle_builder['per_test_modifications'] = _per_test_modifications(
      builder, test_suite_exceptions)

  bundle_output = bundle_builder.output()
  assert bundle_output is not None
  edits = {'targets': bundle_output}
  if (settings_output := settings_builder.output()) is not None:
    edits['targets_settings'] = ''.join(settings_output)

  return edits


class WaterfallError(Exception):
  """Raised for errors related to processing waterfall data."""
  pass


@dataclasses.dataclass
class StarlarkEdits:
  """Edits to make to a starlark file to migrate tests for builders."""

  targets_builder_defaults: dict[str, str]
  """The parameters to set in the targets.builder_defaults declaration.

  parameter name -> string representation of parameter value
  """

  targets_settings_defaults: dict[str, str]
  """The parameters to set in the targets.settings_defaults declaration.

  parameter name -> string representation of parameter value
  """

  edits_by_builder: dict[str, dict[str, str]]
  """The parameters to set for each builder to be modified.

  builder name -> parameter name
    -> string representation of parameter value
  """


def process_waterfall(
    builder_group_name: str,
    builders: set[str] | None,
    waterfalls: pyl.List[pyl.Dict[pyl.Str, pyl.Value]],
    test_suite_exceptions: pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
) -> StarlarkEdits:
  """Processes waterfall data to generate starlark migration edits.

  Args:
    builder_group_name: The name of the builder group to process.
    builders: A set of specific builders to process, or None for all.
    waterfalls: The parsed content of waterfalls.pyl.
    test_suite_exceptions: The parsed content of test_suite_exceptions.pyl.

  Returns:
    A StarlarkEdits object containing the edits.

  Raises:
    WaterfallError: If the builder group or specified builders are not found.
  """
  for waterfall in waterfalls.elements:
    if any(key.value == 'name'
           and typing.cast(pyl.Str, value).value == builder_group_name
           for key, value in waterfall.items):
      break
  else:
    raise WaterfallError(f'builder_group "{builder_group_name}" not found')

  targets_builder_defaults = {}
  targets_settings_defaults = {}
  edits_by_builder = {}

  # Make a copy of builders since it will be modified
  builders_to_process = builders.copy() if builders is not None else None

  for key, value in waterfall.items:
    match key.value:
      case 'project' | 'bucket' | 'name':
        pass

      case 'mixins':
        value = typing.cast(pyl.List[pyl.Str], value)
        mixins_default_builder = values.ListValueBuilder(
            [f'"{m.value}"' for m in value.elements])
        targets_builder_defaults['mixins'] = mixins_default_builder.output()

      case 'machines':
        value = typing.cast(pyl.Dict[pyl.Str, pyl.Dict[pyl.Str, pyl.Value]],
                            value)
        for builder_name, builder_config in value.items:
          if (builders_to_process is not None
              and builder_name.value not in builders_to_process):
            continue

          edits_by_builder[builder_name.value] = _compute_edits(
              builder_name.value,
              builder_config,
              test_suite_exceptions,
          )

          if builders_to_process is not None:
            builders_to_process.remove(builder_name.value)
            if not builders_to_process:
              break

      case 'forbid_script_tests':
        value = typing.cast(pyl.Bool, value)
        if value.value:
          targets_settings_defaults['allow_script_tests'] = 'False'

      case _:
        raise Exception(
            f'{key.start}: unhandled key in waterfall: "{key.value}"')

  if builders_to_process:
    builder_message = ', '.join(f'"{b}"' for b in sorted(builders_to_process))
    raise WaterfallError("the following builders don't exist in builder group "
                         f'"{builder_group_name}": {builder_message}')

  return StarlarkEdits(targets_builder_defaults, targets_settings_defaults,
                       edits_by_builder)


def _escape_spaces(s: str) -> str:
  return s.replace(' ', '\\ ').replace('\n', '\\\n')


def update_starlark(
    builder_group: str,
    star_file: pathlib.Path,
    edits: StarlarkEdits,
) -> None:
  """Update a starlark file with the given edits.

  Args:
    builder_group: The name of the builder group to update.
    star_file: The path to the starlark file to update.
    edits: The edits to make to the starlark file.
  """

  # Ideally all modifications would be done with a single buildozer invocation
  # using a commands file, but escaped newlines aren't supported with commands
  # files.
  #
  # Pull request to fix: https://github.com/bazelbuild/buildtools/pull/1296

  # Buildozer is geared towards manipulating build targets, actions that operate
  # on the file level, such as adding a new rule, use __pkg__ as the target name
  file_target = f'{star_file}:__pkg__'

  buildozer.run('new_load //lib/targets.star targets', file_target)

  def create_defaults(kind):
    # %{kind} as the pattern tells it to operate on all rules of that kind.
    # There shouldn't be more than one targets.builder_defaults.set "rule".
    defaults_target = f'{star_file}:%{kind}'
    # Check if a declaration of the kind already exists, any print operation
    # will result in output if there is already a rule
    if not buildozer.run('print kind', defaults_target):
      # It's not possible to add an arbitrary function call, only new rules,
      # which require a name, so create a rule with a temporary name and then
      # remove the name attribute, then we can just use the kind filter for
      # modifying it
      temp_name = 'NO_DECLARATION_SHOULD_EXIST_WITH_THIS_NAME'
      buildozer.run(f'new {kind} {temp_name} before {builder_group}',
                    file_target)
      buildozer.run('remove name', f'{star_file}:{temp_name}')
    return defaults_target

  for kind, defaults in (
      ('targets.builder_defaults.set', edits.targets_builder_defaults),
      ('targets.settings_defaults.set', edits.targets_settings_defaults),
  ):
    if not defaults:
      continue
    defaults_target = create_defaults(kind)
    for attr, value in defaults.items():
      buildozer.run(f'set {attr} {_escape_spaces(value)}', defaults_target)

  for builder, builder_edits in edits.edits_by_builder.items():
    for attr, value in builder_edits.items():
      buildozer.run(f'set {attr} {_escape_spaces(value)}',
                    f'{star_file}:{builder}')
