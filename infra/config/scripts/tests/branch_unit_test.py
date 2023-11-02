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
    output = branch.initial_settings(milestone='MM', branch='BBBB')
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "chromium-mMM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/BBBB",
                "chrome_project": "chrome-mMM",
                "branch_types": [
                    "standard"
                ]
            }
            """))

  def test_set_type_parse_args_fails_when_missing_required_args(self):
    with self.assertRaises(ParseError) as caught:
      branch.parse_args(['set-type'], parser_type=ArgumentParser)
    self.assertEqual(str(caught.exception),
                     'the following arguments are required: --type')

  def test_set_type_parse_args_fails_for_invalid_type(self):
    with self.assertRaises(ParseError) as caught:
      branch.parse_args(['set-type', '--type', 'foo'],
                        parser_type=ArgumentParser)
    self.assertIn("invalid choice: 'foo'", str(caught.exception))

  def test_set_type_parse_args(self):
    args = branch.parse_args(
        ['set-type', '--type', 'desktop-extended-stable', '--type', 'cros-lts'])
    self.assertEqual(args.type, ['desktop-extended-stable', 'cros-lts'])

  def test_set_type(self):
    input = textwrap.dedent("""\
        {
            "project": "chromium-mMM",
            "project_title": "Chromium MMM",
            "ref": "refs/branch-heads/AAAA"
        }""")
    output = branch.set_type(
        input, ['desktop-extended-stable', 'cros-lts', 'fuchsia-lts'])
    self.assertEqual(
        output,
        textwrap.dedent("""\
            {
                "project": "chromium-mMM",
                "project_title": "Chromium MMM",
                "ref": "refs/branch-heads/AAAA",
                "branch_types": [
                    "desktop-extended-stable",
                    "cros-lts",
                    "fuchsia-lts"
                ]
            }
            """))


if __name__ == '__main__':
  unittest.main()
