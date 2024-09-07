#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import tempfile
from typing import Iterable, Set
import unittest
from unittest import mock

import six

from pyfakefs import fake_filesystem_unittest

from unexpected_passes_common import data_types
from unexpected_passes_common import result_output
from unexpected_passes_common import unittest_utils as uu

from blinkpy.w3c import buganizer

# Protected access is allowed for unittests.
# pylint: disable=protected-access

def CreateTextOutputPermutations(text: str, inputs: Iterable[str]) -> Set[str]:
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
  def testEmptyResults(self) -> None:
    """Tests that providing empty results is a no-op."""
    self.assertEqual(result_output._ConvertUnmatchedResultsToStringDict({}), {})

  def testMinimalData(self) -> None:
    """Tests that everything functions when minimal data is provided."""
    unmatched_results = {
        'builder': [
            data_types.Result('foo', [], 'Failure', 'step', 'build_id'),
        ],
    }
    expected_output = {
        'foo': {
            'builder': {
                'step': [
                    'Got "Failure" on http://ci.chromium.org/b/build_id with '
                    'tags []',
                ],
            },
        },
    }
    output = result_output._ConvertUnmatchedResultsToStringDict(
        unmatched_results)
    self.assertEqual(output, expected_output)

  def testRegularData(self) -> None:
    """Tests that everything functions when regular data is provided."""
    unmatched_results = {
        'builder': [
            data_types.Result('foo', ['win', 'intel'], 'Failure', 'step_name',
                              'build_id')
        ],
    }
    # TODO(crbug.com/40177248): Hard-code the tag string once only Python 3 is
    # supported.
    expected_output = {
        'foo': {
            'builder': {
                'step_name': [
                    'Got "Failure" on http://ci.chromium.org/b/build_id with '
                    'tags [%s]' % ' '.join(set(['win', 'intel'])),
                ]
            }
        }
    }
    output = result_output._ConvertUnmatchedResultsToStringDict(
        unmatched_results)
    self.assertEqual(output, expected_output)


class ConvertTestExpectationMapToStringDictUnittest(unittest.TestCase):
  def testEmptyMap(self) -> None:
    """Tests that providing an empty map is a no-op."""
    self.assertEqual(
        result_output._ConvertTestExpectationMapToStringDict(
            data_types.TestExpectationMap()), {})

  def testSemiStaleMap(self) -> None:
    """Tests that everything functions when regular data is provided."""
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win', 'intel'], [
                                       'RetryOnFailure'
                                   ]):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'all_pass':
                    uu.CreateStatsWithPassFails(2, 0),
                    'all_fail':
                    uu.CreateStatsWithPassFails(0, 2),
                    'some_pass':
                    uu.CreateStatsWithPassFails(1, 1),
                }),
            }),
            data_types.Expectation('foo/test', ['linux', 'intel'], [
                                       'RetryOnFailure'
                                   ]):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'all_pass':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
            }),
            data_types.Expectation('foo/test', ['mac', 'intel'], [
                                       'RetryOnFailure'
                                   ]):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'all_fail':
                    uu.CreateStatsWithPassFails(0, 2),
                }),
            }),
        }),
    })
    # TODO(crbug.com/40177248): Remove the Python 2 version once we are fully
    # switched to Python 3.
    if six.PY2:
      expected_output = {
          'expectation_file': {
              'foo/test': {
                  '"RetryOnFailure" expectation on "win intel"': {
                      'builder': {
                          'Fully passed in the following': [
                              'all_pass (2/2 passed)',
                          ],
                          'Never passed in the following': [
                              'all_fail (0/2 passed)',
                          ],
                          'Partially passed in the following': {
                              'some_pass (1/2 passed)': [
                                  data_types.BuildLinkFromBuildId('build_id0'),
                              ],
                          },
                      },
                  },
                  '"RetryOnFailure" expectation on "intel linux"': {
                      'builder': {
                          'Fully passed in the following': [
                              'all_pass (2/2 passed)',
                          ],
                      },
                  },
                  '"RetryOnFailure" expectation on "mac intel"': {
                      'builder': {
                          'Never passed in the following': [
                              'all_fail (0/2 passed)',
                          ],
                      },
                  },
              },
          },
      }
    else:
      # Set ordering does not appear to be stable between test runs, as we can
      # get either order of tags. So, generate them now instead of hard coding
      # them.
      linux_tags = ' '.join(set(['linux', 'intel']))
      win_tags = ' '.join(set(['win', 'intel']))
      mac_tags = ' '.join(set(['mac', 'intel']))
      expected_output = {
          'expectation_file': {
              'foo/test': {
                  '"RetryOnFailure" expectation on "%s"' % linux_tags: {
                      'builder': {
                          'Fully passed in the following': [
                              'all_pass (2/2 passed)',
                          ],
                      },
                  },
                  '"RetryOnFailure" expectation on "%s"' % win_tags: {
                      'builder': {
                          'Fully passed in the following': [
                              'all_pass (2/2 passed)',
                          ],
                          'Partially passed in the following': {
                              'some_pass (1/2 passed)': [
                                  data_types.BuildLinkFromBuildId('build_id0'),
                              ],
                          },
                          'Never passed in the following': [
                              'all_fail (0/2 passed)',
                          ],
                      },
                  },
                  '"RetryOnFailure" expectation on "%s"' % mac_tags: {
                      'builder': {
                          'Never passed in the following': [
                              'all_fail (0/2 passed)',
                          ],
                      },
                  },
              },
          },
      }

    str_dict = result_output._ConvertTestExpectationMapToStringDict(
        expectation_map)
    self.assertEqual(str_dict, expected_output)


