# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for converting python values to starlark values."""

import typing

from . import comments
from . import pyl
from . import values

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


def convert_arg(arg: pyl.Str) -> values.MaybeCommentedValue[str]:
  """Convert a test argument to a starlark value.

  In //testing/buildbot, there are magic strings that trigger special
  replacement behavior. In starlark, these are replaced with struct
  values. This function takes care of converting the argument to a
  string or a magic struct as appropriate.
  """
  arg_value = typing.cast(str, arg.value)
  if arg_value.startswith('$$MAGIC_SUBSTITUTION_'):
    return comments.comment(
        arg, f'targets.magic_args.{_MAGIC_ARG_MAPPING[arg_value]}')
  return convert_direct(arg)


def convert_args(args: pyl.List[pyl.Str]) -> values.Value:
  """Convert a list of test arguments to a starlark value.

  In //testing/buildbot, there are magic strings that trigger special
  replacement behavior. In starlark, these are replaced with struct
  values. This function takes care of converting the individual
  arguments to a string or a magic struct as appropriate.
  """
  return values.ListValueBuilder([convert_arg(arg) for arg in args.elements])


@typing.overload
def convert_direct(
    value: pyl.Constant,
    *,
    include_comments: typing.Literal[False],
) -> str:
  ...  # pragma: no cover


@typing.overload
def convert_direct(
    value: pyl.Constant,
    *,
    include_comments: bool = True,
) -> values.MaybeCommentedValue[str]:
  ...  # pragma: no cover


@typing.overload
def convert_direct(
    value: pyl.Dict | pyl.List,
    *,
    include_comments: typing.Literal[False],
) -> values.ValueBuilder:
  ...  # pragma: no cover


@typing.overload
def convert_direct(
    value: pyl.Dict | pyl.List,
    *,
    include_comments: bool = True,
) -> values.MaybeCommentedValue[values.ValueBuilder]:
  ...  # pragma: no cover


def convert_direct(value, *, include_comments=True):
  """Convert a pyl value to a starlark value.

  This converts pyl values where the starlark representation is the
  same.
  """
  T = typing.TypeVar('T', bound=(str | values.ValueBuilder))

  def maybe_comment(converted: T) -> values.MaybeCommentedValue[T]:
    if include_comments and value.comments:
      return comments.comment(value, converted)
    return converted

  match value:
    case pyl.Int() | pyl.Bool() | pyl.None_():
      return maybe_comment(repr(value.value))
    case pyl.Str():
      return maybe_comment('"{}"'.format(value.value.replace("\"", "\\\"")))
    case pyl.List():
      return maybe_comment(
          typing.cast(
              values.ValueBuilder,
              values.ListValueBuilder(
                  [convert_direct(e) for e in value.elements])))
    case pyl.Dict():  # pragma: no branch
      return maybe_comment(converted=typing.cast(
          values.ValueBuilder,
          values.DictValueBuilder({
              typing.cast(str, convert_direct(k)):
              convert_direct(v)
              for k, v in value.items
          })))
  typing.assert_never(value)  # pragma: no cover


def param_name(
    key: pyl.Str,
    *,
    remap_to: str | None = None,
) -> values.MaybeCommentedValue[str]:
  """Convert a pyl dict key to a starlark parameter name.

  Arg:
    key: The key to convert.
    remap_to: If non-empty, will be used as the parameter name instead
      of key, but any comments attached to key will be attached to the
      returned value.
  """
  name = remap_to or key.value
  return comments.comment(key, name)


def convert_resultdb(resultdb: pyl.Dict[pyl.Str, pyl.Value]) -> values.Value:
  """Convert a resultdb dict to a targets.resultdb call."""
  value_builder = values.CallValueBuilder('targets.resultdb')

  for key, value in resultdb.items:
    match key.value:
      case 'enable':
        pass

      case _:
        raise Exception(
            f'{key.start}: unhandled key in resultdb: "{key.value}"')

    converted_value = convert_direct(value)
    assert not isinstance(converted_value, values.CommentedValue), (
        f'{value.start}: unexpected comment on value for key "{key.value}"')
    converted_param = param_name(key)
    value_builder[converted_param] = converted_value

  return value_builder


def convert_swarming(swarming: pyl.Dict[pyl.Str, pyl.Value]) -> values.Value:
  """Convert a swarming dict to a targets.swarming call."""
  value_builder = values.CallValueBuilder('targets.swarming')

  for key, value in swarming.items:
    match key.value:
      case 'dimensions' | 'idempotent' | 'service_account' | 'shards':
        converted_param = param_name(key)

      case 'expiration':
        converted_param = param_name(key, remap_to='expiration_sec')

      case 'hard_timeout':
        converted_param = param_name(key, remap_to='hard_timeout_sec')

      case 'io_timeout':
        converted_param = param_name(key, remap_to='io_timeout_sec')

      case _:
        raise Exception(
            f'{key.start}: unhandled key in swarming: "{key.value}"')

    converted_value = convert_direct(value)
    assert not isinstance(converted_value, values.CommentedValue), (
        f'{value.start}: unexpected comment on value for key "{key.value}"')
    value_builder[converted_param] = converted_value

  return value_builder


def convert_skylab(skylab: pyl.Dict[pyl.Str, pyl.Value]) -> values.Value:
  """Convert a skylab dict to a targets.skylab call."""
  value_builder = values.CallValueBuilder('targets.skylab')

  for key, value in skylab.items:
    match key.value:
      case 'shards' | 'timeout_sec':
        pass

      case _:
        raise Exception(f'{key.start}: unhandled key in skylab: "{key.value}"')

    converted_value = convert_direct(value)
    assert not isinstance(converted_value, values.CommentedValue), (
        f'{value.start}: unexpected comment on value for key "{key.value}"')
    converted_param = param_name(key)
    value_builder[converted_param] = converted_value

  return value_builder
