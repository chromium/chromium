# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various custom data types for use throughout the unexpected pass finder."""

from __future__ import print_function

import collections
import copy
import fnmatch
import logging

import six

FULL_PASS = 1
NEVER_PASS = 2
PARTIAL_PASS = 3

# Allow different unexpected pass finder implementations to register custom
# data types if necessary. These are set to the base versions at the end of the
# file.
Expectation = None
Result = None
BuildStats = None
TestExpectationMap = None

# pylint: disable=super-with-arguments,useless-object-inheritance


def SetExpectationImplementation(impl):
  global Expectation
  assert issubclass(impl, BaseExpectation)
  Expectation = impl


def SetResultImplementation(impl):
  global Result
  assert issubclass(impl, BaseResult)
  Result = impl


def SetBuildStatsImplementation(impl):
  global BuildStats
  assert issubclass(impl, BaseBuildStats)
  BuildStats = impl


def SetTestExpectationMapImplementation(impl):
  global TestExpectationMap
  assert issubclass(impl, BaseTestExpectationMap)
  TestExpectationMap = impl


class BaseExpectation(object):
  """Container for a test expectation.

  Similar to typ's expectations_parser.Expectation class, but with unnecessary
  data stripped out and made hashable.

  The data contained in an Expectation is equivalent to a single line in an
  expectation file.
  """

  def __init__(self, test, tags, expected_results, bug=None):
    self.test = test
    self.tags = frozenset(tags)
    self.bug = bug or ''
    if isinstance(expected_results, str):
      self.expected_results = frozenset([expected_results])
    else:
      self.expected_results = frozenset(expected_results)

    # We're going to be making a lot of comparisons, and fnmatch is *much*
    # slower (~40x from rough testing) than a straight comparison, so only use
    # it if necessary.
    if '*' in test:
      self._comp = self._CompareWildcard
    else:
      self._comp = self._CompareNonWildcard

  def __eq__(self, other):
    return (isinstance(other, BaseExpectation) and self.test == other.test
            and self.tags == other.tags
            and self.expected_results == other.expected_results
            and self.bug == other.bug)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash((self.test, self.tags, self.expected_results, self.bug))

  def _CompareWildcard(self, result_test_name):
    return fnmatch.fnmatch(result_test_name, self.test)

  def _CompareNonWildcard(self, result_test_name):
    return result_test_name == self.test

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
    assert isinstance(result, BaseResult)
    return (self._comp(result.test) and self.tags <= result.tags)

  def MaybeAppliesToTest(self, test_name):
    """Similar to AppliesToResult, but used to do initial filtering.

    Args:
      test_name: A string containing the name of a test.

    Returns:
      True if |self| could apply to a test named |test_name|, otherwise False.
    """
    return self._comp(test_name)


class BaseResult(object):
  """Container for a test result.

  Contains the minimal amount of data necessary to describe/identify a result
  from ResultDB for the purposes of the unexpected pass finder.
  """

  def __init__(self, test, tags, actual_result, step, build_id):
    """
    Args:
      test: A string containing the name of the test.
      tags: An iterable containing the typ tags for the result.
      actual_result: The actual result of the test as a string.
      step: A string containing the name of the step on the builder.
      build_id: A string containing the Buildbucket ID for the build this result
          came from.
    """
    self.test = test
    self.tags = frozenset(tags)
    self.actual_result = actual_result
    self.step = step
    self.build_id = build_id

  def __eq__(self, other):
    return (isinstance(other, BaseResult) and self.test == other.test
            and self.tags == other.tags
            and self.actual_result == other.actual_result
            and self.step == other.step and self.build_id == other.build_id)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash(
        (self.test, self.tags, self.actual_result, self.step, self.build_id))


