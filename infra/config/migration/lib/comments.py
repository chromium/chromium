# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for adding comments from pyl values to converted values."""

import typing

from . import pyl
from . import values

T = typing.TypeVar('T', bound=(str | values.ValueBuilder))


def comment(value: pyl.Value, converted: T) -> values.MaybeCommentedValue[T]:
  if value.comments:
    return values.CommentedValue(
        converted, [comment.comment for comment in value.comments])
  return converted
