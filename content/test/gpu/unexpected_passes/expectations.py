# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

import logging

import validate_tag_consistency

from typ import expectations_parser
from unexpected_passes import data_types


FINDER_DISABLE_COMMENT = 'finder:disable'
FINDER_ENABLE_COMMENT = 'finder:enable'


def CreateTestExpectationMap(expectation_file, tests):
  """Creates an expectation map based off a file or list of tests.

  Args:
    expectation_file: A filepath to an expectation file to read from, or None.
        If a filepath is specified, |tests| must be None.
    tests: An iterable of strings containing test names to check. If specified,
        |expectation_file| must be None.

  Returns:
    A dict in the following format:
    {
      test_name1 (str): {
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
      test_name2 (str): { ... },
      ...
    }
    although anything beyond the the data_types.Expectation keys will be left
    empty to be filled at a later time.
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
  expectation_map = {}
  logging.debug('Parsed %d expectations', len(list_parser.expectations))
  for e in list_parser.expectations:
    if 'Skip' in e.raw_results:
      continue
    expectation = data_types.Expectation(e.test, e.tags, e.raw_results)
    expectations_for_test = expectation_map.setdefault(e.test, {})
    assert expectation not in expectations_for_test
    expectations_for_test[expectation] = {}

  return expectation_map


def FilterOutUnusedExpectations(test_expectation_map):
  """Filters out any unused Expectations from |test_expectation_map|.

  An Expectation is considered unused if its corresponding dictionary is empty.
  If removing Expectations results in a top-level test key having an empty
  dictionary, that test entry will also be removed.

  Args:
    test_expectation_map: A dict in the format returned by
        CreateTestExpectationMap(). Will be modified in place.

  Returns:
    A list containing any Expectations that were removed.
  """
  logging.info('Filtering out unused expectations')
  unused_expectations = []
  for _, expectation_map in test_expectation_map.iteritems():
    for expectation, builder_map in expectation_map.iteritems():
      if not builder_map:
        unused_expectations.append(expectation)
  for unused in unused_expectations:
    for _, expectation_map in test_expectation_map.iteritems():
      if unused in expectation_map:
        del expectation_map[unused]
  logging.debug('Found %d unused expectations', len(unused_expectations))

  empty_tests = []
  for test_name, expectation_map in test_expectation_map.iteritems():
    if not expectation_map:
      empty_tests.append(test_name)
  for empty in empty_tests:
    del test_expectation_map[empty]
  logging.debug('Found %d empty tests: %s', len(empty_tests), empty_tests)

  return unused_expectations


def SplitExpectationsByStaleness(test_expectation_map):
  """Separates |test_expectation_map| based on expectation staleness.

  Args:
    test_expectation_map: A dict in the format returned by
        CreateTestExpectationMap() with any unused expectations already filtered
        out.

  Returns:
    Three dicts (stale_dict, semi_stale_dict, active_dict). All three combined
    contain the information of |test_expectation_map| in the same format.
    |stale_dict| contains entries for expectations that are no longer being
    helpful, |semi_stale_dict| contains entries for expectations that might be
    removable or modifiable, but have at least one failed test run.
    |active_dict| contains entries for expectations that are preventing failures
    on all builders they're active on, and thus shouldn't be removed.
  """
  FULL_PASS = 1
  NEVER_PASS = 2
  PARTIAL_PASS = 3

  stale_dict = {}
  semi_stale_dict = {}
  active_dict = {}
  for test_name, expectation_map in test_expectation_map.iteritems():
    for expectation, builder_map in expectation_map.iteritems():
      # A temporary map to hold data so we can later determine whether an
      # expectation is stale, semi-stale, or active.
      tmp_map = {
          FULL_PASS: {},
          NEVER_PASS: {},
          PARTIAL_PASS: {},
      }

      for builder_name, step_map in builder_map.iteritems():
        fully_passed = {}
        partially_passed = {}
        never_passed = {}

        for step_name, stats in step_map.iteritems():
          if stats.passed_builds == stats.total_builds:
            assert step_name not in fully_passed
            fully_passed[step_name] = stats
          elif stats.failed_builds == stats.total_builds:
            assert step_name not in never_passed
            never_passed[step_name] = stats
          else:
            assert step_name not in partially_passed
            partially_passed[step_name] = stats

        if fully_passed:
          tmp_map[FULL_PASS][builder_name] = fully_passed
        if never_passed:
          tmp_map[NEVER_PASS][builder_name] = never_passed
        if partially_passed:
          tmp_map[PARTIAL_PASS][builder_name] = partially_passed

      def _CopyPassesIntoBuilderMap(builder_map, pass_types):
        for pt in pass_types:
          for builder, steps in tmp_map[pt].iteritems():
            builder_map.setdefault(builder, {}).update(steps)

      # Handle the case of a stale expectation.
      if not (tmp_map[NEVER_PASS] or tmp_map[PARTIAL_PASS]):
        builder_map = stale_dict.setdefault(test_name,
                                            {}).setdefault(expectation, {})
        _CopyPassesIntoBuilderMap(builder_map, [FULL_PASS])
      # Handle the case of an active expectation.
      elif not tmp_map[FULL_PASS]:
        builder_map = active_dict.setdefault(test_name,
                                             {}).setdefault(expectation, {})
        _CopyPassesIntoBuilderMap(builder_map, [NEVER_PASS, PARTIAL_PASS])
      # Handle the case of a semi-stale expectation.
      else:
        # TODO(crbug.com/998329): Sort by pass percentage so it's easier to find
        # problematic builders without highlighting.
        builder_map = semi_stale_dict.setdefault(test_name, {}).setdefault(
            expectation, {})
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
  header = validate_tag_consistency.TAG_HEADER

  with open(expectation_file) as f:
    input_contents = f.read()

  output_contents = ''
  in_disable_block = False
  disable_block_reason = ''
  removed_urls = set()
  for line in input_contents.splitlines(True):
    # Auto-add any comments or empty lines
    stripped_line = line.strip()
    if not stripped_line or stripped_line.startswith('#'):
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

    single_line_content = header + line
    list_parser = expectations_parser.TaggedTestListParser(single_line_content)
    assert len(list_parser.expectations) == 1

    typ_expectation = list_parser.expectations[0]
    current_expectation = data_types.Expectation(typ_expectation.test,
                                                 typ_expectation.tags,
                                                 typ_expectation.raw_results)

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
        reason = list_parser.expectations[0].reason
        if reason:
          removed_urls.add(reason)
    else:
      output_contents += line

  with open(expectation_file, 'w') as f:
    f.write(output_contents)

  return removed_urls


def _GetDisableReasonFromComment(line):
  return line.split(FINDER_DISABLE_COMMENT, 1)[1].strip()