class BaseBuildStats(object):
  """Container for keeping track of a builder's pass/fail stats."""

  def __init__(self):
    self.passed_builds = 0
    self.total_builds = 0
    self.failure_links = frozenset()

  @property
  def failed_builds(self):
    return self.total_builds - self.passed_builds

  @property
  def did_fully_pass(self):
    return self.passed_builds == self.total_builds

  @property
  def did_never_pass(self):
    return self.failed_builds == self.total_builds

  def AddPassedBuild(self):
    self.passed_builds += 1
    self.total_builds += 1

  def AddFailedBuild(self, build_id):
    self.total_builds += 1
    build_link = BuildLinkFromBuildId(build_id)
    self.failure_links = frozenset([build_link]) | self.failure_links

  def GetStatsAsString(self):
    return '(%d/%d passed)' % (self.passed_builds, self.total_builds)

  # pylint:disable=unused-argument
  def NeverNeededExpectation(self, expectation):
    """Returns whether the results tallied in |self| never needed |expectation|.

    Args:
      expectation: An Expectation object that |stats| is located under.

    Returns:
      True if all the results tallied in |self| would have passed without
      |expectation| being present. Otherwise, False.
    """
    return self.did_fully_pass
  # pylint:enable=unused-argument

  # pylint:disable=unused-argument
  def AlwaysNeededExpectation(self, expectation):
    """Returns whether the results tallied in |self| always needed |expectation.

    Args:
      expectation: An Expectation object that |stats| is located under.

    Returns:
      True if all the results tallied in |self| would have failed without
      |expectation| being present. Otherwise, False.
    """
    return self.did_never_pass
  # pylint:enable=unused-argument

  def __eq__(self, other):
    return (isinstance(other, BuildStats)
            and self.passed_builds == other.passed_builds
            and self.total_builds == other.total_builds
            and self.failure_links == other.failure_links)

  def __ne__(self, other):
    return not self.__eq__(other)


def BuildLinkFromBuildId(build_id):
  return 'http://ci.chromium.org/b/%s' % build_id


# These explicit overrides could likely be replaced by using regular dicts with
# type hinting in Python 3. Based on https://stackoverflow.com/a/2588648, this
# should cover all cases where the dict can be modified.
class BaseTypedMap(dict):
  """A base class for typed dictionaries.

  Any child classes that override __setitem__ will have any modifications to the
  dictionary go through the type checking in __setitem__.
  """

  def __init__(self, *args, **kwargs):  # pylint:disable=super-init-not-called
    self.update(*args, **kwargs)

  def update(self, *args, **kwargs):
    if args:
      assert len(args) == 1
      other = dict(args[0])
      for k, v in other.items():
        self[k] = v
    for k, v in kwargs.items():
      self[k] = v

  def setdefault(self, key, value=None):
    if key not in self:
      self[key] = value
    return self[key]

  def _value_type(self):
    raise NotImplementedError()

  def IterToValueType(self, value_type):
    """Recursively iterates over contents until |value_type| is found.

    Used to get rid of nested loops, instead using a single loop that
    automatically iterates through all the contents at a certain depth.

    Args:
      value_type: The type to recurse to and then iterate over. For example,
          "BuilderStepMap" would result in iterating over the BuilderStepMap
          values, meaning that the returned generator would create tuples in the
          form (test_name, expectation, builder_map).

    Returns:
      A generator that yields tuples. The length and content of the tuples will
      vary depending on |value_type|. For example, using "BuilderStepMap" would
      result in tuples of the form (test_name, expectation, builder_map), while
      "BuildStats" would result in (test_name, expectation, builder_name,
      step_name, build_stats).
    """
    if self._value_type() == value_type:
      for k, v in self.items():
        yield k, v
    else:
      for k, v in self.items():
        for nested_value in v.IterToValueType(value_type):
          yield (k, ) + nested_value

  def Merge(self, other_map, reference_map=None):
    """Merges |other_map| into self.

    Args:
      other_map: A BaseTypedMap whose contents will be merged into self.
      reference_map: A dict containing the information that was originally in
          self. Used for ensuring that a single expectation/builder/step
          combination is only ever updated once. If None, a copy of self will be
          used.
    """
    assert isinstance(other_map, self.__class__)
    # We should only ever encounter a single updated BuildStats for an
    # expectation/builder/step combination. Use the reference map to determine
    # if a particular BuildStats has already been updated or not.
    reference_map = reference_map or copy.deepcopy(self)
    for key, value in other_map.items():
      if key not in self:
        self[key] = value
      else:
        if isinstance(value, dict):
          self[key].Merge(value, reference_map.get(key, {}))
        else:
          assert isinstance(value, BuildStats)
          # Ensure we haven't updated this BuildStats already. If the reference
          # map doesn't have a corresponding BuildStats, then base_map shouldn't
          # have initially either, and thus it would have been added before
          # reaching this point. Otherwise, the two values must match, meaning
          # that base_map's BuildStats hasn't been updated yet.
          reference_stats = reference_map.get(key, None)
          assert reference_stats is not None
          assert reference_stats == self[key]
          self[key] = value


