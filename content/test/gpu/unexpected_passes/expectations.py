# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Methods related to test expectations/expectation files."""

import logging

from typ import expectations_parser
from unexpected_passes import data_types


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
