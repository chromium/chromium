# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for tag-related helper functions."""

from typing import Iterable, Type

from flake_suppressor_common import common_typing as ct

TagUtils = None


# TODO(crbug.com/358591565): Refactor this to remove the need for global
# statements.
def SetTagUtilsImplementation(impl: Type['BaseTagUtils']) -> None:
  global TagUtils  # pylint: disable=global-statement
  assert issubclass(impl, BaseTagUtils)
  TagUtils = impl()


class BaseTagUtils():
  def RemoveIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
    """Removes ignored tags from |tags|.

    Here we return all the tags as is, since child classes will do the
    implementation specific filtering.

    Args:
      tags: An iterable of strings containing tags

    Returns:
      A tuple of strings containing the contents of |tags| with ignored tags
      removed.
    """
    return tuple(tags)


TagUtils = BaseTagUtils()
