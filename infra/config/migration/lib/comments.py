# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for adding comments from pyl values to converted values."""

import typing

from . import pyl
from . import values

T = typing.TypeVar('T', bound=values.Value)


def comment(value: pyl.Value, converted: T) -> values.MaybeCommentedValue[T]:
  if value.comments:
    return values.CommentedValue(
        converted, [comment.comment for comment in value.comments])
  return converted


@typing.overload
def ensure_no_comments(
    value: pyl.Value,
    converted: values.CommentedValue,
    *,
    message: str | None = None,
) -> typing.Never:
  ...


@typing.overload
def ensure_no_comments(
    value: pyl.Value,
    converted: T,
    *,
    message: str | None = None,
) -> T:
  ...


def ensure_no_comments(value, converted, *, message=None):
  assert not value.comments, (f'{value.comments[0].start} unexpected comment'
                              f'{" " + message if message else ""}')
  assert not isinstance(converted, values.CommentedValue)
  return converted