class ConvertUnusedExpectationsToStringDictUnittest(unittest.TestCase):
  def testEmptyDict(self) -> None:
    """Tests that nothing blows up when given an empty dict."""
    self.assertEqual(result_output._ConvertUnusedExpectationsToStringDict({}),
                     {})

  def testBasic(self) -> None:
    """Basic functionality test."""
    unused = {
        'foo_file': [
            data_types.Expectation('foo/test', ['win', 'nvidia'],
                                   ['Failure', 'Timeout']),
        ],
        'bar_file': [
            data_types.Expectation('bar/test', ['win'], ['Failure']),
            data_types.Expectation('bar/test2', ['win'], ['RetryOnFailure'])
        ],
    }
    if six.PY2:
      expected_output = {
          'foo_file': [
              '[ win nvidia ] foo/test [ Failure Timeout ]',
          ],
          'bar_file': [
              '[ win ] bar/test [ Failure ]',
              '[ win ] bar/test2 [ RetryOnFailure ]',
          ],
      }
    else:
      # Set ordering does not appear to be stable between test runs, as we can
      # get either order of tags. So, generate them now instead of hard coding
      # them.
      tags = ' '.join(['nvidia', 'win'])
      results = ' '.join(['Failure', 'Timeout'])
      expected_output = {
          'foo_file': [
              '[ %s ] foo/test [ %s ]' % (tags, results),
          ],
          'bar_file': [
              '[ win ] bar/test [ Failure ]',
              '[ win ] bar/test2 [ RetryOnFailure ]',
          ],
      }
    self.assertEqual(
        result_output._ConvertUnusedExpectationsToStringDict(unused),
        expected_output)


class HtmlToFileUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testLinkifyString(self) -> None:
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

  def testRecursiveHtmlToFileExpectationMap(self) -> None:
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
    # TODO(crbug.com/40177248): Remove the Python 2 version once we've fully
    # switched to Python 3.
    if six.PY2:
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
    else:
      expected_output = """\
<button type="button" class="collapsible_group">foo</button>
<div class="content">
  <button type="button" class="collapsible_group">"RetryOnFailure" expectation on "win intel"</button>
  <div class="content">
    <button type="button" class="collapsible_group">builder</button>
    <div class="content">
      <button type="button" class="highlighted_collapsible_group">Fully passed in the following</button>
      <div class="content">
        <p>all_pass (2/2)</p>
      </div>
      <button type="button" class="collapsible_group">Never passed in the following</button>
      <div class="content">
        <p>all_fail (0/2)</p>
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

  def testRecursiveHtmlToFileUnmatchedResults(self) -> None:
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
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testRecursivePrintToFileExpectationMap(self) -> None:
    """Tests RecursivePrintToFile() with an expectation map as input."""
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
    result_output.RecursivePrintToFile(expectation_map, 0, self._file_handle)
    self._file_handle.close()

    # TODO(crbug.com/40177248): Keep the Python 3 version once we are fully
    # switched.
    if six.PY2:
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
    else:
      expected_output = """\
