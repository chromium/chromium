# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for interacting with expectation files."""

import base64
import collections
import os
import posixpath
import re
import urllib.request

import gpu_path_util

from flake_suppressor import tag_utils

from typ import expectations_parser

CHROMIUM_SRC_DIR = gpu_path_util.CHROMIUM_SRC_DIR
RELATIVE_EXPECTATION_FILE_DIRECTORY = os.path.join('content', 'test', 'gpu',
                                                   'gpu_tests',
                                                   'test_expectations')
ABSOLUTE_EXPECTATION_FILE_DIRECTORY = os.path.join(
    CHROMIUM_SRC_DIR, RELATIVE_EXPECTATION_FILE_DIRECTORY)
# For most test suites reported to ResultDB, we can chop off "_integration_test"
# and get the name used for the expectation file. However, there are a few
# special cases, so map those there.
EXPECTATION_FILE_OVERRIDE = {
    'info_collection_test': 'info_collection',
    'trace': 'trace_test',
}
GITILES_URL = 'https://chromium.googlesource.com/chromium/src/+/refs/heads/main'
TEXT_FORMAT_ARG = '?format=TEXT'

TAG_GROUP_REGEX = re.compile(r'# tags: \[([^\]]*)\]', re.MULTILINE | re.DOTALL)


# pylint: disable=too-many-locals
def IterateThroughResultsForUser(result_map, group_by_tags, include_all_tags):
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
  typ_tag_ordered_result_map = _ReorderMapByTypTags(result_map)
  for suite, test_map in result_map.items():
    for test, tag_map in test_map.items():
      for typ_tags, build_url_list in tag_map.items():

        print('')
        print('Suite: %s' % suite)
        print('Test: %s' % test)
        print('Configuration:\n    %s' % '\n    '.join(typ_tags))
        print('Failed builds:\n    %s' % '\n    '.join(build_url_list))

        other_failures_for_test = FindFailuresInSameTest(
            result_map, suite, test, typ_tags)
        if other_failures_for_test:
          print('Other failures in same test found on other configurations')
          for (tags, failure_count) in other_failures_for_test:
            print('    %d failures on %s' % (failure_count, ' '.join(tags)))

        other_failures_for_config = FindFailuresInSameConfig(
            typ_tag_ordered_result_map, suite, test, typ_tags)
        if other_failures_for_config:
          print('Other failures on same configuration found in other tests')
          for (name, failure_count) in other_failures_for_config:
            print('    %d failures in %s' % (failure_count, name))

        expected_result, bug = PromptUserForExpectationAction()
        if not expected_result:
          continue

        ModifyFileForResult(suite, test, typ_tags, bug, expected_result,
                            group_by_tags, include_all_tags)
# pylint: enable=too-many-locals


# pylint: disable=too-many-locals,too-many-arguments
def IterateThroughResultsWithThresholds(result_map, group_by_tags,
                                        result_counts, ignore_threshold,
                                        flaky_threshold, include_all_tags):
  """Iterates over |result_map| and generates expectations based off thresholds.

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
    flaky_threshold: A float containing the fraction of failed tests under which
        failures will be suppressed with RetryOnFailure and above which will be
        suppressed with Failure.
    include_all_tags: A boolean denoting whether all tags should be used for
        expectations or only the most specific ones.
  """
  assert isinstance(ignore_threshold, float)
  assert isinstance(flaky_threshold, float)
  for suite, test_map in result_map.items():
    for test, tag_map in test_map.items():
      for typ_tags, build_url_list in tag_map.items():
        failure_count = len(build_url_list)
        total_count = result_counts[typ_tags][test]
        fraction = failure_count / total_count
        if fraction < ignore_threshold:
          continue
        if fraction < flaky_threshold:
          expected_result = 'RetryOnFailure'
        else:
          expected_result = 'Failure'
        ModifyFileForResult(suite, test, typ_tags, '', expected_result,
                            group_by_tags, include_all_tags)
# pylint: enable=too-many-locals,too-many-arguments


