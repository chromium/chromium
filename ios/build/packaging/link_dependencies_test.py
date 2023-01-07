#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from link_dependencies import extract_inputs, library_deps
import unittest


class ExtractInputsTest(unittest.TestCase):
  def test_empty(self):
    self.assertListEqual([], extract_inputs(''))
    query_result = '\n'.join([
      'header',
      '  input: link',
      '  outputs: nothing',
      'footer'
    ])
    self.assertListEqual([], extract_inputs(query_result))

  def test_one_input(self):
    query_result = '\n'.join([
      'header',
      '  input: link',
      '    foo',
      '  outputs: link',
      '    quxx',
      'footer'
    ])
    self.assertListEqual(['foo'], extract_inputs(query_result))

  def test_many_inputs(self):
    query_result = '\n'.join([
      'header',
      '  input: link',
      '    foo',
      '    bar',
      '    baz',
      '  outputs:',
      '    quxx',
      'footer'
    ])
    self.assertListEqual(['foo', 'bar', 'baz'], extract_inputs(query_result))

  def test_no_pipe_inputs(self):
    query_result = '\n'.join([
      'header',
      '  input: link',
      '    |foo',
      '    bar',
      '    |baz',
      '  outputs:',
      '    quxx',
      'footer'
    ])
    self.assertListEqual(['bar'], extract_inputs(query_result))

  def test_prefix(self):
    query_result = '\n'.join([
      'header',
      '  input: link',
      '    bar',
      '  outputs:',
      'footer'
    ])
    self.assertListEqual(['foo/bar'],
                         extract_inputs(query_result, 'foo/'))


class LibraryDepsTest(unittest.TestCase):
  def mkquery(self, answers):
    """Creates a query function for library_deps.

    Creates a query function for library_deps that returns answers from the
    supplied map.
    """
    return lambda target, unused_workdir, unused_prefix='': answers[target]

  def test_empty(self):
    self.assertEqual(set(), library_deps([], 'wd', self.mkquery({})))

  def test_nonarch(self):
    deps = ['abc.a', 'def.o', 'abc.a']
    self.assertEqual({'wd/abc.a', 'wd/def.o'},
                     library_deps(deps, 'wd', self.mkquery({})))

  def test_arch(self):
    dep_map = {'abc': ['wd/def.a', 'wd/ghi.o']}
    deps = ['abc', 'jkl.o']
    self.assertEqual({'wd/def.a', 'wd/ghi.o', 'wd/jkl.o'},
                     library_deps(deps, 'wd', self.mkquery(dep_map)))

  def test_arch_many(self):
    dep_map = {'abc': ['wd/def.o', 'wd/ghi.a'],
               'def': ['wd/def.o', 'wd/jkl.a']}
    deps = ['abc', 'def', 'def.o']
    self.assertEqual({'wd/def.o', 'wd/ghi.a', 'wd/jkl.a'},
                     library_deps(deps, 'wd', self.mkquery(dep_map)))


if __name__ == '__main__':
  unittest.main()
