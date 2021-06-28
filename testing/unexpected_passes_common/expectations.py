# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

from __future__ import print_function

import collections
import copy
import logging
import os
import sys

import validate_tag_consistency

from typ import expectations_parser
from unexpected_passes_common import data_types
from unexpected_passes_common import result_output

EXPECTATIONS_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', 'content', 'test',
                 'gpu', 'gpu_tests', 'test_expectations'))

FINDER_DISABLE_COMMENT = 'finder:disable'
FINDER_ENABLE_COMMENT = 'finder:enable'

FULL_PASS = 1
NEVER_PASS = 2
PARTIAL_PASS = 3


def CreateTestExpectationMap(expectation_file, tests):
  """Creates an expectation map based off a file or list of tests.

  Args:
    expectation_file: A filepath to an expectation file to read from, or None.
        If a filepath is specified, |tests| must be None.
    tests: An iterable of strings containing test names to check. If specified,
        |expectation_file| must be None.

  Returns:
    A data_types.TestExpectationMap, although all its BuilderStepMap contents
    will be empty.
  """
  logging.info('Creating test expectation map')
  assert expectation_file or tests
  assert not (expectation_file and tests)

  if expectation_file:
    with open(expectation_file) as f:
      content = f.read()
  else:
    content = '# results: [ RetryOnFailure ]\n'
    for t in tests:
      content += '%s [ RetryOnFailure ]\n' % t

  list_parser = expectations_parser.TaggedTestListParser(content)
  expectation_map = data_types.TestExpectationMap()
  logging.debug('Parsed %d expectations', len(list_parser.expectations))
  for e in list_parser.expectations:
    if 'Skip' in e.raw_results:
      continue
    expectation = data_types.Expectation(e.test, e.tags, e.raw_results,
                                         e.reason)
    expectations_for_test = expectation_map.setdefault(
        e.test, data_types.ExpectationBuilderMap())
    assert expectation not in expectations_for_test
    expectations_for_test[expectation] = data_types.BuilderStepMap()

  return expectation_map


def FilterOutUnusedExpectations(test_expectation_map):
  """Filters out any unused Expectations from |test_expectation_map|.

  An Expectation is considered unused if its corresponding dictionary is empty.
  If removing Expectations results in a top-level test key having an empty
  dictionary, that test entry will also be removed.

  Args:
    test_expectation_map: A data_types.TestExpectationMap. Will be modified in
        place.

  Returns:
    A list containing any Expectations that were removed.
  """
  assert isinstance(test_expectation_map, data_types.TestExpectationMap)
  logging.info('Filtering out unused expectations')
  unused_expectations = []
  for _, expectation, builder_map in test_expectation_map.IterBuilderStepMaps():
    if not builder_map:
      unused_expectations.append(expectation)
  for unused in unused_expectations:
    for _, expectation_map in test_expectation_map.items():
      if unused in expectation_map:
        del expectation_map[unused]
  logging.debug('Found %d unused expectations', len(unused_expectations))

  empty_tests = []
  for test_name, expectation_map in test_expectation_map.items():
    if not expectation_map:
      empty_tests.append(test_name)
  for empty in empty_tests:
    del test_expectation_map[empty]
  logging.debug('Found %d empty tests: %s', len(empty_tests), empty_tests)

  return unused_expectations


