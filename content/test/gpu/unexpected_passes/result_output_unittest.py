# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import tempfile
import unittest

from pyfakefs import fake_filesystem_unittest

from unexpected_passes import data_types
from unexpected_passes import result_output


def CreateTextOutputPermutations(text, inputs):
  """Creates permutations of |text| filled with the contents of |inputs|.

  Some output ordering is not guaranteed, so this acts as a way to generate
  all possible outputs instead of manually listing them.

  Args:
    text: A string containing a single string field to format.
    inputs: An iterable of strings to permute.

  Returns:
    A set of unique permutations of |text| filled with |inputs|. E.g. if |text|
    is '1%s2' and |inputs| is ['a', 'b'], the return value will be
    set(['1ab2', '1ba2']).
  """
  permutations = set()
  for p in itertools.permutations(inputs):
    permutations.add(text % ''.join(p))
  return permutations


class ConvertUnmatchedResultsToStringDictUnittest(unittest.TestCase):
  def testEmptyResults(self):
    """Tests that providing empty results is a no-op."""
    self.assertEqual(result_output._ConvertUnmatchedResultsToStringDict({}), {})

  def testMinimalData(self):
    """Tests that everything functions when minimal data is provided."""
    unmatched_results = {
        'builder': [
            data_types.Result('foo', [], 'Failure', None, 'build_id'),
        ],
    }
    expected_output = {
        'foo': {
            'builder': {
                None: [
                    'Got "Failure" on http://ci.chromium.org/b/build_id with '
                    'tags []',
                ],
            },
        },
    }
    output = result_output._ConvertUnmatchedResultsToStringDict(
        unmatched_results)
    self.assertEqual(output, expected_output)

  def testRegularData(self):
    """Tests that everything functions when regular data is provided."""
    unmatched_results = {
        'builder': [
            data_types.Result('foo', ['win', 'intel'], 'Failure', 'step_name',
                              'build_id')
        ],
    }
    expected_output = {
        'foo': {
            'builder': {
                'step_name': [
                    'Got "Failure" on http://ci.chromium.org/b/build_id with '
                    'tags [win intel]',
                ]
            }
        }
    }
    output = result_output._ConvertUnmatchedResultsToStringDict(
        unmatched_results)
    self.assertEqual(output, expected_output)


class ConvertTestExpectationMapToStringDictsUnittest(unittest.TestCase):
  def testEmptyMap(self):
    """Tests that providing an empty map is a no-op."""
    self.assertEqual(result_output._ConvertTestExpectationMapToStringDicts({}),
                     ({}, {}, {}))

  def testSemiStaleMap(self):
    """Tests that everything functions when regular data is provided."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['win', 'intel'], ['RetryOnFailure']):
            {
                'builder': {
                    'all_pass': _CreateStatsWithPassFails(2, 0),
                    'all_fail': _CreateStatsWithPassFails(0, 2),
                    'some_pass': _CreateStatsWithPassFails(1, 1),
                },
            },
            data_types.Expectation('foo', ['linux', 'intel'], [
                                       'RetryOnFailure'
                                   ]): {
                'builder': {
                    'all_pass': _CreateStatsWithPassFails(2, 0),
                }
            },
            data_types.Expectation('foo', ['mac', 'intel'], ['RetryOnFailure']):
            {
                'builder': {
                    'all_fail': _CreateStatsWithPassFails(0, 2),
                }
            },
        },
    }
    expected_semi_stale = {
        'foo': {
            '"RetryOnFailure" expectation on "win intel"': {
                'builder': {
                    'Fully passed in the following': [
                        'all_pass (2/2)',
                    ],
                    'Never passed in the following': [
                        'all_fail (0/2)',
                    ],
                    'Partially passed in the following': {
                        'some_pass (1/2)': [
                            data_types.BuildLinkFromBuildId('build_id0'),
                        ],
                    },
                },
            },
        },
    }
    expected_stale = {
        'foo': {
            '"RetryOnFailure" expectation on "intel linux"': {
                'builder': {
                    'Fully passed in the following': [
                        'all_pass (2/2)',
                    ],
                },
            },
        },
    }
    expected_active = {
        'foo': {
            '"RetryOnFailure" expectation on "mac intel"': {
                'builder': {
                    'Never passed in the following': [
                        'all_fail (0/2)',
                    ],
                },
            },
        },
    }

    stale_dict, semi_stale_dict, active_dict =\
        result_output._ConvertTestExpectationMapToStringDicts(
        expectation_map)
    self.assertEqual(semi_stale_dict, expected_semi_stale)
    self.assertEqual(stale_dict, expected_stale)
    self.assertEqual(active_dict, expected_active)


class HtmlToFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False)
    self._filepath = self._file_handle.name

  def testLinkifyString(self):
    """Test for _LinkifyString()."""
    self._file_handle.close()
    s = 'a'
    self.assertEqual(result_output._LinkifyString(s), 'a')
    s = 'http://a'
    self.assertEqual(result_output._LinkifyString(s),
                     '<a href="http://a">http://a</a>')
    s = 'link to http://a, click it'
    self.assertEqual(result_output._LinkifyString(s),
                     'link to <a href="http://a">http://a</a>, click it')

  def testRecursiveHtmlToFileExpectationMap(self):
    """Tests _RecursiveHtmlToFile() with an expectation map as input."""
    expectation_map = {
        'foo': {
            '"RetryOnFailure" expectation on "win intel"': {
                'builder': {
                    'Fully passed in the following': [
                        'all_pass (2/2)',
                    ],
                    'Never passed in the following': [
                        'all_fail (0/2)',
                    ],
                    'Partially passed in the following': {
                        'some_pass (1/2)': [
                            data_types.BuildLinkFromBuildId('build_id0'),
                        ],
                    },
                },
            },
        },
    }
    result_output._RecursiveHtmlToFile(expectation_map, self._file_handle)
    self._file_handle.close()
    # pylint: disable=line-too-long
    expected_output = """\
