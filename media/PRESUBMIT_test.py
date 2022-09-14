#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import re
import unittest

import PRESUBMIT

class MockInputApi(object):
  def __init__(self):
    self.re = re
    self.os_path = os.path
    self.files = []
    self.is_committing = False

  def AffectedFiles(self):
    return self.files

  def AffectedSourceFiles(self, fn):
    # we'll just pretend everything is a source file for the sake of simplicity
    return self.files

  def ReadFile(self, f):
    return f.NewContents()


class MockOutputApi(object):
  class PresubmitResult(object):
    def __init__(self, message, items=None, long_text=''):
      self.message = message
      self.items = items
      self.long_text = long_text

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'error'

  class PresubmitPromptWarning(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'warning'

  class PresubmitNotifyResult(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'notify'

  class PresubmitPromptOrNotify(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'promptOrNotify'


class MockFile(object):
  def __init__(self, local_path, new_contents):
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_contents = [(i + 1, l) for i, l in enumerate(new_contents)]

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path


class MockChange(object):
  def __init__(self, changed_files):
    self._changed_files = changed_files

  def LocalPaths(self):
    return self._changed_files


class HistogramOffByOneTest(unittest.TestCase):

  # Take an input and make sure the problems found equals the expectation.
  def simpleCheck(self, contents, expected_errors):
    input_api = MockInputApi()
    input_api.files.append(MockFile('test.cc', contents))
    results = PRESUBMIT._CheckForHistogramOffByOne(input_api, MockOutputApi())
    if expected_errors:
      self.assertEqual(1, len(results))
      self.assertEqual(expected_errors, len(results[0].items))
    else:
      self.assertEqual(0, len(results))

  def testValid(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFooMax + 1);', 0)

  def testValidComments(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", /*...*/ kFoo, /*...*/'
                     'kFooMax + 1);', 0)

  def testValidMultiLine(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test",\n'
                     '                          kFoo,\n'
                     '                          kFooMax + 1);', 0)

  def testValidMultiLineComments(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test",  // This is the name\n'
                     '                          kFoo,  /* The value */\n'
                     '                          kFooMax + 1 /* The max */ );',
                     0)

  def testNoPlusOne(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFooMax);', 1)

  def testInvalidWithIgnore(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFooMax); '
                     '// PRESUBMIT_IGNORE_UMA_MAX', 0)

  def testNoMax(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFoo + 1);', 1)

  def testNoMaxNoPlusOne(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFoo);', 1)

  def testMultipleErrors(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFoo);\n'
                     'printf("hello, world!");\n'
                     'UMA_HISTOGRAM_ENUMERATION("test", kBar, kBarMax);', 2)

  def testValidAndInvalid(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFoo);\n'
                     'UMA_HISTOGRAM_ENUMERATION("test", kFoo, kFooMax + 1);'
                     'UMA_HISTOGRAM_ENUMERATION("test", kBar, kBarMax);', 2)

  def testInvalidMultiLine(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test",\n'
                     '                          kFoo,\n'
                     '                          kFooMax + 2);', 1)

  def testInvalidComments(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test", /*...*/, val, /*...*/,'
                     'Max);\n', 1)

  def testInvalidMultiLineComments(self):
    self.simpleCheck('UMA_HISTOGRAM_ENUMERATION("test",  // This is the name\n'
                     '                          kFoo,  /* The value */\n'
                     '                          kFooMax + 2 /* The max */ );',
                     1)

class NoV4L2AggregateInitializationTest(unittest.TestCase):

  def testValid(self):
    self._testChange(['struct v4l2_format_ format;'], 0)

  def testInvalid(self):
    self._testChange(['struct v4l2_format format = {};'], 1)
    self._testChange(['  struct v4l2_format format = {};'], 1)
    self._testChange(['  struct std::vector<v4l2_format> format[] = {};'], 1)
    self._testChange(['  struct std::vector<v4l2_format> format[] = {{}};'], 1)

  def _testChange(self, content, expected_warnings):
    mock_input_api = MockInputApi()
    mock_input_api.files.append(MockFile('test.cc', content))
    results = PRESUBMIT._CheckForNoV4L2AggregateInitialization(mock_input_api,
                                                               MockOutputApi())
    if expected_warnings:
      self.assertEqual(1, len(results))
      self.assertEqual(expected_warnings, len(results[0].items))
    else:
      self.assertEqual(0, len(results))


if __name__ == '__main__':
  unittest.main()
