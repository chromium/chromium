#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit test for milestones.py"""

import argparse
import os
import sys
import tempfile
import textwrap
import unittest

INFRA_CONFIG_DIR = os.path.abspath(os.path.join(__file__, '..', '..', '..'))

sys.path.append(os.path.join(INFRA_CONFIG_DIR, 'scripts'))

import milestones

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

class MilestonesUnitTest(unittest.TestCase):

  def test_parse_args_fails_without_subcommand(self):
    with self.assertRaises(ParseError) as caught:
      milestones.parse_args([], parser_type=ArgumentParser)
    self.assertEqual(str(caught.exception), 'no sub-command specified')

  def test_activate_parse_args_fails_when_missing_required_args(self):
    with self.assertRaises(ParseError) as caught:
      milestones.parse_args(['activate'], parser_type=ArgumentParser)
    self.assertEqual(
        str(caught.exception),
        'the following arguments are required: --milestone, --branch')

  def test_activate_parse_args(self):
    args = milestones.parse_args(
        ['activate', '--milestone', 'MM', '--branch', 'BBBB'],
        parser_type=ArgumentParser)
    self.assertEqual(args.milestone, 'MM')
    self.assertEqual(args.branch, 'BBBB')

  def test_numeric_sort_key(self):
    self.assertEqual(
        sorted(['b10', 'b010', 'b9', 'a10', 'a1', 'a9'],
               key=milestones.numeric_sort_key),
        ['a1', 'a9', 'a10', 'b9', 'b010', 'b10'])

  def test_add_milestone_fails_when_milestone_already_active(self):
    current_milestones = {
        '99': {
            'name': 'm99',
            'project': 'chromium-m99',
            'ref': 'refs/branch-heads/AAAA',
        },
    }
    with self.assertRaises(milestones.MilestonesException) as caught:
      milestones.add_milestone(
          current_milestones, milestone='99', branch='BBBB')
    self.assertIn(
        "there is already an active milestone with id '99'",
        str(caught.exception))

  def test_add_milestone(self):
    current_milestones = {
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
    output = milestones.add_milestone(
        current_milestones, milestone='100', branch='CCCC')
    self.assertEqual(output, textwrap.dedent("""\
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

  def test_remove_milestone_fails_when_milestone_not_active(self):
    current_milestones = {}
    with self.assertRaises(milestones.MilestonesException) as caught:
      milestones.remove_milestone(current_milestones, '99')
    self.assertIn(
        "'99' does not refer to an active milestone", str(caught.exception))

  def test_remove_milestone(self):
    current_milestones = {
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
    output = milestones.remove_milestone(current_milestones, milestone='99')
    self.assertEqual(output, textwrap.dedent("""\
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