def SplitExpectationsByStaleness(test_expectation_map):
  """Separates |test_expectation_map| based on expectation staleness.

  Args:
    test_expectation_map: A data_types.TestExpectationMap with any unused
        expectations already filtered out.

  Returns:
    Three data_types.TestExpectationMaps (stale_dict, semi_stale_dict,
    active_dict). All three combined contain the information of
    |test_expectation_map|. |stale_dict| contains entries for expectations that
    are no longer being helpful, |semi_stale_dict| contains entries for
    expectations that might be removable or modifiable, but have at least one
    failed test run. |active_dict| contains entries for expectations that are
    preventing failures on all builders they're active on, and thus shouldn't be
    removed.
  """
  assert isinstance(test_expectation_map, data_types.TestExpectationMap)

  stale_dict = data_types.TestExpectationMap()
  semi_stale_dict = data_types.TestExpectationMap()
  active_dict = data_types.TestExpectationMap()

  # This initially looks like a good target for using
  # data_types.TestExpectationMap's iterators since there are many nested loops.
  # However, we need to reset state in different loops, and the alternative of
  # keeping all the state outside the loop and resetting under certain
  # conditions ends up being less readable than just using nested loops.
  for test_name, expectation_map in test_expectation_map.items():
    for expectation, builder_map in expectation_map.items():
      # A temporary map to hold data so we can later determine whether an
      # expectation is stale, semi-stale, or active.
      tmp_map = {
          FULL_PASS: data_types.BuilderStepMap(),
          NEVER_PASS: data_types.BuilderStepMap(),
          PARTIAL_PASS: data_types.BuilderStepMap(),
      }

      split_stats_map = builder_map.SplitBuildStatsByPass()
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
            builder_map.setdefault(builder,
                                   data_types.StepBuildStatsMap()).update(steps)

      # Handle the case of a stale expectation.
      if not (tmp_map[NEVER_PASS] or tmp_map[PARTIAL_PASS]):
        builder_map = stale_dict.setdefault(
            test_name, data_types.ExpectationBuilderMap()).setdefault(
                expectation, data_types.BuilderStepMap())
        _CopyPassesIntoBuilderMap(builder_map, [FULL_PASS])
      # Handle the case of an active expectation.
      elif not tmp_map[FULL_PASS]:
        builder_map = active_dict.setdefault(
            test_name, data_types.ExpectationBuilderMap()).setdefault(
                expectation, data_types.BuilderStepMap())
        _CopyPassesIntoBuilderMap(builder_map, [NEVER_PASS, PARTIAL_PASS])
      # Handle the case of a semi-stale expectation.
      else:
        # TODO(crbug.com/998329): Sort by pass percentage so it's easier to find
        # problematic builders without highlighting.
        builder_map = semi_stale_dict.setdefault(
            test_name, data_types.ExpectationBuilderMap()).setdefault(
                expectation, data_types.BuilderStepMap())
        _CopyPassesIntoBuilderMap(builder_map,
                                  [FULL_PASS, PARTIAL_PASS, NEVER_PASS])
  return stale_dict, semi_stale_dict, active_dict


def RemoveExpectationsFromFile(expectations, expectation_file):
  """Removes lines corresponding to |expectations| from |expectation_file|.

  Ignores any lines that match but are within a disable block or have an inline
  disable comment.

  Args:
    expectations: A list of data_types.Expectations to remove.
    expectation_file: A filepath pointing to an expectation file to remove lines
        from.

  Returns:
    A set of strings containing URLs of bugs associated with the removed
    expectations.
  """

  with open(expectation_file) as f:
    input_contents = f.read()

  output_contents = ''
  in_disable_block = False
  disable_block_reason = ''
  removed_urls = set()
  for line in input_contents.splitlines(True):
    # Auto-add any comments or empty lines
    stripped_line = line.strip()
    if _IsCommentOrBlankLine(stripped_line):
      output_contents += line
      assert not (FINDER_DISABLE_COMMENT in line
                  and FINDER_ENABLE_COMMENT in line)
      # Handle disable/enable block comments.
      if FINDER_DISABLE_COMMENT in line:
        if in_disable_block:
          raise RuntimeError(
              'Invalid expectation file %s - contains a disable comment "%s" '
              'that is in another disable block.' %
              (expectation_file, stripped_line))
        in_disable_block = True
        disable_block_reason = _GetDisableReasonFromComment(line)
      if FINDER_ENABLE_COMMENT in line:
        if not in_disable_block:
          raise RuntimeError(
              'Invalid expectation file %s - contains an enable comment "%s" '
              'that is outside of a disable block.' %
              (expectation_file, stripped_line))
        in_disable_block = False
      continue

    current_expectation = _CreateExpectationFromExpectationFileLine(line)

    # Add any lines containing expectations that don't match any of the given
    # expectations to remove.
    if any([e for e in expectations if e == current_expectation]):
      # Skip any expectations that match if we're in a disable block or there
      # is an inline disable comment.
      if in_disable_block:
        output_contents += line
        logging.info(
            'Would have removed expectation %s, but inside a disable block '
            'with reason %s', stripped_line, disable_block_reason)
      elif FINDER_DISABLE_COMMENT in line:
        output_contents += line
        logging.info(
            'Would have removed expectation %s, but it has an inline disable '
            'comment with reason %s',
            stripped_line.split('#')[0], _GetDisableReasonFromComment(line))
      else:
        bug = current_expectation.bug
        if bug:
          removed_urls.add(bug)
    else:
      output_contents += line

  with open(expectation_file, 'w') as f:
    f.write(output_contents)

  return removed_urls


