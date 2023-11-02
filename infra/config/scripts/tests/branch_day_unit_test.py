#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Integration test for branch-day.py"""

import json
import os
import shutil
import subprocess
import tempfile
import unittest

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..', '..'))
BRANCH_DAY_PY = os.path.join(INFRA_CONFIG_DIR, 'scripts', 'branch-day.py')
MOCK_PY = os.path.join(INFRA_CONFIG_DIR, 'scripts', 'tests', 'utils', 'mock.py')


class BranchDayUnitTest(unittest.TestCase):
  def setUp(self):
    self._temp_dir = tempfile.TemporaryDirectory()
    self._invocations_file = os.path.join(self._temp_dir.name,
                                          'invocations.json')
    self._milestones_py = os.path.join(self._temp_dir.name, 'milestones.py')
    self._branch_py = os.path.join(self._temp_dir.name, 'branch.py')
    self._main_star = os.path.join(self._temp_dir.name, 'main.star')
    self._dev_star = os.path.join(self._temp_dir.name, 'dev.star')

    self._binaries = (self._milestones_py, self._branch_py, self._main_star,
                      self._dev_star)

    for path in self._binaries:
      shutil.copy2(MOCK_PY, path)

  def tearDown(self):
    self._temp_dir.cleanup()

  def _execute_branch_day_py(self, args, mock_details=None):
    def details(binary, stdout=None, stderr=None, exit_code=None):
      binary = os.path.basename(binary)
      d = {
          'stdout': stdout or 'fake {} stdout'.format(binary),
          'stderr': stderr or 'fake {} stderr'.format(binary),
      }
      if exit_code:
        d['exit_code'] = exit_code
      return d

    mock_details = mock_details or {}
    mock_details = {
        b: details(b, **mock_details.get(b, {}))
        for b in self._binaries
    }

    env = os.environ.copy()
    env.update({
        'INVOCATIONS_FILE': self._invocations_file,
        'MOCK_DETAILS': json.dumps(mock_details),
    })

    cmd = [
        BRANCH_DAY_PY, '--milestones-py', self._milestones_py, '--branch-py',
        self._branch_py, '--main-star', self._main_star, '--dev-star',
        self._dev_star
    ]
    if os.name == 'nt':
      cmd = ['vpython3.bat'] + cmd
    cmd += args or []
    return subprocess.run(cmd,
                          env=env,
                          text=True,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)

  def test_branch_day_invocation_fails(self):
    result = self._execute_branch_day_py(
        ['--milestone', 'XX', '--branch', 'YYYY'],
        mock_details={
            self._milestones_py: {
                'stdout': 'FAKE FAILURE STDOUT',
                'stderr': 'FAKE FAILURE STDERR',
                'exit_code': 1,
            }
        })
    self.assertNotEqual(result.returncode, 0)
    cmd = [self._milestones_py]
    if os.name == 'nt':
      cmd = ['vpython3.bat'] + cmd
    expected_output = '\n'.join([
        'Executing {} failed'.format(
            cmd + ['activate', '--milestone', 'XX', '--branch', 'YYYY']),
        'FAKE FAILURE STDOUT',
        'FAKE FAILURE STDERR',
        '',
    ])
    self.assertEqual(result.stdout, expected_output)

  def test_branch_day(self):
    result = self._execute_branch_day_py(
        ['--milestone', 'XX', '--branch', 'YYYY'])
    self.assertEqual(result.returncode, 0,
                     (f'subprocess failed\n***COMMAND***\n{result.args}\n'
                      f'***OUTPUT***\n{result.stdout}\n'))
    self.assertEqual(result.stdout, '')

    with open(self._invocations_file) as f:
      invocations = json.load(f)
    expected_invocations = [
        [
            self._milestones_py, 'activate', '--milestone', 'XX', '--branch',
            'YYYY'
        ],
        [self._main_star],
        [self._dev_star],
    ]
    self.assertEqual(invocations, expected_invocations)

  def test_branch_day_on_branch(self):
    result = self._execute_branch_day_py(
        ['--on-branch', '--milestone', 'XX', '--branch', 'YYYY'])
    self.assertEqual(result.returncode, 0,
                     (f'subprocess failed\n***COMMAND***\n{result.args}\n'
                      f'***OUTPUT***\n{result.stdout}\n'))
    self.assertEqual(result.stdout, '')

    with open(self._invocations_file) as f:
      invocations = json.load(f)
    expected_invocations = [
        [
            self._branch_py, 'initialize', '--milestone', 'XX', '--branch',
            'YYYY'
        ],
        [self._main_star],
        [self._dev_star],
    ]
    self.assertEqual(invocations, expected_invocations)


if __name__ == '__main__':
  unittest.main()