<button type="button" class="collapsible_group">foo</button>
<div class="content">
  <button type="button" class="collapsible_group">"RetryOnFailure" expectation on "win intel"</button>
  <div class="content">
    <button type="button" class="collapsible_group">builder</button>
    <div class="content">
      <button type="button" class="collapsible_group">Never passed in the following</button>
      <div class="content">
        <p>all_fail (0/2)</p>
      </div>
      <button type="button" class="highlighted_collapsible_group">Fully passed in the following</button>
      <div class="content">
        <p>all_pass (2/2)</p>
      </div>
      <button type="button" class="collapsible_group">Partially passed in the following</button>
      <div class="content">
        <button type="button" class="collapsible_group">some_pass (1/2)</button>
        <div class="content">
          <p><a href="http://ci.chromium.org/b/build_id0">http://ci.chromium.org/b/build_id0</a></p>
        </div>
      </div>
    </div>
  </div>
</div>
"""
    # pylint: enable=line-too-long
    expected_output = _Dedent(expected_output)
    with open(self._filepath) as f:
      self.assertEqual(f.read(), expected_output)

  def testRecursiveHtmlToFileUnmatchedResults(self):
    """Tests _RecursiveHtmlToFile() with unmatched results as input."""
    unmatched_results = {
        'foo': {
            'builder': {
                None: [
                    'Expected "" on http://ci.chromium.org/b/build_id, got '
                    '"Failure" with tags []',
                ],
                'step_name': [
                    'Expected "Failure RetryOnFailure" on '
                    'http://ci.chromium.org/b/build_id, got '
                    '"Failure" with tags [win intel]',
                ]
            },
        },
    }
    result_output._RecursiveHtmlToFile(unmatched_results, self._file_handle)
    self._file_handle.close()
    # pylint: disable=line-too-long
    # Order is not guaranteed, so create permutations.
    expected_template = """\
<button type="button" class="collapsible_group">foo</button>
<div class="content">
  <button type="button" class="collapsible_group">builder</button>
  <div class="content">
    %s
  </div>
</div>
"""
    values = [
        """\
    <button type="button" class="collapsible_group">None</button>
    <div class="content">
      <p>Expected "" on <a href="http://ci.chromium.org/b/build_id">http://ci.chromium.org/b/build_id</a>, got "Failure" with tags []</p>
    </div>
""",
        """\
    <button type="button" class="collapsible_group">step_name</button>
    <div class="content">
      <p>Expected "Failure RetryOnFailure" on <a href="http://ci.chromium.org/b/build_id">http://ci.chromium.org/b/build_id</a>, got "Failure" with tags [win intel]</p>
    </div>