def _IsCommentOrBlankLine(line):
  return (not line or line.startswith('#'))


def _CreateExpectationFromExpectationFileLine(line):
  """Creates a data_types.Expectation from |line|.

  Args:
    line: A string containing a single line from an expectation file.

  Returns:
    A data_types.Expectation containing the same information as |line|.
  """
  header = validate_tag_consistency.TAG_HEADER
  single_line_content = header + line
  list_parser = expectations_parser.TaggedTestListParser(single_line_content)
  assert len(list_parser.expectations) == 1
  typ_expectation = list_parser.expectations[0]
  return data_types.Expectation(typ_expectation.test, typ_expectation.tags,
                                typ_expectation.raw_results,
                                typ_expectation.reason)


def ModifySemiStaleExpectations(stale_expectation_map, expectation_file):
  """Modifies lines from |stale_expectation_map| in |expectation_file|.

  Prompts the user for each modification and provides debug information since
  semi-stale expectations cannot be blindly removed like fully stale ones.

  Args:
    stale_expectation_map: A data_types.TestExpectationMap containing stale
        expectations.
    expectation_file: A filepath pointing to an expectation file to remove lines
        from.
    file_handle: An optional open file-like object to output to. If not
        specified, stdout will be used.

  Returns:
    A set of strings containing URLs of bugs associated with the modified
    (manually modified by the user or removed by the script) expectations.
  """
  with open(expectation_file) as infile:
    file_contents = infile.read()

  expectations_to_remove = []
  expectations_to_modify = []
  for _, e, builder_map in stale_expectation_map.IterBuilderStepMaps():
    line, line_number = _GetExpectationLine(e, file_contents)
    expectation_str = None
    if not line:
      logging.error(
          'Could not find line corresponding to semi-stale expectation for %s '
          'with tags %s and expected results %s' % e.test, e.tags,
          e.expected_results)
      expectation_str = '[ %s ] %s [ %s ]' % (' '.join(
          e.tags), e.test, ' '.join(e.expected_results))
    else:
      expectation_str = '%s (approx. line %d)' % (line, line_number)

    str_dict = _ConvertBuilderMapToPassOrderedStringDict(builder_map)
    print('\nSemi-stale expectation:\n%s' % expectation_str)
    result_output._RecursivePrintToFile(str_dict, 1, sys.stdout)

    response = _WaitForUserInputOnModification()
    if response == 'r':
      expectations_to_remove.append(e)
    elif response == 'm':
      expectations_to_modify.append(e)

  # It's possible that the user will introduce a typo while manually modifying
  # an expectation, which will cause a parser error. Catch that now and give
  # them chances to fix it so that they don't lose all of their work due to an
  # early exit.
  while True:
    try:
      with open(expectation_file) as infile:
        file_contents = infile.read()
      _ = expectations_parser.TaggedTestListParser(file_contents)
      break
    except expectations_parser.ParseError as error:
      logging.error('Got parser error: %s', error)
      logging.error('This probably means you introduced a typo, please fix it.')
      _WaitForAnyUserInput()

  modified_urls = RemoveExpectationsFromFile(expectations_to_remove,
                                             expectation_file)
  for e in expectations_to_modify:
    modified_urls.add(e.bug)
  return modified_urls


