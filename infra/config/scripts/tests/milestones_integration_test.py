#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration test for milestones.py"""

import json
import os
import subprocess
import tempfile
import textwrap
import unittest

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..', '..'))
MILESTONES_PY = os.path.join(INFRA_CONFIG_DIR, 'scripts', 'milestones.py')

class MilestonesIntgrationTest(unittest.TestCase):

  def setUp(self):
    self._temp_dir = tempfile.TemporaryDirectory()
    self._milestones_json = os.path.join(self._temp_dir.name, 'milestones.json')

  def tearDown(self):
    self._temp_dir.cleanup()

  def _execute_milestones_py(self, args):
    cmd = [MILESTONES_PY, '--milestones-json', self._milestones_json]
    if os.name == 'nt':
      cmd = ['vpython3.bat'] + cmd
    return subprocess.run((cmd + (args or [])), text=True, capture_output=True)

  def test_activate_fails_when_missing_required_args(self):
    result = self._execute_milestones_py(['activate'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        'the following arguments are required: --milestone, --branch',
        result.stderr)

  def test_activate_fails_when_milestone_already_active(self):
    milestones = {
        '99': {
            'name': 'm99',
            'project': 'chromium-m99',
            'ref': 'refs/branch-heads/AAAA',
        },
    }
    with open(self._milestones_json, 'w') as f:
      json.dump(milestones, f, indent=4)

    result = self._execute_milestones_py(
        ['activate', '--milestone', '99', '--branch', 'BBBB'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        "there is already an active milestone with id '99'",
        result.stderr)

  def test_activate_rewrites_milestones_json(self):
    milestones = {
        '99': {
            'name': 'm99',
            'project': 'chromium-m99',
            'ref': 'refs/branch-heads/AAAA',
        },
        '101': {
            'name': 'm101',
            'project': 'chromium-m101',
            'ref': 'refs/branch-heads/BBBB',
        },
    }
    with open(self._milestones_json, 'w') as f:
      json.dump(milestones, f, indent=4)

    result = self._execute_milestones_py(
        ['activate', '--milestone', '100', '--branch', 'CCCC'])
    self.assertEqual(
        result.returncode, 0,
        (f'subprocess failed\n***COMMAND***\n{result.args}\n'
         f'***STDERR***\n{result.stderr}'))

    with open(self._milestones_json) as f:
      milestones = f.read()
    self.assertEqual(milestones,  textwrap.dedent("""\
        {
            "99": {
                "name": "m99",
                "project": "chromium-m99",
                "ref": "refs/branch-heads/AAAA"
            },
            "100": {
                "name": "m100",
                "project": "chromium-m100",
                "ref": "refs/branch-heads/CCCC"
            },
            "101": {
                "name": "m101",
                "project": "chromium-m101",
                "ref": "refs/branch-heads/BBBB"
            }
        }
        """))

  def test_deactivate_fails_when_missing_required_args(self):
    result = self._execute_milestones_py(['deactivate'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        'the following arguments are required: --milestone',
        result.stderr)

  def test_deactivate_fails_when_milestone_not_active(self):
    milestones = {}
    with open(self._milestones_json, 'w') as f:
      json.dump(milestones, f, indent=4)

    result = self._execute_milestones_py(['deactivate', '--milestone', '99'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        "'99' does not refer to an active milestone", result.stderr)

  def test_deactivate_rewrites_milestones_json(self):
    milestones = {
        '99': {
            'name': 'm99',
            'project': 'chromium-m99',
            'ref': 'refs/branch-heads/AAAA',
        },
        '101': {
            'name': 'm101',
            'project': 'chromium-m101',
            'ref': 'refs/branch-heads/BBBB',
        },
        '100': {
            'name': 'm100',
            'project': 'chromium-m100',
            'ref': 'refs/branch-heads/CCCC'
        },
    }
    with open(self._milestones_json, 'w') as f:
      json.dump(milestones, f, indent=4)

    result = self._execute_milestones_py(['deactivate', '--milestone', '99'])
    self.assertEqual(
        result.returncode, 0,
        (f'subprocess failed\n***COMMAND***\n{result.args}\n'
         f'***STDERR***\n{result.stderr}'))

    with open(self._milestones_json) as f:
      milestones = f.read()
    self.assertEqual(milestones,  textwrap.dedent("""\
        {
            "100": {
                "name": "m100",
                "project": "chromium-m100",
                "ref": "refs/branch-heads/CCCC"
            },
            "101": {
                "name": "m101",
                "project": "chromium-m101",
                "ref": "refs/branch-heads/BBBB"
            }
        }
        """))

if __name__ == '__main__':
  unittest.main()
