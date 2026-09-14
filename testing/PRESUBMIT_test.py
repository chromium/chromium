# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import ntpath
import unittest

import PRESUBMIT

# pylint: disable=protected-access


class MockAffectedFile:
  def __init__(self, path):
    self._path = path

  def AbsoluteLocalPath(self):
    return self._path


class MockInputApi:
  def __init__(self, affected_paths=None, no_diffs=False):
    self.os_path = os.path
    self.re = re
    self.no_diffs = no_diffs
    self._affected_files = [MockAffectedFile(p) for p in (affected_paths or [])]

  @staticmethod
  def PresubmitLocalPath():
    return os.path.dirname(os.path.abspath(__file__))

  def AffectedFiles(self):
    return self._affected_files


class GetPylintFilesToCheckTest(unittest.TestCase):
  def setUp(self):
    self.testing_dir = os.path.dirname(os.path.abspath(__file__))

  def testNoDiffsReturnsNone(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, 'buildbot', 'generate_buildbot_json.py')
      ],
      no_diffs=True,
    )
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(input_api))

  def testThisPresubmitModifiedReturnsNone(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, 'PRESUBMIT.py'),
        os.path.join(self.testing_dir, 'buildbot', 'generate_buildbot_json.py'),
      ]
    )
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(input_api))

  def testRelativePresubmitLocalPath(self):
    rel_root = os.path.relpath(self.testing_dir)
    input_api = MockInputApi(
      affected_paths=[os.path.join(self.testing_dir, 'PRESUBMIT.py')]
    )
    input_api.PresubmitLocalPath = staticmethod(lambda: rel_root)
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(input_api))

  def testRootPythonFileModifiedReturnsNone(self):
    input_api = MockInputApi(
      affected_paths=[os.path.join(self.testing_dir, 'test_env.py')]
    )
    self.assertIsNone(PRESUBMIT._GetPylintFilesToCheck(input_api))

  def testNonPythonFilesReturnsEmptyList(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, 'buildbot', 'test.json'),
        os.path.join(self.testing_dir, 'test.gni'),
      ]
    )
    self.assertEqual(PRESUBMIT._GetPylintFilesToCheck(input_api), [])

  def testFilesOutsideTestingReturnsEmptyList(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(os.path.dirname(self.testing_dir), 'chrome', 'test.py')
      ]
    )
    self.assertEqual(PRESUBMIT._GetPylintFilesToCheck(input_api), [])

  def testSingleSubdirectoryScoped(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, 'buildbot', 'generate_buildbot_json.py'),
        os.path.join(self.testing_dir, 'buildbot', 'check.py'),
      ]
    )
    self.assertEqual(
      PRESUBMIT._GetPylintFilesToCheck(input_api),
      [r'buildbot(?:/|\\).*\.py$'],
    )

  def testMultipleSubdirectoriesScoped(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, 'scripts', 'run_telemetry.py'),
        os.path.join(self.testing_dir, 'buildbot', 'generate_buildbot_json.py'),
      ]
    )
    self.assertEqual(
      PRESUBMIT._GetPylintFilesToCheck(input_api),
      [r'buildbot(?:/|\\).*\.py$', r'scripts(?:/|\\).*\.py$'],
    )

  def testNestedSubdirectoryScopedToFirstLevel(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(
          self.testing_dir,
          'variations',
          'presubmit',
          'variations_presubmit.py',
        )
      ]
    )
    self.assertEqual(
      PRESUBMIT._GetPylintFilesToCheck(input_api),
      [r'variations(?:/|\\).*\.py$'],
    )

  def testWindowsPathSeparatorsAndCasing(self):

    testing_dir_win = ntpath.normpath("C:/src/chromium/src/testing")
    file_path_win = ntpath.normpath(
      "C:/src/chromium/src/testing/Buildbot/generate_buildbot_json.py"
    )
    input_api = MockInputApi(affected_paths=[file_path_win])
    input_api.os_path = ntpath
    input_api.PresubmitLocalPath = staticmethod(lambda: testing_dir_win)
    self.assertEqual(
      PRESUBMIT._GetPylintFilesToCheck(input_api),
      [r'Buildbot(?:/|\\).*\.py$'],
    )

  def testPresubmitLocalPathWithTrailingSlash(self):
    input_api = MockInputApi(
      affected_paths=[
        os.path.join(self.testing_dir, "buildbot", "generate_buildbot_json.py")
      ]
    )
    input_api.PresubmitLocalPath = staticmethod(
      lambda: self.testing_dir + os.path.sep
    )
    self.assertEqual(
      PRESUBMIT._GetPylintFilesToCheck(input_api),
      [r"buildbot(?:/|\\).*\.py$"],
    )

  def testCheckPylintSkipsWhenNoPythonFiles(self):
    input_api = MockInputApi(
      affected_paths=[os.path.join(self.testing_dir, "buildbot", "test.json")]
    )
    # CheckPylint should short-circuit and return [] when
    # _GetPylintFilesToCheck returns [].
    self.assertEqual(PRESUBMIT.CheckPylint(input_api, None), [])


if __name__ == "__main__":
  unittest.main()
