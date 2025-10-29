# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for converting python values to starlark values."""

import typing

import values

_MAGIC_ARG_MAPPING = {
    '$$MAGIC_SUBSTITUTION_AndroidDesktopTelemetryRemote':
    'ANDROID_DESKTOP_TELEMETRY_REMOTE',
    '$$MAGIC_SUBSTITUTION_ChromeOSTelemetryRemote': 'CROS_TELEMETRY_REMOTE',
    '$$MAGIC_SUBSTITUTION_ChromeOSGtestFilterFile': 'CROS_GTEST_FILTER_FILE',
    '$$MAGIC_SUBSTITUTION_GPUExpectedVendorId': 'GPU_EXPECTED_VENDOR_ID',
    '$$MAGIC_SUBSTITUTION_GPUExpectedDeviceId': 'GPU_EXPECTED_DEVICE_ID',
    '$$MAGIC_SUBSTITUTION_GPUParallelJobs': 'GPU_PARALLEL_JOBS',
    '$$MAGIC_SUBSTITUTION_GPUTelemetryNoRootForUnrootedDevices':
    'GPU_TELEMETRY_NO_ROOT_FOR_UNROOTED_DEVICES',
    '$$MAGIC_SUBSTITUTION_GPUWebGLRuntimeFile': 'GPU_WEBGL_RUNTIME_FILE',
}


def convert_arg(arg: str) -> str:
  """Convert a test argument to a starlark value.

  In //testing/buildbot, there are magic strings that trigger special
  replacement behavior. In starlark, these are replaced with struct
  values. This function takes care of converting the argument to a
  string or a magic struct as appropriate.
  """
  if arg.startswith('$$MAGIC_SUBSTITUTION_'):
    return f'targets.magic_args.{_MAGIC_ARG_MAPPING[arg]}'
  return convert_direct(arg)


def convert_args(args: list[str]) -> values.Value:
  """Convert a list of test arguments to a starlark value.

  In //testing/buildbot, there are magic strings that trigger special
  replacement behavior. In starlark, these are replaced with struct
  values. This function takes care of converting the individual
  arguments to a string or a magic struct as appropriate.
  """
  return values.ListValueBuilder([convert_arg(arg) for arg in args])


@typing.overload
def convert_direct(value: bool | int | str | None) -> str:
  ...  # pragma: no cover


@typing.overload
def convert_direct(value: list | dict) -> values.ValueBuilder:
  ...  # pragma: no cover


def convert_direct(value):
  """Convert a python value to a starlark value.

  This converts python values where the starlark representation is the
  same.
  """
  if value is None or isinstance(value, (int, bool)):
    return str(value)
  if isinstance(value, str):
    return '"{}"'.format(value.replace("\"", "\\\""))
  if isinstance(value, list):
    return typing.cast(
        values.ValueBuilder,
        values.ListValueBuilder([convert_direct(e) for e in value]))
  if isinstance(value, dict):
    return typing.cast(
        values.ValueBuilder,
        values.DictValueBuilder({
            typing.cast(str, convert_direct(k)):
            convert_direct(v)
            for k, v in value.items()
        }))
  raise Exception(f'unhandled python value: {value!r}')


def convert_resultdb(resultdb: dict[str, typing.Any]) -> values.Value:
  """Convert a resultdb dict to a targets.resultdb call."""
  value_builder = values.CallValueBuilder('targets.resultdb')

  for key, value in resultdb.items():
    match key:
      case 'enable':
        value_builder[key] = convert_direct(value)

      case _:
        raise Exception(f'unhandled key in resultdb: "{key}"')

  return value_builder


def convert_swarming(swarming: dict[str, typing.Any]) -> values.Value:
  """Convert a swarming dict to a targets.swarming call."""
  value_builder = values.CallValueBuilder('targets.swarming')

  for key, value in swarming.items():
    match key:
      case 'dimensions' | 'idempotent' | 'service_account' | 'shards':
        value_builder[key] = convert_direct(value)

      case 'expiration':
        value_builder['expiration_sec'] = convert_direct(value)

      case 'hard_timeout':
        value_builder['hard_timeout_sec'] = convert_direct(value)

      case 'io_timeout':
        value_builder['io_timeout_sec'] = convert_direct(value)

      case _:
        raise Exception(f'unhandled key in swarming: "{key}"')

  return value_builder


def convert_skylab(skylab: dict[str, typing.Any]) -> values.Value:
  """Convert a skylab dict to a targets.skylab call."""
  value_builder = values.CallValueBuilder('targets.skylab')

  for key, value in skylab.items():
    match key:
      case 'shards' | 'timeout_sec':
        value_builder[key] = convert_direct(value)

      case _:
        raise Exception(f'unhandled key in skylab: "{key}"')

  return value_builder
