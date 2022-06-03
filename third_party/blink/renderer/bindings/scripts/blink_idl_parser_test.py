# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=no-member,relative-import
"""Unit tests for blink_idl_parser.py."""

import unittest

from blink_idl_parser import BlinkIDLParser


class BlinkIDLParserTest(unittest.TestCase):
    def test_missing_semicolon_between_definitions(self):
        # No semicolon after enum definition.
        text = '''enum TestEnum { "value" } dictionary TestDictionary {};'''
        parser = BlinkIDLParser()
        parser.ParseText(filename='', data=text)
        self.assertGreater(parser.GetErrors(), 0)
