# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for tag-related helper functions."""

from gpu_tests import gpu_integration_test

IGNORED_TAGS_TO_TEMPORARILY_KEEP = set([
    'webgl-version-1',
    'webgl-version-2',
])


def RemoveMostIgnoredTags(tags):
  """Removes ignored tags from |tags| except temporarily kept ones.

  The temporarily kept ones can later be removed by
  RemoveTemporarilyKeptIgnoredTags().

  Some tags are kept around temporarily because they contain useful information
  for other parts of the script, but are not present in expectation files.

  Args:
    tags: An iterable of strings containing tags

  Returns:
    A list of strings containing the contents of |tags| with ignored tags
    removed except for the ones in IGNORED_TAGS_TO_TEMPORARILY_KEEP.
  """
  ignored_tags = set(gpu_integration_test.GpuIntegrationTest.IgnoredTags())
  tags = set(tags)
  ignored_tags_to_keep = tags & IGNORED_TAGS_TO_TEMPORARILY_KEEP
  tags -= ignored_tags
  tags |= ignored_tags_to_keep
  tags = list(tags)
  tags.sort()
  return tags


def RemoveTemporarilyKeptIgnoredTags(tags):
  """Removes ignored tags that were temporarily kept.

  Args:
    tags: An iterable of strings containing tags that at one point were passed
        through RemoveMostIgnoredTags()

  Returns:
    A list of strings containing the contents of |tags| with the contents of
    IGNORED_TAGS_TO_TEMPORARILY_KEEP removed. Thus, the return value should not
    have any remaining ignored tags.
  """
  tags = list(set(tags) - IGNORED_TAGS_TO_TEMPORARILY_KEEP)
  tags.sort()
  return tags