class BaseTestExpectationMap(BaseTypedMap):
  """Typed map for string types -> ExpectationBuilderMap.

  This results in a dict in the following format:
  {
    expectation_file1 (str): {
      expectation1 (data_types.Expectation): {
        builder_name1 (str): {
          step_name1 (str): stats1 (data_types.BuildStats),
          step_name2 (str): stats2 (data_types.BuildStats),
          ...
        },
        builder_name2 (str): { ... },
      },
      expectation2 (data_types.Expectation): { ... },
      ...
    },
    expectation_file2 (str): { ... },
    ...
  }
  """

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, ExpectationBuilderMap)
    super(BaseTestExpectationMap, self).__setitem__(key, value)

  def _value_type(self):
    return ExpectationBuilderMap

  def IterBuilderStepMaps(self):
    """Iterates over all BuilderStepMaps contained in the map.

    Returns:
      A generator yielding tuples in the form (expectation_file (str),
      expectation (Expectation), builder_map (BuilderStepMap))
    """
    return self.IterToValueType(BuilderStepMap)

  def AddResultList(self, builder, results, expectation_files=None):
    """Adds |results| to |self|.

    Args:
      builder: A string containing the builder |results| came from. Should be
          prefixed with something to distinguish between identically named CI
          and try builders.
      results: A list of data_types.Result objects corresponding to the ResultDB
          data queried for |builder|.
      expectation_files: An iterable of expectation file names that these
          results could possibly apply to. If None, then expectations from all
          known expectation files will be used.

    Returns:
      A list of data_types.Result objects who did not have a matching
      expectation in |self|.
    """
    failure_results = set()
    pass_results = set()
    unmatched_results = []
    for r in results:
      if r.actual_result == 'Pass':
        pass_results.add(r)
      else:
        failure_results.add(r)

    # Remove any cases of failure -> pass from the passing set. If a test is
    # flaky, we get both pass and failure results for it, so we need to remove
    # the any cases of a pass result having a corresponding, earlier failure
    # result.
    modified_failing_retry_results = set()
    for r in failure_results:
      modified_failing_retry_results.add(
          Result(r.test, r.tags, 'Pass', r.step, r.build_id))
    pass_results -= modified_failing_retry_results

    # Group identically named results together so we reduce the number of
    # comparisons we have to make.
    all_results = pass_results | failure_results
    grouped_results = collections.defaultdict(list)
    for r in all_results:
      grouped_results[r.test].append(r)

    matched_results = self._AddGroupedResults(grouped_results, builder,
                                              expectation_files)
    unmatched_results = list(all_results - matched_results)

    return unmatched_results

  def _AddGroupedResults(self, grouped_results, builder, expectation_files):
    """Adds all results in |grouped_results| to |self|.

    Args:
      grouped_results: A dict mapping test name (str) to a list of
          data_types.Result objects for that test.
      builder: A string containing the name of the builder |grouped_results|
          came from.
      expectation_files: An iterable of expectation file names that these
          results could possibly apply to. If None, then expectations from all
          known expectation files will be used.

    Returns:
      A set of data_types.Result objects that had at least one matching
      expectation.
    """
    matched_results = set()
    for test_name, result_list in grouped_results.items():
      for ef, expectation_map in self.items():
        if expectation_files is not None and ef not in expectation_files:
          continue
        for expectation, builder_map in expectation_map.items():
          if not expectation.MaybeAppliesToTest(test_name):
            continue
          for r in result_list:
            if expectation.AppliesToResult(r):
              matched_results.add(r)
              step_map = builder_map.setdefault(builder, StepBuildStatsMap())
              stats = step_map.setdefault(r.step, BuildStats())
              self._AddSingleResult(r, stats)
    return matched_results

  def _AddSingleResult(self, result, stats):
    """Adds |result| to |self|.

    Args:
      result: A data_types.Result object to add.
      stats: A data_types.BuildStats object to add the result to.
    """
    if result.actual_result == 'Pass':
      stats.AddPassedBuild()
    else:
      stats.AddFailedBuild(result.build_id)

  def SplitByStaleness(self):
    """Separates stored data based on expectation staleness.

    Returns:
      Three TestExpectationMaps (stale_dict, semi_stale_dict, active_dict). All
      three combined contain the information of |self|. |stale_dict| contains
      entries for expectations that are no longer being helpful,
      |semi_stale_dict| contains entries for expectations that might be
      removable or modifiable, but have at least one failed test run.
      |active_dict| contains entries for expectations that are preventing
      failures on all builders they're active on, and thus shouldn't be removed.
    """
    stale_dict = TestExpectationMap()
    semi_stale_dict = TestExpectationMap()
    active_dict = TestExpectationMap()

    # This initially looks like a good target for using
    # TestExpectationMap's iterators since there are many nested loops.
    # However, we need to reset state in different loops, and the alternative of
    # keeping all the state outside the loop and resetting under certain
    # conditions ends up being less readable than just using nested loops.
    for expectation_file, expectation_map in self.items():
      for expectation, builder_map in expectation_map.items():
        # A temporary map to hold data so we can later determine whether an
        # expectation is stale, semi-stale, or active.
        tmp_map = {
            FULL_PASS: BuilderStepMap(),
            NEVER_PASS: BuilderStepMap(),
            PARTIAL_PASS: BuilderStepMap(),
        }

        split_stats_map = builder_map.SplitBuildStatsByPass(expectation)
        for builder_name, (fully_passed, never_passed,
                           partially_passed) in split_stats_map.items():
          if fully_passed:
            tmp_map[FULL_PASS][builder_name] = fully_passed
          if never_passed:
            tmp_map[NEVER_PASS][builder_name] = never_passed
          if partially_passed:
            tmp_map[PARTIAL_PASS][builder_name] = partially_passed

        def _CopyPassesIntoBuilderMap(builder_map, pass_types):
          for pt in pass_types:
            for builder, steps in tmp_map[pt].items():
              builder_map.setdefault(builder, StepBuildStatsMap()).update(steps)

        # Handle the case of a stale expectation.
        if not (tmp_map[NEVER_PASS] or tmp_map[PARTIAL_PASS]):
          builder_map = stale_dict.setdefault(
              expectation_file,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map, [FULL_PASS])
        # Handle the case of an active expectation.
        elif not tmp_map[FULL_PASS]:
          builder_map = active_dict.setdefault(
              expectation_file,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map, [NEVER_PASS, PARTIAL_PASS])
        # Handle the case of a semi-stale expectation.
        else:
          # TODO(crbug.com/998329): Sort by pass percentage so it's easier to
          # find problematic builders without highlighting.
          builder_map = semi_stale_dict.setdefault(
              expectation_file,
              ExpectationBuilderMap()).setdefault(expectation, BuilderStepMap())
          _CopyPassesIntoBuilderMap(builder_map,
                                    [FULL_PASS, PARTIAL_PASS, NEVER_PASS])
    return stale_dict, semi_stale_dict, active_dict

  def FilterOutUnusedExpectations(self):
    """Filters out any unused Expectations from stored data.

    An Expectation is considered unused if its corresponding dictionary is
    empty. If removing Expectations results in a top-level test key having an
    empty dictionary, that test entry will also be removed.

    Returns:
      A dict from expectation file name (str) to set of unused expectations
      (str) from that file.
    """
    logging.info('Filtering out unused expectations')
    unused = collections.defaultdict(list)
    unused_count = 0
    for (expectation_file, expectation,
         builder_map) in self.IterBuilderStepMaps():
      if not builder_map:
        unused[expectation_file].append(expectation)
        unused_count += 1
    for expectation_file, expectations in unused.items():
      for e in expectations:
        del self[expectation_file][e]
    logging.debug('Found %d unused expectations', unused_count)

    empty_files = []
    for expectation_file, expectation_map in self.items():
      if not expectation_map:
        empty_files.append(expectation_file)
    for empty in empty_files:
      del self[empty]
    logging.debug('Found %d empty files: %s', len(empty_files), empty_files)

    return unused