def _GetExpectationLine(expectation, file_contents):
  """Gets the line and line number of |expectation| in |file_contents|.

  Args:
    expectation: A data_types.Expectation.
    file_contents: A string containing the contents read from an expectation
        file.

  Returns:
    A tuple (line, line_number). |line| is a string containing the exact line
    in |file_contents| corresponding to |expectation|. |line_number| is an int
    corresponding to where |line| is in |file_contents|. |line_number| may be
    off if the file on disk has changed since |file_contents| was read. If a
    corresponding line cannot be found, both |line| and |line_number| are None.
  """
  # We have all the information necessary to recreate the expectation line and
  # line number can be pulled during the initial expectation parsing. However,
  # the information we have is not necessarily in the same order as the
  # text file (e.g. tag ordering), and line numbers can change pretty
  # dramatically between the initial parse and now due to stale expectations
  # being removed. So, parse this way in order to improve the user experience.
  file_lines = file_contents.splitlines()
  for line_number, line in enumerate(file_lines):
    if _IsCommentOrBlankLine(line.strip()):
      continue
    current_expectation = _CreateExpectationFromExpectationFileLine(line)
    if expectation == current_expectation:
      return line, line_number + 1
  return None, None


def _ConvertBuilderMapToPassOrderedStringDict(builder_map):
  """Converts |builder_map| into an ordered dict split by pass type.

  Args:
    builder_map: A data_types.BuildStepMap.

  Returns:
    A collections.OrderedDict in the following format:
    {
      result_output.FULL_PASS: {
        builder_name: [
          step_name (total passes / total builds)
        ],
      },
      result_output.NEVER_PASS: {
        builder_name: [
          step_name (total passes / total builds)
        ],
      },
      result_output.PARTIAL_PASS: {
        builder_name: [
          step_name (total passes / total builds): [
            failure links,
          ],
        ],
      },
    }

    The ordering and presence of the top level keys is guaranteed.
  """
  # This is similar to what we do in
  # result_output._ConvertTestExpectationMapToStringDict, but we want the
  # top-level grouping to be by pass type rather than by builder, so we can't
  # re-use the code from there.
  # Ordered dict used to ensure that order is guaranteed when printing out.
  str_dict = collections.OrderedDict()
  str_dict[result_output.FULL_PASS] = {}
  str_dict[result_output.NEVER_PASS] = {}
  str_dict[result_output.PARTIAL_PASS] = {}
  for builder_name, step_name, stats in builder_map.IterBuildStats():
    step_str = result_output.AddStatsToStr(step_name, stats)
    if stats.did_fully_pass:
      str_dict[result_output.FULL_PASS].setdefault(builder_name,
                                                   []).append(step_str)
    elif stats.did_never_pass:
      str_dict[result_output.NEVER_PASS].setdefault(builder_name,
                                                    []).append(step_str)
    else:
      str_dict[result_output.PARTIAL_PASS].setdefault(
          builder_name, {})[step_str] = list(stats.failure_links)
  return str_dict


def _WaitForAnyUserInput():
  """Waits for any user input.

  Split out for testing purposes.
  """
  _get_input('Press any key to continue')


def _WaitForUserInputOnModification():
  """Waits for user input on how to modify a semi-stale expectation.

  Returns:
    One of the following string values:
      i - Expectation should be ignored and left alone.
      m - Expectation will be manually modified by the user.
      r - Expectation should be removed by the script.
  """
  valid_inputs = ['i', 'm', 'r']
  prompt = ('How should this expectation be handled? (i)gnore/(m)anually '
            'modify/(r)emove: ')
  response = _get_input(prompt).lower()
  while response not in valid_inputs:
    print('Invalid input, valid inputs are %s' % (', '.join(valid_inputs)))
    response = _get_input(prompt).lower()
  return response


def MergeExpectationMaps(base_map, merge_map, reference_map=None):
  """Merges |merge_map| into |base_map|.

  Args:
    base_map: A data_types.TestExpectationMap to be updated with the contents of
        |merge_map|. Will be modified in place.
    merge_map: A data_types.TestExpectationMap whose contents will be merged
        into |base_map|.
    reference_map: A dict containing the information that was originally in
        |base_map|. Used for ensuring that a single expectation/builder/step
        combination is only ever updated once. If None, a copy of |base_map|
        will be used.
  """
  # We only enforce that we're starting with a TestExpectationMap when this is
  # initially called, not on the recursive calls.
  if reference_map is None:
    assert isinstance(base_map, data_types.TestExpectationMap)
    assert isinstance(merge_map, data_types.TestExpectationMap)
  # We should only ever encounter a single updated BuildStats for an
  # expectation/builder/step combination. Use the reference map to determine
  # if a particular BuildStats has already been updated or not.
  reference_map = reference_map or copy.deepcopy(base_map)
  for key, value in merge_map.items():
    if key not in base_map:
      base_map[key] = value
    else:
      if isinstance(value, dict):
        MergeExpectationMaps(base_map[key], value, reference_map.get(key, {}))
      else:
        assert isinstance(value, data_types.BuildStats)
        # Ensure we haven't updated this BuildStats already. If the reference
        # map doesn't have a corresponding BuildStats, then base_map shouldn't
        # have initially either, and thus it would have been added before
        # reaching this point. Otherwise, the two values must match, meaning
        # that base_map's BuildStats hasn't been updated yet.
        reference_stats = reference_map.get(key, None)
        assert reference_stats is not None
        assert reference_stats == base_map[key]
        base_map[key] = value