foo
  "RetryOnFailure" expectation on "win intel"
    builder
      Fully passed in the following
        all_pass (2/2)
      Never passed in the following
        all_fail (0/2)
      Partially passed in the following
        some_pass (1/2)
          http://ci.chromium.org/b/build_id0
"""
    with open(self._filepath) as f:
      self.assertEqual(f.read(), expected_output)

  def testRecursivePrintToFileUnmatchedResults(self) -> None:
    """Tests RecursivePrintToFile() with unmatched results as input."""
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
    result_output.RecursivePrintToFile(unmatched_results, 0, self._file_handle)
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
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testOutputResultsUnsupportedFormat(self) -> None:
    """Tests that passing in an unsupported format is an error."""
    with self.assertRaises(RuntimeError):
      result_output.OutputResults(data_types.TestExpectationMap(),
                                  data_types.TestExpectationMap(),
                                  data_types.TestExpectationMap(), {}, {},
                                  'asdf')

  def testOutputResultsSmoketest(self) -> None:
    """Test that nothing blows up when outputting."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win', 'intel'], 'RetryOnFailure'):
            data_types.BuilderStepMap({
                'stale':
                data_types.StepBuildStatsMap({
                    'all_pass':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
            }),
            data_types.Expectation('foo', ['linux'], 'Failure'):
            data_types.BuilderStepMap({
                'semi_stale':
                data_types.StepBuildStatsMap({
                    'all_pass':
                    uu.CreateStatsWithPassFails(2, 0),
                    'some_pass':
                    uu.CreateStatsWithPassFails(1, 1),
                    'none_pass':
                    uu.CreateStatsWithPassFails(0, 2),
                }),
            }),
            data_types.Expectation('foo', ['mac'], 'Failure'):
            data_types.BuilderStepMap({
                'active':
                data_types.StepBuildStatsMap({
                    'none_pass':
                    uu.CreateStatsWithPassFails(0, 2),
                }),
            }),
        }),
    })
    unmatched_results = {
        'builder': [
            data_types.Result('foo', ['win', 'intel'], 'Failure', 'step_name',
                              'build_id'),
        ],
    }
    unmatched_expectations = {
        'foo_file': [
            data_types.Expectation('foo', ['linux'], 'RetryOnFailure'),
        ],
    }

    stale, semi_stale, active = expectation_map.SplitByStaleness()

    result_output.OutputResults(stale, semi_stale, active, {}, {}, 'print',
                                self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, unmatched_results,
                                {}, 'print', self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, {},
                                unmatched_expectations, 'print',
                                self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, unmatched_results,
                                unmatched_expectations, 'print',
                                self._file_handle)

    result_output.OutputResults(stale, semi_stale, active, {}, {}, 'html',
                                self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, unmatched_results,
                                {}, 'html', self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, {},
                                unmatched_expectations, 'html',
                                self._file_handle)
    result_output.OutputResults(stale, semi_stale, active, unmatched_results,
                                unmatched_expectations, 'html',
                                self._file_handle)


class OutputAffectedUrlsUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testOutput(self) -> None:
    """Tests that the output is correct."""
    urls = [
        'https://crbug.com/1234',
        'https://crbug.com/angleproject/1234',
        'http://crbug.com/2345',
        'crbug.com/3456',
    ]
    orphaned_urls = ['https://crbug.com/1234', 'crbug.com/3456']
    result_output._OutputAffectedUrls(urls, orphaned_urls, self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs: '
                                  'https://crbug.com/1234 '
                                  'https://crbug.com/angleproject/1234 '
                                  'http://crbug.com/2345 '
                                  'https://crbug.com/3456\n'
                                  'Closable bugs: '
                                  'https://crbug.com/1234 '
                                  'https://crbug.com/3456\n'))


class OutputUrlsForClDescriptionUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self) -> None:
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testSingleLine(self) -> None:
    """Tests when all bugs can fit on a single line."""
    urls = [
        'crbug.com/1234',
        'https://crbug.com/angleproject/2345',
    ]
    result_output._OutputUrlsForClDescription(urls, [], self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 1234, angleproject:2345\n'))

  def testBugLimit(self) -> None:
    """Tests that only a certain number of bugs are allowed per line."""
    urls = [
        'crbug.com/1',
        'crbug.com/2',
        'crbug.com/3',
        'crbug.com/4',
        'crbug.com/5',
        'crbug.com/6',
    ]
    result_output._OutputUrlsForClDescription(urls, [], self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 1, 2, 3, 4, 5\n'
                                  'Bug: 6\n'))

  def testLengthLimit(self) -> None:
    """Tests that only a certain number of characters are allowed per line."""
    urls = [
        'crbug.com/averylongprojectthatwillgooverthelinelength/1',
        'crbug.com/averylongprojectthatwillgooverthelinelength/2',
    ]
    result_output._OutputUrlsForClDescription(urls, [], self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(),
                       ('Affected bugs for CL description:\n'
                        'Bug: averylongprojectthatwillgooverthelinelength:1\n'
                        'Bug: averylongprojectthatwillgooverthelinelength:2\n'))

    project_name = (result_output.MAX_CHARACTERS_PER_CL_LINE - len('Bug: ') -
                    len(':1, 2')) * 'a'
    urls = [
        'crbug.com/%s/1' % project_name,
        'crbug.com/2',
    ]
    with open(self._filepath, 'w') as f:
      result_output._OutputUrlsForClDescription(urls, [], f)
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 2, %s:1\n' % project_name))

    project_name += 'a'
    urls = [
        'crbug.com/%s/1' % project_name,
        'crbug.com/2',
    ]
    with open(self._filepath, 'w') as f:
      result_output._OutputUrlsForClDescription(urls, [], f)
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 2\nBug: %s:1\n' % project_name))

  def testSingleBugOverLineLimit(self) -> None:
    """Tests the behavior when a single bug by itself is over the line limit."""
    project_name = result_output.MAX_CHARACTERS_PER_CL_LINE * 'a'
    urls = [
        'crbug.com/%s/1' % project_name,
        'crbug.com/2',
    ]
    result_output._OutputUrlsForClDescription(urls, [], self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 2\n'
                                  'Bug: %s:1\n' % project_name))

  def testOrphanedBugs(self) -> None:
    """Tests that orphaned bugs are output properly alongside affected ones."""
    urls = [
        'crbug.com/1',
        'crbug.com/2',
        'crbug.com/3',
    ]
    orphaned_urls = ['crbug.com/2']
    result_output._OutputUrlsForClDescription(urls, orphaned_urls,
                                              self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 1, 3\n'
                                  'Fixed: 2\n'))

  def testOnlyOrphanedBugs(self) -> None:
    """Tests output when all affected bugs are orphaned bugs."""
    urls = [
        'crbug.com/1',
        'crbug.com/2',
    ]
    orphaned_urls = [
        'crbug.com/1',
        'crbug.com/2',
    ]
    result_output._OutputUrlsForClDescription(urls, orphaned_urls,
                                              self._file_handle)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Fixed: 1, 2\n'))

  def testNoAutoCloseBugs(self):
    """Tests behavior when not auto closing bugs."""
    urls = [
        'crbug.com/0',
        'crbug.com/1',
    ]
    orphaned_urls = [
        'crbug.com/0',
    ]
    mock_buganizer = MockBuganizerClient()
    with mock.patch.object(result_output,
                           '_GetBuganizerClient',
                           return_value=mock_buganizer):
      result_output._OutputUrlsForClDescription(urls,
                                                orphaned_urls,
                                                self._file_handle,
                                                auto_close_bugs=False)
    self._file_handle.close()
    with open(self._filepath) as f:
      self.assertEqual(f.read(), ('Affected bugs for CL description:\n'
                                  'Bug: 1\n'
                                  'Bug: 0\n'))
    mock_buganizer.NewComment.assert_called_once_with(
        'crbug.com/0', result_output.BUGANIZER_COMMENT)


class MockBuganizerClient:

  def __init__(self):
    self.comment_list = []
    self.NewComment = mock.Mock()

  def GetIssueComments(self, _) -> list:
    return self.comment_list


