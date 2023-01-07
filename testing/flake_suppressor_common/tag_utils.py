# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for tag-related helper functions."""

from typing import Iterable, Type

from flake_suppressor_common import common_typing as ct

TagUtils = None


def SetTagUtilsImplementation(impl: Type['BaseTagUtils']) -> None:
  global TagUtils
  assert issubclass(impl, BaseTagUtils)
  TagUtils = impl()


class BaseTagUtils():
  def RemoveMostIgnoredTags(self, tags: Iterable[str]) -> ct.TagTupleType:
    """Removes ignored tags from |tags| except temporarily kept ones.

    The temporarily kept ones can later be removed by
    RemoveTemporarilyKeptIgnoredTags().

    Some tags are kept around temporarily because they contain useful
    information for other parts of the script, but are not present in
    expectation files.

    Here we return all the tags as is, since child classes will do the
    implementation specific filtering.

    Args:
      tags: An iterable of strings containing tags

    Returns:
      A tuple of strings containing the contents of |tags| with ignored tags
      removed except for the ones in IGNORED_TAGS_TO_TEMPORARILY_KEEP.
    """
    return tuple(tags)

  def RemoveTemporarilyKeptIgnoredTags(self,
                                       tags: Iterable[str]) -> ct.TagTupleType:
    """Removes ignored tags that were temporarily kept.

    Here we return all the tags as is, since child classes will do the
    implementation specific filtering.

    Args:
      tags: An iterable of strings containing tags that at one point were passed
          through RemoveMostIgnoredTags()

    Returns:
      A tuple of strings containing the contents of |tags| with the contents of
      IGNORED_TAGS_TO_TEMPORARILY_KEEP removed. Thus, the return value should
      not have any remaining ignored tags.
    """
    return tuple(tags)


TagUtils = BaseTagUtils()