class ExpectationBuilderMap(BaseTypedMap):
  """Typed map for Expectation -> BuilderStepMap."""

  def __setitem__(self, key, value):
    assert isinstance(key, BaseExpectation)
    assert isinstance(value, self._value_type())
    super(ExpectationBuilderMap, self).__setitem__(key, value)

  def _value_type(self):
    return BuilderStepMap


class BuilderStepMap(BaseTypedMap):
  """Typed map for string types -> StepBuildStatsMap."""

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, self._value_type())
    super(BuilderStepMap, self).__setitem__(key, value)

  def _value_type(self):
    return StepBuildStatsMap

  def SplitBuildStatsByPass(self, expectation):
    """Splits the underlying BuildStats data by passing-ness.

    Args:
      expectation: The Expectation that this BuilderStepMap is located under.

    Returns:
      A dict mapping builder name to a tuple (fully_passed, never_passed,
      partially_passed). Each *_passed is a StepBuildStatsMap containing data
      for the steps that either fully passed on all builds, never passed on any
      builds, or passed some of the time.
    """
    retval = {}
    for builder_name, step_map in self.items():
      fully_passed = StepBuildStatsMap()
      never_passed = StepBuildStatsMap()
      partially_passed = StepBuildStatsMap()

      for step_name, stats in step_map.items():
        if stats.NeverNeededExpectation(expectation):
          assert step_name not in fully_passed
          fully_passed[step_name] = stats
        elif stats.AlwaysNeededExpectation(expectation):
          assert step_name not in never_passed
          never_passed[step_name] = stats
        else:
          assert step_name not in partially_passed
          partially_passed[step_name] = stats
      retval[builder_name] = (fully_passed, never_passed, partially_passed)
    return retval

  def IterBuildStats(self):
    """Iterates over all BuildStats contained in the map.

    Returns:
      A generator yielding tuples in the form (builder_name (str), step_name
      (str), build_stats (BuildStats)).
    """
    return self.IterToValueType(BuildStats)