class PostCommentsToOrphanedBugsUnittest(unittest.TestCase):

  def setUp(self):
    self._buganizer_client = MockBuganizerClient()
    self._buganizer_patcher = mock.patch.object(
        result_output,
        '_GetBuganizerClient',
        return_value=self._buganizer_client)
    self._buganizer_patcher.start()
    self.addCleanup(self._buganizer_patcher.stop)

  def testBasic(self):
    """Tests the basic/happy path scenario."""
    self._buganizer_client.comment_list.append({'comment': 'Not matching'})
    result_output._PostCommentsToOrphanedBugs(
        ['crbug.com/0', 'crbug.com/angleproject/0'])
    self.assertEqual(self._buganizer_client.NewComment.call_count, 2)
    self._buganizer_client.NewComment.assert_any_call(
        'crbug.com/0', result_output.BUGANIZER_COMMENT)
    self._buganizer_client.NewComment.assert_any_call(
        'crbug.com/angleproject/0', result_output.BUGANIZER_COMMENT)

  def testNoDuplicateComments(self):
    """Tests that duplicate comments are not posted on bugs."""
    self._buganizer_client.comment_list.append(
        {'comment': result_output.BUGANIZER_COMMENT})
    result_output._PostCommentsToOrphanedBugs(
        ['crbug.com/0', 'crbug.com/angleproject/0'])
    self._buganizer_client.NewComment.assert_not_called()

  def testInvalidBugUrl(self):
    """Tests behavior when a non-crbug URL is provided."""
    with mock.patch.object(self._buganizer_client,
                           'GetIssueComments',
                           side_effect=buganizer.BuganizerError):
      with self.assertLogs(level='WARNING') as log_manager:
        result_output._PostCommentsToOrphanedBugs(['somesite.com/0'])
        for message in log_manager.output:
          if 'Could not fetch or add comments for somesite.com/0' in message:
            break
        else:
          self.fail('Did not find expected log message')
    self._buganizer_client.NewComment.assert_not_called()

  def testServiceDiscoveryError(self):
    """Tests behavior when service discovery fails."""
    with mock.patch.object(result_output,
                           '_GetBuganizerClient',
                           side_effect=buganizer.BuganizerError):
      with self.assertLogs(level='ERROR') as log_manager:
        result_output._PostCommentsToOrphanedBugs(['crbug.com/0'])
        for message in log_manager.output:
          if ('Encountered error when authenticating, cannot post '
              'comments') in message:
            break
        else:
          self.fail('Did not find expected log message')

  def testGetIssueCommentsError(self):
    """Tests behavior when GetIssueComments encounters an error."""
    with mock.patch.object(self._buganizer_client,
                           'GetIssueComments',
                           side_effect=({
                               'error': ':('
                           }, [{
                               'comment': 'Not matching'
                           }])):
      with self.assertLogs(level='ERROR') as log_manager:
        result_output._PostCommentsToOrphanedBugs(
            ['crbug.com/0', 'crbug.com/1'])
        for message in log_manager.output:
          if 'Failed to get comments from crbug.com/0: :(' in message:
            break
        else:
          self.fail('Did not find expected log message')
    self._buganizer_client.NewComment.assert_called_once_with(
        'crbug.com/1', result_output.BUGANIZER_COMMENT)

  def testGetIssueCommentsUnspecifiedError(self):
    """Tests behavior when GetIssueComments encounters an unspecified error."""
    with mock.patch.object(self._buganizer_client,
                           'GetIssueComments',
                           side_effect=({}, [{
                               'comment': 'Not matching'
                           }])):
      with self.assertLogs(level='ERROR') as log_manager:
        result_output._PostCommentsToOrphanedBugs(
            ['crbug.com/0', 'crbug.com/1'])
        for message in log_manager.output:
          if ('Failed to get comments from crbug.com/0: error not provided'
              in message):
            break
        else:
          self.fail('Did not find expected log message')
    self._buganizer_client.NewComment.assert_called_once_with(
        'crbug.com/1', result_output.BUGANIZER_COMMENT)


def _Dedent(s: str) -> str:
  output = ''
  for line in s.splitlines(True):
    output += line.lstrip()
  return output


if __name__ == '__main__':
  unittest.main(verbosity=2)