def AddResultListToMap(expectation_map, builder, results):
  """Adds |results| to |expectation_map|.

  Args:
    expectation_map: A data_types.TestExpectationMap. Will be modified in-place.
    builder: A string containing the builder |results| came from. Should be
        prefixed with something to distinguish between identically named CI and
        try builders.
    results: A list of data_types.Result objects corresponding to the ResultDB
        data queried for |builder|.

  Returns:
    A list of data_types.Result objects who did not have a matching expectation
    in |expectation_map|.
  """
  assert isinstance(expectation_map, data_types.TestExpectationMap)
  failure_results = set()
  pass_results = set()
  unmatched_results = []
  for r in results:
    if r.actual_result == 'Pass':
      pass_results.add(r)
    else:
      failure_results.add(r)

  # Remove any cases of failure -> pass from the passing set. If a test is
  # flaky, we get both pass and failure results for it, so we need to remove the
  # any cases of a pass result having a corresponding, earlier failure result.
  modified_failing_retry_results = set()
  for r in failure_results:
    modified_failing_retry_results.add(
        data_types.Result(r.test, r.tags, 'Pass', r.step, r.build_id))
  pass_results -= modified_failing_retry_results

  for r in pass_results | failure_results:
    found_matching = _AddResultToMap(r, builder, expectation_map)
    if not found_matching:
      unmatched_results.append(r)

  return unmatched_results


def _AddResultToMap(result, builder, test_expectation_map):
  """Adds a single |result| to |test_expectation_map|.

  Args:
    result: A data_types.Result object to add.
    builder: A string containing the name of the builder |result| came from.
    test_expectation_map: A data_types.TestExpectationMap. Will be modified
        in-place.

  Returns:
    True if an expectation in |test_expectation_map| applied to |result|,
    otherwise False.
  """
  assert isinstance(test_expectation_map, data_types.TestExpectationMap)
  found_matching_expectation = False
  # We need to use fnmatch since wildcards are supported, so there's no point in
  # checking the test name key right now. The AppliesToResult check already does
  # an fnmatch check.
  for _, expectation, builder_map in test_expectation_map.IterBuilderStepMaps():
    if expectation.AppliesToResult(result):
      found_matching_expectation = True
      step_map = builder_map.setdefault(builder, data_types.StepBuildStatsMap())
      stats = step_map.setdefault(result.step, data_types.BuildStats())
      if result.actual_result == 'Pass':
        stats.AddPassedBuild()
      else:
        stats.AddFailedBuild(result.build_id)
  return found_matching_expectation


def _GetDisableReasonFromComment(line):
  return line.split(FINDER_DISABLE_COMMENT, 1)[1].strip()


def FindOrphanedBugs(affected_urls):
  """Finds cases where expectations for bugs no longer exist.

  Args:
    affected_urls: An iterable of affected bug URLs, as returned by functions
        such as RemoveExpectationsFromFile.

  Returns:
    A set containing a subset of |affected_urls| who no longer have any
    associated expectations in any expectation files.
  """
  seen_bugs = set()

  for expectation_file in os.listdir(EXPECTATIONS_DIR):
    if not expectation_file.endswith('_expectations.txt'):
      continue
    with open(os.path.join(EXPECTATIONS_DIR, expectation_file)) as infile:
      contents = infile.read()
    for url in affected_urls:
      if url in seen_bugs:
        continue
      if url in contents:
        seen_bugs.add(url)
  return set(affected_urls) - seen_bugs


def _get_input(prompt):
  if sys.version_info[0] == 2:
    return raw_input(prompt)
  return input(prompt)