class StepBuildStatsMap(BaseTypedMap):
  """Typed map for string types -> BuildStats"""

  def __setitem__(self, key, value):
    assert IsStringType(key)
    assert isinstance(value, self._value_type())
    super(StepBuildStatsMap, self).__setitem__(key, value)

  def _value_type(self):
    return BuildStats


class BuilderEntry(object):
  """Simple container for defining a builder."""

  def __init__(self, name, builder_type, is_internal_builder):
    """
    Args:
      name: A string containing the name of the builder.
      builder_type: A string containing the type of builder this is, either
          "ci" or "try".
      is_internal_builder: A boolean denoting whether the builder is internal or
          not.
    """
    self.name = name
    self.builder_type = builder_type
    self.is_internal_builder = is_internal_builder

  @property
  def project(self):
    return 'chrome' if self.is_internal_builder else 'chromium'

  def __eq__(self, other):
    return (isinstance(other, BuilderEntry) and self.name == other.name
            and self.builder_type == other.builder_type
            and self.is_internal_builder == other.is_internal_builder)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __hash__(self):
    return hash((self.name, self.builder_type, self.is_internal_builder))


def IsStringType(s):
  return isinstance(s, six.string_types)


Expectation = BaseExpectation
Result = BaseResult
BuildStats = BaseBuildStats
TestExpectationMap = BaseTestExpectationMap
