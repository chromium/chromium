#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for branch.py"""

import argparse
import os
import sys
import textwrap
import unittest

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..', '..'))

sys.path.append(os.path.join(INFRA_CONFIG_DIR, 'scripts'))

import branch

class ParseError(Exception):
  pass

class ArgumentParser(argparse.ArgumentParser):
  """Test version of ArgumentParser

  This behaves the same as argparse.ArgumentParser except that the error
  method raises an instance of ParseError rather than printing output
  and exiting. This simplifies testing for error conditions and puts the
  actual error information in the traceback for unexpectedly failing
  tests.
  """

  def error(self, message):
    raise ParseError(message)

class BranchUnitTest(unittest.TestCase):

  def test_parse_args_fails_without_subcommand(self):
    with self.assertRaises(ParseError) as caught:
      branch.parse_args([], parser_type=ArgumentParser)
    self.assertEqual(str(caught.exception), 'no sub-command specified')

  def test_initialize_parse_args_fails_when_missing_required_args(self):
    with self.assertRaises(ParseError) as caught:
      branch.parse_args(['initialize'], parser_type=ArgumentParser)
    self.assertEqual(
        str(caught.exception),
        'the following arguments are required: --milestone, --branch')

  def test_initialize_parse_args(self):
    args = branch.parse_args(
        ['initialize', '--milestone', 'MM', '--branch', 'BBBB'],
        parser_type=ArgumentParser)
    self.assertEqual(args.milestone, 'MM')
    self.assertEqual(args.branch, 'BBBB')

  def test_initial_settings(self):
    output = branch.initial_settings(
        milestone='MM',
        branch='BBBB',
        chromium_project='CHROMIUM',
        chrome_project='CHROME',
    )
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "CHROMIUM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/BBBB",
                "chrome_project": "CHROME",
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
    with self.assertRaises(ParseError) as caught:
      branch.parse_args(['enable-platform'], parser_type=ArgumentParser)
    self.assertEqual(
        str(caught.exception),
        'the following arguments are required: platform, --description')

  def test_enable_platform_parse_args(self):
    args = branch.parse_args([
        'enable-platform', 'fake-platform', '--description', 'fake-description'
    ])
    self.assertEqual(args.platform, 'fake-platform')
    self.assertEqual(args.description, 'fake-description')
    self.assertIsNone(args.gardener_rotation)

  def test_enable_platform_parse_args_gardener_rotation(self):
    args = branch.parse_args([
        'enable-platform',
        'fake-platform',
        '--description',
        'fake-description',
        '--gardener-rotation',
        'fake-gardener-rotation',
    ])
    self.assertEqual(args.platform, 'fake-platform')
    self.assertEqual(args.description, 'fake-description')
    self.assertEqual(args.gardener_rotation, 'fake-gardener-rotation')

  def test_enable_platform(self):
    input = textwrap.dedent("""\
        {
            "project": "chromium-mMM",
            "project_title": "Chromium MMM",
            "ref": "refs/branch-heads/AAAA",
            "is_main": true
        }""")
    output = branch.enable_platform(
        input,
        'fake-platform',
        'fake-description',
        None,
    )
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "chromium-mMM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/AAAA",
                "is_main": false,
                "platforms": {
                    "fake-platform": {
                        "description": "fake-description"
                    }
                }
            }
            """))

  def test_enable_platform_gardener_rotation(self):
    input = textwrap.dedent("""\
        {
            "project": "chromium-mMM",
            "project_title": "Chromium MMM",
            "ref": "refs/branch-heads/AAAA",
            "is_main": true
        }""")
    output = branch.enable_platform(
        input,
        'fake-platform',
        'fake-description',
        'fake-gardener-rotation',
    )
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "chromium-mMM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/AAAA",
                "is_main": false,
                "platforms": {
                    "fake-platform": {
                        "description": "fake-description",
                        "gardener_rotation": "fake-gardener-rotation"
                    }
                }
            }
            """))

  def test_enable_platform_with_existing_platforms(self):
    input = textwrap.dedent("""\
        {
            "project": "chromium-mMM",
            "project_title": "Chromium MMM",
            "ref": "refs/branch-heads/AAAA",
            "platforms": {
                "fake-platform2": {
                    "description": "fake-description"
                }
            },
            "is_main": false
        }""")
    output = branch.enable_platform(
        input,
        'fake-platform1',
        'fake-description',
        None,
    )
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "chromium-mMM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/AAAA",
                "is_main": false,
                "platforms": {
                    "fake-platform1": {
                        "description": "fake-description"
                    },
                    "fake-platform2": {
                        "description": "fake-description"
                    }
                }
            }
            """))


if __name__ == '__main__':
  unittest.main()
