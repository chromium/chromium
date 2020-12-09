# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various custom data types for use throughout the unexpected pass finder."""

import fnmatch


class Expectation(object):
  """Container for a test expectation.

  Similar to typ's expectations_parser.Expectation class, but with unnecessary
  data stripped out and made hashable.

  The data contained in an Expectation is equivalent to a single line in an
  expectation file.
  """

  def __init__(self, test, tags, expected_results):
    self.test = test
    self.tags = frozenset(tags)
    if isinstance(expected_results, str):
      self.expected_results = frozenset([expected_results])
    else:
      self.expected_results = frozenset(expected_results)

    # We're going to be making a lot of comparisons, and fnmatch is *much*
    # slower (~40x from rough testing) than a straight comparison, so only use
    # it if necessary.
    if '*' in test:
      self._comp = lambda r: fnmatch.fnmatch(r, self.test)
    else:
      self._comp = lambda r: r == self.test

  def __eq__(self, other):
    return (isinstance(other, Expectation) and self.test == other.test
            and self.tags == other.tags
            and self.expected_results == other.expected_results)

  def __hash__(self):
    return hash((self.test, self.tags, self.expected_results))

  def AppliesToResult(self, result):
    """Checks whether this expectation should have applied to |result|.

    An expectation applies to a result if the test names match (including
    wildcard expansion) and the expectation's tags are a subset of the result's
    tags.

    Args:
      result: A Result instance to check against.

    Returns:
      True if |self| applies to |result|, otherwise False.
    """
    assert isinstance(result, Result)
    return (self._comp(result.test) and self.tags <= result.tags)


class Result(object):
  """Container for a test result.

  Contains the minimal amount of data necessary to describe/identify a result
  from ResultDB for the purposes of the unexpected pass finder.
  """

  def __init__(self, test, tags, actual_result, step, build_id):
    """
    Args:
      test: A string containing the name of the test. Cannot have wildcards.
      tags: An iterable containing the typ tags for the result.
      actual_result: The actual result of the test as a string.
      step: A string containing the name of the step on the builder.
      build_id: A string containing the Buildbucket ID for the build this result
          came from.
    """
    # Results should not have any globs.
    assert '*' not in test
    self.test = test
    self.tags = frozenset(tags)
    self.actual_result = actual_result
    self.step = step
    self.build_id = build_id

  def __eq__(self, other):
    return (isinstance(other, Result) and self.test == other.test
            and self.tags == other.tags
            and self.actual_result == other.actual_result
            and self.step == other.step and self.build_id == other.build_id)

  def __hash__(self):
    return hash(
        (self.test, self.tags, self.actual_result, self.step, self.build_id))


class BuildStats(object):
  """Container for keeping track of a builder's pass/fail stats."""

  def __init__(self):
    self.passed_builds = 0
    self.total_builds = 0
    self.failure_links = frozenset()

  @property
  def failed_builds(self):
    return self.total_builds - self.passed_builds

  def AddPassedBuild(self):
    self.passed_builds += 1
    self.total_builds += 1

  def AddFailedBuild(self, build_id):
    self.total_builds += 1
    build_link = BuildLinkFromBuildId(build_id)
    self.failure_links = frozenset([build_link]) | self.failure_links

  def __eq__(self, other):
    return (isinstance(other, BuildStats)
            and self.passed_builds == other.passed_builds
            and self.total_builds == other.total_builds
            and self.failure_links == other.failure_links)


def BuildLinkFromBuildId(build_id):
  return 'http://ci.chromium.org/b/%s' % build_id