def FindFailuresInSameTest(result_map, target_suite, target_test,
                           target_typ_tags):
  """Finds all other failures that occurred in the given test.

  Ignores the failures for the test on the same configuration.

  Args:
    result_map: Aggregated query results from results.AggregateResults.
    target_suite: A string containing the test suite being checked.
    target_test: A string containing the target test case being checked.
    target_typ_tags: A list of strings containing the typ tags that the failure
        took place on.

  Returns:
    A list of tuples (typ_tags, count). |typ_tags| is a list of strings
    defining a configuration the specified test failed on. |count| is how many
    times the test failed on that configuration.
  """
  other_failures = []
  target_typ_tags = tuple(target_typ_tags)
  tag_map = result_map.get(target_suite, {}).get(target_test, {})
  for typ_tags, build_url_list in tag_map.items():
    if typ_tags == target_typ_tags:
      continue
    other_failures.append((typ_tags, len(build_url_list)))
  return other_failures


def FindFailuresInSameConfig(typ_tag_ordered_result_map, target_suite,
                             target_test, target_typ_tags):
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
    target_typ_tags: A list of strings containing the typ tags defining the
        configuration to find failures for.

  Returns:
    A list of tuples (full_name, count). |full_name| is a string containing a
    test suite and test case concatenated together. |count| is how many times
    |full_name| failed on the configuration specified by |target_typ_tags|.
  """
  target_typ_tags = tuple(target_typ_tags)
  other_failures = []
  suite_map = typ_tag_ordered_result_map.get(target_typ_tags, {})
  for suite, test_map in suite_map.items():
    for test, build_url_list in test_map.items():
      if suite == target_suite and test == target_test:
        continue
      full_name = '%s.%s' % (suite, test)
      other_failures.append((full_name, len(build_url_list)))
  return other_failures


def _ReorderMapByTypTags(result_map):
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


def PromptUserForExpectationAction():
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
def ModifyFileForResult(suite, test, typ_tags, bug, expected_result,
                        group_by_tags, include_all_tags):
  """Adds an expectation to the appropriate expectation file.

  Args:
    suite: A string containing the suite the failure occurred in.
    test: A string containing the test case the failure occurred in.
    typ_tags: A list of strings containing the typ tags the test produced.
    bug: A string containing the bug to associate with the new expectation.
    expected_result: A string containing the expected result to use for the new
        expectation, e.g. RetryOnFailure.
    group_by_tags: A boolean denoting whether to attempt to group expectations
        by tags or not. If True, expectations will be added after an existing
        expectation whose tags are the largest subset of the produced tags. If
        False, new expectations will be appended to the end of the file.
    include_all_tags: A boolean denoting whether all tags should be used for
        expectations or only the most specific ones.
  """
  expectation_file = GetExpectationFileForSuite(suite, typ_tags)
  if not include_all_tags:
    # Remove temporarily un-ignored tags, namely webgl-version-x tags, since
    # those were necessary to find the correct file. However, we do not want
    # to actually include them in the file since they are unused/ignored.
    typ_tags = tag_utils.RemoveTemporarilyKeptIgnoredTags(typ_tags)
    typ_tags = FilterToMostSpecificTypTags(typ_tags, expectation_file)
  bug = '%s ' % bug if bug else bug

  def AppendExpectationToEnd():
    expectation_line = '%s[ %s ] %s [ %s ]\n' % (bug, ' '.join(typ_tags), test,
                                                 expected_result)
    with open(expectation_file, 'a') as outfile:
      outfile.write(expectation_line)

  if group_by_tags:
    insertion_line, best_matching_tags = FindBestInsertionLineForExpectation(
        typ_tags, expectation_file)
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
      tags_to_use = list(tags_to_use)
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
def FilterToMostSpecificTypTags(typ_tags, expectation_file):
  """Filters |typ_tags| to the most specific set.

  Assumes that the tags in |expectation_file| are ordered from least specific
  to most specific within each tag group.

  Args:
    typ_tags: A list of strings containing the typ tags the test produced.
    expectation_file: A string containing a filepath pointing to the
        expectation file to filter tags with.

  Returns:
    A list containing the contents of |typ_tags| with only the most specific
    tag from each tag group remaining.
  """
  with open(expectation_file) as infile:
    contents = infile.read()

  tag_groups = []
  for match in TAG_GROUP_REGEX.findall(contents):
    tag_groups.append(match.strip().replace('#', '').split())

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
  return filtered_tags
# pylint: enable=too-many-locals


def GetExpectationFileForSuite(suite, typ_tags):
  """Finds the correct expectation file for the given suite.

  Args:
    suite: A string containing the test suite to look for.
    typ_tags: A list of strings containing typ tags that were produced by the
        failing test.

  Returns:
    A string containing a filepath to the correct expectation file for |suite|
    and |typ_tags|.
  """
  truncated_suite = suite.replace('_integration_test', '')
  if truncated_suite == 'webgl_conformance':
    if 'webgl-version-2' in typ_tags:
      truncated_suite = 'webgl2_conformance'

  expectation_file = EXPECTATION_FILE_OVERRIDE.get(truncated_suite,
                                                   truncated_suite)
  expectation_file += '_expectations.txt'
  expectation_file = os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                  expectation_file)
  return expectation_file


def FindBestInsertionLineForExpectation(typ_tags, expectation_file):
  """Finds the best place to insert an expectation when grouping by tags.

  Args:
    typ_tags: A list of strings containing typ tags that were produced by the
        failing test.
    expectation_file: A string containing a filepath to the expectation file to
        use.

  Returns:
    A tuple (insertion_line, best_matching_tags). |insertion_line| is an int
    specifying the line number to insert the expectation into.
    |best_matching_tags| is a set containing the tags of an existing expectation
    that was found to be the closest match. If no appropriate line is found,
    |insertion_line| is -1 and |best_matching_tags| is empty.
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


