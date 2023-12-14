# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for interacting with expectation files."""

import base64
import collections
from datetime import timedelta, date
import itertools
import os
import posixpath
import re
from typing import Dict, List, Set, Tuple, Union
import urllib.request

from flake_suppressor_common import common_typing as ct

from typ import expectations_parser

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
GITILES_URL = 'https://chromium.googlesource.com/chromium/src/+/refs/heads/main'
TEXT_FORMAT_ARG = '?format=TEXT'

TAG_GROUP_REGEX = re.compile(r'# tags: \[([^\]]*)\]', re.MULTILINE | re.DOTALL)

TestToUrlsType = Dict[str, List[str]]
SuiteToTestsType = Dict[str, TestToUrlsType]
TagOrderedAggregateResultType = Dict[ct.TagTupleType, SuiteToTestsType]


def OverFailedBuildThreshold(failed_result_tuple_list: List[ct.ResultTupleType],
                             build_fail_total_number_threshold: int) -> bool:
  """Check if the number of failed build in |failed_result_tuple_list| is
     equal to or more than |build_fail_total_number_threshold|.

  Args:
    failed_result_tuple_list: A list of ct.ResultTupleType failed test results.
    build_fail_total_number_threshold: Threshold base on the number of failed
      build caused by a test.

  Returns:
      Whether number of failed build in |failed_result_tuple_list| is equal to
      or more than |build_fail_total_number_threshold|.
  """
  unique_build_ids = set()
  for result in failed_result_tuple_list:
    if '/' in result.build_url:
      unique_build_ids.add(result.build_url.split('/')[-1])
      if len(unique_build_ids) >= build_fail_total_number_threshold:
        return True
  return False


def OverFailedBuildByConsecutiveDayThreshold(
    failed_result_tuple_list: List[ct.ResultTupleType],
    build_fail_consecutive_day_threshold: int) -> bool:
  """Check if the max number of build fail in consecutive date
     is equal to or more than |build_fail_consecutive_day_threshold|.

  Args:
    failed_result_tuple_list: A list of ct.ResultTupleType failed test result.
    build_fail_consecutive_day_threshold: Threshold base on the number of
      consecutive days that a test caused build fail.

  Returns:
      Whether the max number of build fail in consecutive date
      is equal to or more than |build_fail_consecutive_day_threshold|.
  """
  dates = {t.date: False for t in failed_result_tuple_list}

  for cur_date, is_checked in dates.items():
    # A beginning point.
    if not is_checked:
      count = 1

      while count < build_fail_consecutive_day_threshold:
        new_date = cur_date + timedelta(days=count)
        if new_date in dates:
          count += 1
          # Mark checked date.
          dates[new_date] = True
        else:
          break

      if count >= build_fail_consecutive_day_threshold:
        return True

  return False


def FailedBuildWithinRecentDayThreshold(
    failed_result_tuple_list: List[ct.ResultTupleType],
    build_fail_recent_day_threshold: int) -> bool:
  """Check if there are any failed builds within the most
    recent |build_fail_latest_day_threshold| days.

  Args:
    failed_result_tuple_list: A list of ct.ResultTupleType failed test result.
    build_fail_recent_day_threshold: Threshold base on the recent day range
      that the test caused build fail.

  Returns:
      Whether the test caused build fail within the recent day.
  """
  recent_check_day = date.today() - timedelta(
      days=build_fail_recent_day_threshold)
  for test in failed_result_tuple_list:
    if test.date >= recent_check_day:
      return True
  return False


