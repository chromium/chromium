#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import itertools
import sys
import tempfile
import unittest

import six

from pyfakefs import fake_filesystem_unittest

from unexpected_passes_common import data_types
from unexpected_passes_common import result_output
from unexpected_passes_common import unittest_utils as uu


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
    # TODO(crbug.com/1198237): Hard-code the tag string once only Python 3 is
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
  def testEmptyMap(self):
    """Tests that providing an empty map is a no-op."""
    self.assertEqual(
        result_output._ConvertTestExpectationMapToStringDict(
            data_types.TestExpectationMap()), {})

  def testSemiStaleMap(self):
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
    # TODO(crbug.com/1198237): Remove the Python 2 version once we are fully
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
  def testEmptyDict(self):
    """Tests that nothing blows up when given an empty dict."""
    self.assertEqual(result_output._ConvertUnusedExpectationsToStringDict({}),
                     {})

  def testBasic(self):
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
      tags = ' '.join(set(['win', 'nvidia']))
      results = ' '.join(set(['Failure', 'Timeout']))
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
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
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
    # TODO(crbug.com/1198237): Remove the Python 2 version once we've fully
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
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testRecursivePrintToFileExpectationMap(self):
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

    # TODO(crbug.com/1198237): Keep the Python 3 version once we are fully
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

  def testRecursivePrintToFileUnmatchedResults(self):
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
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testOutputResultsUnsupportedFormat(self):
    """Tests that passing in an unsupported format is an error."""
    with self.assertRaises(RuntimeError):
      result_output.OutputResults(data_types.TestExpectationMap(),
                                  data_types.TestExpectationMap(),
                                  data_types.TestExpectationMap(), {}, {},
                                  'asdf')

  def testOutputResultsSmoketest(self):
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
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testOutput(self):
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
  def setUp(self):
    self.setUpPyfakefs()
    self._file_handle = tempfile.NamedTemporaryFile(delete=False, mode='w')
    self._filepath = self._file_handle.name

  def testSingleLine(self):
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

  def testBugLimit(self):
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

  def testLengthLimit(self):
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

  def testSingleBugOverLineLimit(self):
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

  def testOrphanedBugs(self):
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

  def testOnlyOrphanedBugs(self):
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


class ConvertBuilderMapToPassOrderedStringDictUnittest(unittest.TestCase):
  def testEmptyInput(self):
    """Tests that an empty input doesn't cause breakage."""
    output = result_output.ConvertBuilderMapToPassOrderedStringDict(
        data_types.BuilderStepMap())
    expected_output = collections.OrderedDict()
    expected_output[result_output.FULL_PASS] = {}
    expected_output[result_output.NEVER_PASS] = {}
    expected_output[result_output.PARTIAL_PASS] = {}
    self.assertEqual(output, expected_output)

  def testBasic(self):
    """Tests that a map is properly converted."""
    builder_map = data_types.BuilderStepMap({
        'fully pass':
        data_types.StepBuildStatsMap({
            'step1': uu.CreateStatsWithPassFails(1, 0),
        }),
        'never pass':
        data_types.StepBuildStatsMap({
            'step3': uu.CreateStatsWithPassFails(0, 1),
        }),
        'partial pass':
        data_types.StepBuildStatsMap({
            'step5': uu.CreateStatsWithPassFails(1, 1),
        }),
        'mixed':
        data_types.StepBuildStatsMap({
            'step7': uu.CreateStatsWithPassFails(1, 0),
            'step8': uu.CreateStatsWithPassFails(0, 1),
            'step9': uu.CreateStatsWithPassFails(1, 1),
        }),
    })
    output = result_output.ConvertBuilderMapToPassOrderedStringDict(builder_map)

    expected_output = collections.OrderedDict()
    expected_output[result_output.FULL_PASS] = {
        'fully pass': [
            'step1 (1/1 passed)',
        ],
        'mixed': [
            'step7 (1/1 passed)',
        ],
    }
    expected_output[result_output.NEVER_PASS] = {
        'never pass': [
            'step3 (0/1 passed)',
        ],
        'mixed': [
            'step8 (0/1 passed)',
        ],
    }
    expected_output[result_output.PARTIAL_PASS] = {
        'partial pass': {
            'step5 (1/2 passed)': [
                'http://ci.chromium.org/b/build_id0',
            ],
        },
        'mixed': {
            'step9 (1/2 passed)': [
                'http://ci.chromium.org/b/build_id0',
            ],
        },
    }
    self.assertEqual(output, expected_output)


def _Dedent(s):
  output = ''
  for line in s.splitlines(True):
    output += line.lstrip()
  return output


if __name__ == '__main__':
  unittest.main(verbosity=2)
