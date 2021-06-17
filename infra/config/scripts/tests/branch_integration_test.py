#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration test for branch.py"""

import json
import os
import subprocess
import tempfile
import textwrap
import unittest

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..', '..'))
BRANCH_PY = os.path.join(INFRA_CONFIG_DIR, 'scripts', 'branch.py')

class BranchIntegrationTest(unittest.TestCase):

  def setUp(self):
    self._temp_dir = tempfile.TemporaryDirectory()
    self._settings_json = os.path.join(self._temp_dir.name, 'settings.json')

  def tearDown(self):
    self._temp_dir.cleanup()

  def _execute_branch_py(self, args):
    return subprocess.run(
        [BRANCH_PY, '--settings-json', self._settings_json] + (args or []),
        text=True, capture_output=True)

  def test_initialize_fails_when_missing_required_args(self):
    result = self._execute_branch_py(['initialize'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        'the following arguments are required: --milestone, --branch',
        result.stderr)

  def test_initialize_rewrites_settings_json(self):
    result = self._execute_branch_py(
        ['initialize', '--milestone', 'XX', '--branch', 'YYYY'])
    self.assertEqual(
        result.returncode, 0,
        (f'subprocess failed\n***COMMAND***\n{result.args}\n'
         f'***STDERR***\n{result.stderr}\n'))

    with open(self._settings_json) as f:
      settings = f.read()
    self.assertEqual(
        settings,
        textwrap.dedent("""\
        {
            "project": "chromium-mXX",
            "project_title": "Chromium MXX",
            "ref": "refs/branch-heads/YYYY",
            "chrome_project": "chrome-mXX",
            "branch_types": [
                "standard"
            ]
        }
        """))

  def test_set_type_fails_when_missing_required_args(self):
    result = self._execute_branch_py(['set-type'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn('the following arguments are required: --type', result.stderr)

  def test_set_type_fails_for_invalid_type(self):
    result = self._execute_branch_py(['set-type', '--type', 'foo'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn("invalid choice: 'foo'", str(result.stderr))

  def test_set_type_rewrites_settings_json(self):
    with open(self._settings_json, 'w') as f:
      settings = {
          "project": "chromium-mXX",
          "project_title": "Chromium MXX",
          "ref": "refs/branch-heads/YYYY"
      }
      json.dump(settings, f)

    result = self._execute_branch_py(['set-type', '--type', 'cros-lts'])
    self.assertEqual(result.returncode, 0,
                     (f'subprocess failed\n***COMMAND***\n{result.args}\n'
                      f'***STDERR***\n{result.stderr}\n'))

    with open(self._settings_json) as f:
      settings = f.read()
    self.assertEqual(
        settings,
        textwrap.dedent("""\
            {
                "project": "chromium-mXX",
                "project_title": "Chromium MXX",
                "ref": "refs/branch-heads/YYYY",
                "branch_types": [
                    "cros-lts"
                ]
            }
            """))


if __name__ == '__main__':
  unittest.main()
