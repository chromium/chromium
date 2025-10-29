# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing

import starlark_conversions
import values


def convert_basic_suite(
    suite: dict[str, dict[str, typing.Any]]) -> dict[str, str | None]:
  """Convert a basic suite definition to a bundle.

  Args:
    suite: The basic suite pyl definition. The keys are the names of
      tests in the suite with the corresponding values being the
      definition of the test.

  Returns:
    A dict containing the parameters to define a targets.bundle with.
    The keys are the parameter names with the corresponding values being
    the string representation of the parameter values.
  """
  targets_builder = values.ListValueBuilder()
  per_test_modifications_builder = values.DictValueBuilder()
  for test_name, test in suite.items():
    test_name = starlark_conversions.convert_direct(test_name)
    targets_builder.append(test_name)

    anonymous_mixin_builder = values.CallValueBuilder('targets.mixin')
    mixins_builder = None
    modifications_builder = values.CallValueBuilder(
        'targets.per_test_modification',
        elide_param='mixins',
    )

    per_test_modifications_builder[test_name] = modifications_builder

    for key, value in test.items():
      match key:
      # These keys are actually filled in by the declaration of the test in
      # starlark, so can't/shouldn't be part of the bundle definition
        case ('results_handler' | 'script' | 'telemetry_test_name' | 'test'
              | 'test_common'):
          pass

        case 'ci_only' | 'experiment_percentage' | 'use_isolated_scripts_api':
          anonymous_mixin_builder[key] = (
              starlark_conversions.convert_direct(value))

        case ('android_args' | 'chromeos_args' | 'desktop_args' | 'args'
              | 'lacros_args' | 'linux_args'):
          anonymous_mixin_builder[key] = (
              starlark_conversions.convert_args(value))

        case 'resultdb':
          anonymous_mixin_builder['resultdb'] = (
              starlark_conversions.convert_resultdb(value))

        case 'android_swarming' | 'chromeos_swarming' | 'swarming':
          anonymous_mixin_builder[key] = (
              starlark_conversions.convert_swarming(value))

        case 'skylab':
          anonymous_mixin_builder[key] = (
              starlark_conversions.convert_skylab(value))

        case 'mixins':
          mixins_builder = values.ListValueBuilder([anonymous_mixin_builder])
          for e in value:
            mixins_builder.append(starlark_conversions.convert_direct(e))

        case 'remove_mixins':
          # Remove_mixins in the starlark basic suite declaration won't be part
          # of the generated test_suites.pyl if a pyl entry isn't generated for
          # the mixin, so ensure the remove_mixins gets checked to include all
          # of the elements
          remove_mixins_builder = values.ListValueBuilder([
              f'"DO{""} NOT SUBMIT ensure all remove mixins values are present"'
          ])
          modifications_builder['remove_mixins'] = remove_mixins_builder
          for m in value:
            remove_mixins_builder.append(starlark_conversions.convert_direct(m))

        case _:
          raise Exception(
              f'unhandled key in basic suite test definition: "{key}"')

    modifications_builder['mixins'] = mixins_builder or anonymous_mixin_builder

  return {
      'targets': targets_builder.output(),
      'per_test_modifications': per_test_modifications_builder.output(),
  }


def convert_compound_suite(suite: list[str]) -> dict[str, str | None]:
  """Convert a compound suite definition to a bundle.

  Args:
    suite: The compound suite pyl definition. The elements are the names
      of basic suites in the compound suite.

  Returns:
    A dict containing the parameters to define a targets.bundle with.
    The keys are the parameter names with the corresponding values being
    the string representation of the parameter values.
  """
  return {
      'targets': values.to_output(starlark_conversions.convert_direct(suite))
  }


def convert_matrix_compound_suite(
    suite: dict[str, dict[str, typing.Any]]) -> dict[str, str | None]:
  """Convert a matrix compound suite definition to a bundle.

  Args:
    suite: The matrix compound suite pyl definition. The keys are the
      names of basic suites in the matrix compound suite with the
      corresponding value being the matrix config applied to the basic
      suite.

  Returns:
    A dict containing the parameters to define a targets.bundle with.
    The keys are the parameter names with the corresponding values being
    the string representation of the parameter values.
  """
  targets_builder = values.ListValueBuilder()
  for suite_name, matrix_config in suite.items():
    if not matrix_config:
      targets_builder.append(starlark_conversions.convert_direct(suite_name))
    else:
      bundle_builder = values.CallValueBuilder(
          'targets.bundle',
          {'targets': starlark_conversions.convert_direct(suite_name)})
      for key, value in matrix_config.items():
        match key:
          case 'mixins' | 'variants':
            bundle_builder[key] = starlark_conversions.convert_direct(value)

          case _:
            raise Exception(f'unhandled key in matrix config: "{key}"')
      targets_builder.append(bundle_builder)

  return {'targets': targets_builder.output()}