def GetExpectationFilesFromOrigin():
  """Gets expectation file contents from origin/main.

  Returns:
    A dict of expectation file name (str) -> expectation file contents (str)
    that are available on origin/main.
  """
  # Get the path to the expectation file directory in gitiles, i.e. the POSIX
  # path relative to the Chromium src directory.
  origin_dir = RELATIVE_EXPECTATION_FILE_DIRECTORY.replace(os.sep, '/')

  origin_dir_url = posixpath.join(GITILES_URL, origin_dir) + TEXT_FORMAT_ARG
  response = urllib.request.urlopen(origin_dir_url).read()
  # Response is a base64 encoded, newline-separated list of files in the
  # directory in the format: `mode file_type hash name`
  files = []
  decoded_text = base64.b64decode(response).decode('utf-8')
  for line in decoded_text.splitlines():
    files.append(line.split()[-1])

  origin_file_contents = {}
  for f in (f for f in files if f.endswith('.txt')):
    origin_filepath = posixpath.join(origin_dir, f)
    origin_filepath_url = posixpath.join(GITILES_URL,
                                         origin_filepath) + TEXT_FORMAT_ARG
    response = urllib.request.urlopen(origin_filepath_url).read()
    decoded_text = base64.b64decode(response).decode('utf-8')
    origin_file_contents[f] = decoded_text

  return origin_file_contents


def GetExpectationFilesFromLocalCheckout():
  """Gets expectaiton file contents from the local checkout.

  Returns:
    A dict of expectation file name (str) -> expectation file contents (str)
    that are available from the local checkout.
  """
  local_file_contents = {}
  for f in (f for f in os.listdir(ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
            if f.endswith('.txt')):
    with open(os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY, f)) as infile:
      local_file_contents[f] = infile.read()
  return local_file_contents


def AssertCheckoutIsUpToDate():
  """Confirms that the local checkout's expectations are up to date."""
  origin_file_contents = GetExpectationFilesFromOrigin()
  local_file_contents = GetExpectationFilesFromLocalCheckout()
  if origin_file_contents != local_file_contents:
    raise RuntimeError(
        'Local Chromium checkout expectations are out of date. Please perform '
        'a `git pull`.')
