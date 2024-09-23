#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
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
    cmd = [BRANCH_PY, '--settings-json', self._settings_json]
    if os.name == 'nt':
      cmd = ['vpython3.bat'] + cmd
    return subprocess.run(cmd + (args or []), text=True, capture_output=True)

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
            "is_main": false,
            "platforms": {
                "android": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "cros": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "fuchsia": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "ios": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "linux": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "mac": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "windows": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                }
            }
        }
        """))

  def test_initialize_test_config_rewrites_settings_json(self):
    result = self._execute_branch_py([
        'initialize', '--milestone', 'XX', '--branch', 'YYYY', '--test-config'
    ])
    self.assertEqual(result.returncode, 0,
                     (f'subprocess failed\n***COMMAND***\n{result.args}\n'
                      f'***STDERR***\n{result.stderr}\n'))

    with open(self._settings_json) as f:
      settings = f.read()
    self.assertEqual(
        settings,
        textwrap.dedent("""\
        {
            "project": "chromium",
            "project_title": "Chromium MXX",
            "ref": "refs/branch-heads/YYYY",
            "chrome_project": "chrome",
            "is_main": false,
            "platforms": {
                "android": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "cros": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "fuchsia": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "ios": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "linux": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "mac": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                },
                "windows": {
                    "description": "beta/stable",
                    "gardener_rotation": "chrome_browser_release"
                }
            }
        }
        """))

  def test_enable_platform_parse_args_fails_when_missing_required_args(self):
    result = self._execute_branch_py(['enable-platform'])
    self.assertNotEqual(result.returncode, 0)
    self.assertIn(
        'the following arguments are required: platform, --description',
        result.stderr)

  def test_enable_platform_rewrites_settings_json(self):
    with open(self._settings_json, 'w') as f:
      settings = {
          "project": "chromium-mXX",
          "project_title": "Chromium MXX",
          "ref": "refs/branch-heads/YYYY",
          "is_main": True
      }
      json.dump(settings, f)

    result = self._execute_branch_py([
        'enable-platform', 'fake-platform', '--description', 'fake-description'
    ])
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
                "is_main": false,
                "platforms": {
                    "fake-platform": {
                        "description": "fake-description"
                    }
                }
            }
            """))

  def test_enable_platform_with_gardener_rotation_rewrites_settings_json(self):
    with open(self._settings_json, 'w') as f:
      settings = {
          "project": "chromium-mXX",
          "project_title": "Chromium MXX",
          "ref": "refs/branch-heads/YYYY",
          "is_main": True
      }
      json.dump(settings, f)

    result = self._execute_branch_py([
        'enable-platform',
        'fake-platform',
        '--description',
        'fake-description',
        '--gardener-rotation',
        'fake-gardener-rotation',
    ])
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
                "is_main": false,
                "platforms": {
                    "fake-platform": {
                        "description": "fake-description",
                        "gardener_rotation": "fake-gardener-rotation"
                    }
                }
            }
            """))


if __name__ == '__main__':
  unittest.main()