""",
    ]
    expected_output = CreateTextOutputPermutations(expected_template, values)
    # pylint: enable=line-too-long
    expected_output = [_Dedent(e) for e in expected_output]
    with open(self._filepath) as f:
      self.assertIn(f.read(), expected_output)


class PrintToFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False)
    self._filepath = self._file_handle.name

  def testRecursivePrintToFileExpectationMap(self):
    """Tests _RecursivePrintToFile() with an expectation map as input."""
    expectation_map = {
        'foo': {
            '"RetryOnFailure" expectation on "win intel"': {
                'builder': {
                    'Fully passed in the following': [
                        'all_pass (2/2)',
                    ],
                    'Never passed in the following': [
                        'all_fail (0/2)',
                    ],
                    'Partially passed in the following': {
                        'some_pass (1/2)': [
                            data_types.BuildLinkFromBuildId('build_id0'),
                        ],
                    },
                },
            },
        },
    }
    result_output._RecursivePrintToFile(expectation_map, 0, self._file_handle)
    self._file_handle.close()
    expected_output = """\
foo
  "RetryOnFailure" expectation on "win intel"
    builder
      Never passed in the following
        all_fail (0/2)
      Fully passed in the following
        all_pass (2/2)
      Partially passed in the following
        some_pass (1/2)
          http://ci.chromium.org/b/build_id0
"""
    with open(self._filepath) as f:
      self.assertEqual(f.read(), expected_output)

  def testRecursivePrintToFileUnmatchedResults(self):
    """Tests _RecursivePrintToFile() with unmatched results as input."""
    unmatched_results = {
        'foo': {
            'builder': {
                None: [
                    'Expected "" on http://ci.chromium.org/b/build_id, got '
                    '"Failure" with tags []',
                ],
                'step_name': [
                    'Expected "Failure RetryOnFailure" on '
                    'http://ci.chromium.org/b/build_id, got '
                    '"Failure" with tags [win intel]',
                ]
            },
        },
    }
    result_output._RecursivePrintToFile(unmatched_results, 0, self._file_handle)
    self._file_handle.close()
    # pylint: disable=line-too-long
    # Order is not guaranteed, so create permutations.
    expected_template = """\
foo
  builder%s
"""
    values = [
        """
    None
      Expected "" on http://ci.chromium.org/b/build_id, got "Failure" with tags []\
""",
        """
    step_name
      Expected "Failure RetryOnFailure" on http://ci.chromium.org/b/build_id, got "Failure" with tags [win intel]\
""",
    ]
    expected_output = CreateTextOutputPermutations(expected_template, values)
    # pylint: enable=line-too-long
    with open(self._filepath) as f:
      self.assertIn(f.read(), expected_output)


class OutputResultsUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False)
    self._filepath = self._file_handle.name

  def testOutputResultsUnsupportedFormat(self):
    """Tests that passing in an unsupported format is an error."""
    with self.assertRaises(RuntimeError):
      result_output.OutputResults({}, {}, [], 'asdf')

  def testOutputResultsSmoketest(self):
    """Test that nothing blows up when outputting."""
    expectation_map = {
        'foo': {
            data_types.Expectation('foo', ['win', 'intel'], 'RetryOnFailure'): {
                'builder': {
                    'all_pass': _CreateStatsWithPassFails(2, 0),
                },
            },
        },
    }
    unmatched_results = {
        'builder': [
            data_types.Result('foo', ['win', 'intel'], 'Failure', 'step_name',
                              'build_id'),
        ],
    }
    unmatched_expectations = [
        data_types.Expectation('foo', ['linux'], 'RetryOnFailure')
    ]

    result_output.OutputResults(expectation_map, {}, [], 'print',
                                self._file_handle)
    result_output.OutputResults(expectation_map, unmatched_results, [], 'print',
                                self._file_handle)
    result_output.OutputResults(expectation_map, {}, unmatched_expectations,
                                'print', self._file_handle)
    result_output.OutputResults(expectation_map, unmatched_results,
                                unmatched_expectations, 'print',
                                self._file_handle)

    result_output.OutputResults(expectation_map, {}, [], 'html',
                                self._file_handle)
    result_output.OutputResults(expectation_map, unmatched_results, [], 'html',
                                self._file_handle)
    result_output.OutputResults(expectation_map, {}, unmatched_expectations,
                                'html', self._file_handle)
    result_output.OutputResults(expectation_map, unmatched_results,
                                unmatched_expectations, 'html',
                                self._file_handle)


def _CreateStatsWithPassFails(passes, fails):
  stats = data_types.BuildStats()
  for _ in xrange(passes):
    stats.AddPassedBuild()
  for i in xrange(fails):
    stats.AddFailedBuild('build_id%d' % i)
  return stats


def _Dedent(s):
  output = ''
  for line in s.splitlines(True):
    output += line.lstrip()
  return output


if __name__ == '__main__':
  unittest.main(verbosity=2)