class ExpectationProcessor():
  # pylint: disable=too-many-locals
  def IterateThroughResultsForUser(self, result_map: ct.AggregatedResultsType,
                                   group_by_tags: bool,
                                   include_all_tags: bool) -> None:
    """Iterates over |result_map| for the user to provide input.

    For each unique result, user will be able to decide whether to ignore it (do
    nothing), mark as flaky (add RetryOnFailure expectation), or mark as failing
    (add Failure expectation). If the latter two are chosen, they can also
    associate a bug with the new expectation.

    Args:
      result_map: Aggregated query results from results.AggregateResults to
          iterate over.
      group_by_tags: A boolean denoting whether to attempt to group expectations
          by tags or not. If True, expectations will be added after an existing
          expectation whose tags are the largest subset of the produced tags. If
          False, new expectations will be appended to the end of the file.
      include_all_tags: A boolean denoting whether all tags should be used for
          expectations or only the most specific ones.
    """
    typ_tag_ordered_result_map = self._ReorderMapByTypTags(result_map)
    for suite, test_map in result_map.items():
      if self.IsSuiteUnsupported(suite):
        continue
      for test, tag_map in test_map.items():
        for typ_tags, build_url_list in tag_map.items():

          print('')
          print('Suite: %s' % suite)
          print('Test: %s' % test)
          print('Configuration:\n    %s' % '\n    '.join(typ_tags))
          print('Failed builds:\n    %s' % '\n    '.join(build_url_list))

          other_failures_for_test = self.FindFailuresInSameTest(
              result_map, suite, test, typ_tags)
          if other_failures_for_test:
            print('Other failures in same test found on other configurations')
            for (tags, failure_count) in other_failures_for_test:
              print('    %d failures on %s' % (failure_count, ' '.join(tags)))

          other_failures_for_config = self.FindFailuresInSameConfig(
              typ_tag_ordered_result_map, suite, test, typ_tags)
          if other_failures_for_config:
            print('Other failures on same configuration found in other tests')
            for (name, failure_count) in other_failures_for_config:
              print('    %d failures in %s' % (failure_count, name))

          expected_result, bug = self.PromptUserForExpectationAction()
          if not expected_result:
            continue

          self.ModifyFileForResult(suite, test, typ_tags, bug, expected_result,
                                   group_by_tags, include_all_tags)

  # pylint: enable=too-many-locals

  # pylint: disable=too-many-locals,too-many-arguments
  def IterateThroughResultsWithThresholds(
      self, result_map: ct.AggregatedResultsType, group_by_tags: bool,
      result_counts: ct.ResultCountType, ignore_threshold: float,
      flaky_threshold: float, include_all_tags: bool) -> None:
    """Iterates over |result_map| and generates expectations based off
       thresholds.

    Args:
      result_map: Aggregated query results from results.AggregateResults to
          iterate over.
      group_by_tags: A boolean denoting whether to attempt to group expectations
          by tags or not. If True, expectations will be added after an existing
          expectation whose tags are the largest subset of the produced tags. If
          False, new expectations will be appended to the end of the file.
      result_counts: A dict in the format output by queries.GetResultCounts.
      ignore_threshold: A float containing the fraction of failed tests under
          which failures will be ignored.
      flaky_threshold: A float containing the fraction of failed tests under
          which failures will be suppressed with RetryOnFailure and above which
          will be suppressed with Failure.
      include_all_tags: A boolean denoting whether all tags should be used for
          expectations or only the most specific ones.
    """
    assert isinstance(ignore_threshold, float)
    assert isinstance(flaky_threshold, float)
    for suite, test_map in result_map.items():
      if self.IsSuiteUnsupported(suite):
        continue
      for test, tag_map in test_map.items():
        for typ_tags, build_url_list in tag_map.items():
          failure_count = len(build_url_list)
          total_count = result_counts[typ_tags][test]
          fraction = failure_count / total_count
          if fraction < ignore_threshold:
            continue
          expected_result = self.GetExpectedResult(fraction, flaky_threshold)
          if expected_result:
            self.ModifyFileForResult(suite, test, typ_tags, '', expected_result,
                                     group_by_tags, include_all_tags)

  def CreateExpectationsForAllResults(
      self, result_map: ct.AggregatedStatusResultsType, group_by_tags: bool,
      include_all_tags: bool, build_fail_total_number_threshold: int,
      build_fail_consecutive_day_threshold: int,
      build_fail_recent_day_threshold: int) -> None:
    """Iterates over |result_map|, selects tests that hit all
       build-fail*-thresholds and adds expectations for their results. Same
       test in all builders that caused build fail must be over all threshold
       requirement.

    Args:
      result_map: Aggregated query results from results.AggregateResults to
          iterate over.
      group_by_tags: A boolean denoting whether to attempt to group expectations
          by tags or not. If True, expectations will be added after an existing
          expectation whose tags are the largest subset of the produced tags. If
          False, new expectations will be appended to the end of the file.
      include_all_tags: A boolean denoting whether all tags should be used for
          expectations or only the most specific ones.
      build_fail_total_number_threshold: Threshold based on the number of
          failed builds caused by a test. Add to the expectations, if actual
          is equal to or more than this threshold. All build-fail*-thresholds
          must be hit in order for a test to actually be suppressed.
      build_fail_consecutive_day_threshold: Threshold based on the number of
          consecutive days that a test caused build fail. Add to the
          expectations, if the consecutive days that it caused build fail
          are equal to or more than this. All build-fail*-thresholds
          must be hit in order for a test to actually be suppressed.
      build_fail_recent_day_threshold: How many days worth of recent builds
          to check for non-hidden failures. A test will be suppressed if
          it has non-hidden failures within this time span. All
          build-fail*-thresholds must be hit in order for a test to actually
          be suppressed.
    """
    for suite, test_map in result_map.items():
      if self.IsSuiteUnsupported(suite):
        continue
      for test, tag_map in test_map.items():
        # Same test in all builders that caused build fail must be over all
        # threshold requirement.
        all_results = list(itertools.chain(*tag_map.values()))
        if (not OverFailedBuildThreshold(all_results,
                                         build_fail_total_number_threshold)
            or not OverFailedBuildByConsecutiveDayThreshold(
                all_results, build_fail_consecutive_day_threshold)):
          continue
        for typ_tags, result_tuple_list in tag_map.items():
          if not FailedBuildWithinRecentDayThreshold(
              result_tuple_list, build_fail_recent_day_threshold):
            continue
          status = set()
          for test_result in result_tuple_list:
            # Should always add a pass to all flaky web tests in
            # TestsExpectation that have passed runs.
            status.add('Pass')
            if test_result.status == ct.ResultStatus.CRASH:
              status.add('Crash')
            elif test_result.status == ct.ResultStatus.FAIL:
              status.add('Failure')
            elif test_result.status == ct.ResultStatus.ABORT:
              status.add('Timeout')
          if status:
            status_list = list(status)
            status_list.sort()
            self.ModifyFileForResult(suite, test, typ_tags, '',
                                     ' '.join(status_list), group_by_tags,
                                     include_all_tags)

  # pylint: enable=too-many-locals,too-many-arguments

  def FindFailuresInSameTest(self, result_map: ct.AggregatedResultsType,
                             target_suite: str, target_test: str,
                             target_typ_tags: ct.TagTupleType
                             ) -> List[Tuple[ct.TagTupleType, int]]:
    """Finds all other failures that occurred in the given test.

    Ignores the failures for the test on the same configuration.

    Args:
      result_map: Aggregated query results from results.AggregateResults.
      target_suite: A string containing the test suite being checked.
      target_test: A string containing the target test case being checked.
      target_typ_tags: A tuple of strings containing the typ tags that the
          failure took place on.

    Returns:
      A list of tuples (typ_tags, count). |typ_tags| is a list of strings
      defining a configuration the specified test failed on. |count| is how many
      times the test failed on that configuration.
    """
    assert isinstance(target_typ_tags, tuple)
    other_failures = []
    tag_map = result_map.get(target_suite, {}).get(target_test, {})
    for typ_tags, build_url_list in tag_map.items():
      if typ_tags == target_typ_tags:
        continue
      other_failures.append((typ_tags, len(build_url_list)))
    return other_failures

  def FindFailuresInSameConfig(
      self, typ_tag_ordered_result_map: TagOrderedAggregateResultType,
      target_suite: str, target_test: str,
      target_typ_tags: ct.TagTupleType) -> List[Tuple[str, int]]:
    """Finds all other failures that occurred on the given configuration.

    Ignores the failures for the given test on the given configuration.

    Args:
      typ_tag_ordered_result_map: Aggregated query results from
          results.AggregateResults that have been reordered using
          _ReorderMapByTypTags.
      target_suite: A string containing the test suite the original failure was
          found in.
      target_test: A string containing the test case the original failure was
          found in.
      target_typ_tags: A tuple of strings containing the typ tags defining the
          configuration to find failures for.

    Returns:
      A list of tuples (full_name, count). |full_name| is a string containing a
      test suite and test case concatenated together. |count| is how many times
      |full_name| failed on the configuration specified by |target_typ_tags|.
    """
    assert isinstance(target_typ_tags, tuple)
    other_failures = []
    suite_map = typ_tag_ordered_result_map.get(target_typ_tags, {})
    for suite, test_map in suite_map.items():
      for test, build_url_list in test_map.items():
        if suite == target_suite and test == target_test:
          continue
        full_name = '%s.%s' % (suite, test)
        other_failures.append((full_name, len(build_url_list)))
    return other_failures

  def _ReorderMapByTypTags(self, result_map: ct.AggregatedResultsType
                           ) -> TagOrderedAggregateResultType:
    """Rearranges|result_map| to use typ tags as the top level keys.

    Args:
      result_map: Aggregated query results from results.AggregateResults

    Returns:
      A dict containing the same contents as |result_map|, but in the following
      format:
      {
        typ_tags (tuple of str): {
          suite (str): {
            test (str): build_url_list (list of str),
          },
        },
      }
    """
    reordered_map = {}
    for suite, test_map in result_map.items():
      for test, tag_map in test_map.items():
        for typ_tags, build_url_list in tag_map.items():
          reordered_map.setdefault(typ_tags,
                                   {}).setdefault(suite,
                                                  {})[test] = build_url_list
    return reordered_map

  def PromptUserForExpectationAction(
      self) -> Union[Tuple[str, str], Tuple[None, None]]:
    """Prompts the user on what to do to handle a failure.

    Returns:
      A tuple (expected_result, bug). |expected_result| is a string containing
      the expected result to use for the expectation, e.g. RetryOnFailure. |bug|
      is a string containing the bug to use for the expectation. If the user
      chooses to ignore the failure, both will be None. Otherwise, both are
      filled, although |bug| may be an empty string if no bug is provided.
    """
    prompt = ('How should this failure be handled? (i)gnore/(r)etry on '
              'failure/(f)ailure: ')
    valid_inputs = ['f', 'i', 'r']
    response = input(prompt).lower()
    while response not in valid_inputs:
      print('Invalid input, valid inputs are %s' % (', '.join(valid_inputs)))
      response = input(prompt).lower()

    if response == 'i':
      return (None, None)
    expected_result = 'RetryOnFailure' if response == 'r' else 'Failure'

    prompt = ('What is the bug URL that should be associated with this '
              'expectation? E.g. crbug.com/1234. ')
    response = input(prompt)
    return (expected_result, response)

  # pylint: disable=too-many-locals,too-many-arguments
  def ModifyFileForResult(self, suite: str, test: str,
                          typ_tags: ct.TagTupleType, bug: str,
                          expected_result: str, group_by_tags: bool,
                          include_all_tags: bool) -> None:
    """Adds an expectation to the appropriate expectation file.

    Args:
      suite: A string containing the suite the failure occurred in.
      test: A string containing the test case the failure occurred in.
      typ_tags: A tuple of strings containing the typ tags the test produced.
      bug: A string containing the bug to associate with the new expectation.
      expected_result: A string containing the expected result to use for the
          new expectation, e.g. RetryOnFailure.
      group_by_tags: A boolean denoting whether to attempt to group expectations
          by tags or not. If True, expectations will be added after an existing
          expectation whose tags are the largest subset of the produced tags. If
          False, new expectations will be appended to the end of the file.
      include_all_tags: A boolean denoting whether all tags should be used for
          expectations or only the most specific ones.
    """
    expectation_file = self.GetExpectationFileForSuite(suite, typ_tags)
    if not include_all_tags:
      typ_tags = self.FilterToMostSpecificTypTags(typ_tags, expectation_file)
    bug = '%s ' % bug if bug else bug

    def AppendExpectationToEnd():
      expectation_line = '%s[ %s ] %s [ %s ]\n' % (bug, ' '.join(
          self.ProcessTypTagsBeforeWriting(typ_tags)), test, expected_result)
      with open(expectation_file, 'a') as outfile:
        outfile.write(expectation_line)

    if group_by_tags:
      insertion_line, best_matching_tags = (
          self.FindBestInsertionLineForExpectation(typ_tags, expectation_file))
      if insertion_line == -1:
        AppendExpectationToEnd()
      else:
        # If we've already filtered tags, then use those instead of the "best
        # matching" ones.
        tags_to_use = best_matching_tags
        if not include_all_tags:
          tags_to_use = typ_tags
        # enumerate starts at 0 but line numbers start at 1.
        insertion_line -= 1
        tags_to_use = list(self.ProcessTypTagsBeforeWriting(tags_to_use))
        tags_to_use.sort()
        expectation_line = '%s[ %s ] %s [ %s ]\n' % (bug, ' '.join(tags_to_use),
                                                     test, expected_result)
        with open(expectation_file) as infile:
          input_contents = infile.read()
        output_contents = ''
        for lineno, line in enumerate(input_contents.splitlines(True)):
          output_contents += line
          if lineno == insertion_line:
            output_contents += expectation_line
        with open(expectation_file, 'w') as outfile:
          outfile.write(output_contents)
    else:
      AppendExpectationToEnd()

  # pylint: enable=too-many-locals,too-many-arguments

  # pylint: disable=too-many-locals
  def FilterToMostSpecificTypTags(self, typ_tags: ct.TagTupleType,
                                  expectation_file: str) -> ct.TagTupleType:
    """Filters |typ_tags| to the most specific set.

    Assumes that the tags in |expectation_file| are ordered from least specific
    to most specific within each tag group.

    Args:
      typ_tags: A tuple of strings containing the typ tags the test produced.
      expectation_file: A string containing a filepath pointing to the
          expectation file to filter tags with.

    Returns:
      A tuple containing the contents of |typ_tags| with only the most specific
      tag from each tag group remaining.
    """
    with open(expectation_file) as infile:
      contents = infile.read()

    tag_groups = self.GetTagGroups(contents)
    num_matches = 0
    tags_in_same_group = collections.defaultdict(list)
    for tag in typ_tags:
      for index, tag_group in enumerate(tag_groups):
        if tag in tag_group:
          tags_in_same_group[index].append(tag)
          num_matches += 1
          break
    if num_matches != len(typ_tags):
      all_tags = set()
      for group in tag_groups:
        all_tags |= set(group)
      raise RuntimeError('Found tags not in expectation file: %s' %
                         ' '.join(set(typ_tags) - all_tags))

    filtered_tags = []
    for index, tags in tags_in_same_group.items():
      if len(tags) == 1:
        filtered_tags.append(tags[0])
      else:
        tag_group = tag_groups[index]
        best_index = -1
        for t in tags:
          i = tag_group.index(t)
          if i > best_index:
            best_index = i
        filtered_tags.append(tag_group[best_index])

    # Sort to keep order consistent with what we were given.
    filtered_tags.sort()
    return tuple(filtered_tags)

  # pylint: enable=too-many-locals

  def FindBestInsertionLineForExpectation(self, typ_tags: ct.TagTupleType,
                                          expectation_file: str
                                          ) -> Tuple[int, Set[str]]:
    """Finds the best place to insert an expectation when grouping by tags.

    Args:
      typ_tags: A tuple of strings containing typ tags that were produced by the
          failing test.
      expectation_file: A string containing a filepath to the expectation file
      to use.

    Returns:
      A tuple (insertion_line, best_matching_tags). |insertion_line| is an int
      specifying the line number to insert the expectation into.
      |best_matching_tags| is a set containing the tags of an existing
      expectation that was found to be the closest match. If no appropriate
      line is found, |insertion_line| is -1 and |best_matching_tags| is empty.
    """
    best_matching_tags = set()
    best_insertion_line = -1
    with open(expectation_file) as f:
      content = f.read()
    list_parser = expectations_parser.TaggedTestListParser(content)
    for e in list_parser.expectations:
      expectation_tags = e.tags
      if not expectation_tags.issubset(typ_tags):
        continue
      if len(expectation_tags) > len(best_matching_tags):
        best_matching_tags = expectation_tags
        best_insertion_line = e.lineno
      elif len(expectation_tags) == len(best_matching_tags):
        if best_insertion_line < e.lineno:
          best_insertion_line = e.lineno
    return best_insertion_line, best_matching_tags

  def GetOriginExpectationFileContents(self) -> Dict[str, str]:
    """Gets expectation file contents from origin/main.

    Returns:
      A dict of expectation file name (str) -> expectation file contents (str)
      that are available on origin/main. File paths are relative to the
      Chromium src dir and are OS paths.
    """
    # Get the path to the expectation file directory in gitiles, i.e. the POSIX
    # path relative to the Chromium src directory.
    origin_file_contents = {}
    expectation_files = self.ListOriginExpectationFiles()
    for f in expectation_files:
      filepath_posix = f.replace(os.sep, '/')
      origin_filepath_url = posixpath.join(GITILES_URL,
                                           filepath_posix) + TEXT_FORMAT_ARG
      response = urllib.request.urlopen(origin_filepath_url).read()
      decoded_text = base64.b64decode(response).decode('utf-8')
      # After the URL access maintain all the paths as os paths.
      origin_file_contents[f] = decoded_text

    return origin_file_contents

  def GetLocalCheckoutExpectationFileContents(self) -> Dict[str, str]:
    """Gets expectation file contents from the local checkout.

    Returns:
      A dict of expectation file name (str) -> expectation file contents (str)
      that are available from the local checkout. File paths are relative to
      the Chromium src dir and are OS paths.
    """
    local_file_contents = {}
    expectation_files = self.ListLocalCheckoutExpectationFiles()
    for f in expectation_files:
      absolute_filepath = os.path.join(CHROMIUM_SRC_DIR, f)
      with open(absolute_filepath) as infile:
        local_file_contents[f] = infile.read()
    return local_file_contents

  def AssertCheckoutIsUpToDate(self) -> None:
    """Confirms that the local checkout's expectations are up to date."""
    origin_file_contents = self.GetOriginExpectationFileContents()
    local_file_contents = self.GetLocalCheckoutExpectationFileContents()
    if origin_file_contents != local_file_contents:
      raise RuntimeError(
          'Local Chromium checkout expectations are out of date. Please '
          'perform a `git pull`.')

  def GetExpectationFileForSuite(self, suite: str,
                                 typ_tags: ct.TagTupleType) -> str:
    """Finds the correct expectation file for the given suite.

    Args:
      suite: A string containing the test suite to look for.
      typ_tags: A tuple of strings containing typ tags that were produced by
          the failing test.

    Returns:
      A string containing a filepath to the correct expectation file for
      |suite|and |typ_tags|.
    """
    raise NotImplementedError

  def ListGitilesDirectory(self, origin_dir: str) -> List[str]:
    """Gets the list of all files from origin/main under origin_dir.

    Args:
      origin_dir: A string containing the path to the directory containing
      expectation files. Path is relative to the Chromium src dir.

    Returns:
      A list of filename strings under origin_dir.
    """
    origin_dir_url = posixpath.join(GITILES_URL, origin_dir) + TEXT_FORMAT_ARG
    response = urllib.request.urlopen(origin_dir_url).read()
    # Response is a base64 encoded, newline-separated list of files in the
    # directory in the format: `mode file_type hash name`
    files = []
    decoded_text = base64.b64decode(response).decode('utf-8')
    for line in decoded_text.splitlines():
      files.append(line.split()[-1])
    return files

  def IsSuiteUnsupported(self, suite: str) -> bool:
    raise NotImplementedError

  def ListLocalCheckoutExpectationFiles(self) -> List[str]:
    """Finds the list of all expectation files from the local checkout.

    Returns:
      A list of strings containing relative file paths to expectation files.
      OS paths relative to Chromium src dir are returned.
    """
    raise NotImplementedError

  def ListOriginExpectationFiles(self) -> List[str]:
    """Finds the list of all expectation files from origin/main.

    Returns:
      A list of strings containing relative file paths to expectation files.
      OS paths are relative to Chromium src directory.
    """
    raise NotImplementedError

  def GetTagGroups(self, contents: str) -> List[List[str]]:
    tag_groups = []
    for match in TAG_GROUP_REGEX.findall(contents):
      tag_groups.append(match.strip().replace('#', '').split())
    return tag_groups

  def GetExpectedResult(self, fraction: float, flaky_threshold: float) -> str:
    raise NotImplementedError

  def ProcessTypTagsBeforeWriting(self,
                                  typ_tags: ct.TagTupleType) -> ct.TagTupleType:
    return typ_tags
