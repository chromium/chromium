# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from io import StringIO
import os
import unittest

import mojom.format.format as mojofmt


class MojomFormatTest(unittest.TestCase):

    def setUp(self):
        self.maxDiff = None
        self.test_dir = os.path.join(os.path.dirname(__file__), 'testdata')

    def golden_test(self, basename):
        infile = os.path.join(self.test_dir, f'{basename}.in')
        outfile = os.path.join(self.test_dir, f'{basename}.out')
        actual = mojofmt.mojom_format(infile)
        with open(outfile) as f:
            expected = f.read()
        self.assertMultiLineEqual(expected, actual)

    def test_attributes(self):
        self.golden_test('attributes')

    def test_comments(self):
        self.golden_test('comments')

    def test_copyright(self):
        self.golden_test('copyright')

    def test_enum(self):
        self.golden_test('enum')

    def test_imports(self):
        self.golden_test('imports')

    def test_interface_odds(self):
        self.golden_test('interface_odds')

    def test_long_comment(self):
        self.golden_test('long_comment')

    def test_method_wrapping(self):
        self.golden_test('method_wrapping')

    def test_struct_fields(self):
        self.golden_test('struct_fields')


class LineWrapperTest(unittest.TestCase):

    def testFineOneLine(self):
        lw = mojofmt.LineWrapper()
        data = 'const uint32 kFoo = 32;'
        lw.write(data)
        self.assertEqual(data, lw.finish())

    def testSimpleWrap(self):
        lw = mojofmt.LineWrapper()
        data = 'const uint32 kLongName' + ('A' * 55) + ' = 32;'
        lw.write(data)
        expected = 'const uint32 kLongName' + ('A' * 55) + ' =\n    32;'
        self.assertEqual(expected, lw.finish())

    def testMultiWrap(self):
        lw = mojofmt.LineWrapper()
        data = ('A' * 50) + ' ' + ('B' * 50) + ' ' + ('C' * 50)
        lw.write(data)
        expected = 'A' * 50 + '\n    ' + 'B' * 50 + '\n        ' + 'C' * 50
        self.assertEqual(expected, lw.finish())

    def testWrapWithIndent(self):
        lw = mojofmt.LineWrapper(base_indent=2)
        data = ('array<blink.mojom.ParsedPermissionsPolicyDeclaration> ' +
                'permissions_policy_header;')
        lw.write(data)
        expected = ('  array<blink.mojom.ParsedPermissionsPolicyDeclaration>' +
                    '\n      permissions_policy_header;')
        self.assertEqual(expected, lw.finish())

    def testAlreadyIndented(self):
        lw = mojofmt.LineWrapper(base_indent=2, already_indented=True)
        data = ('A' * 50) + ' ' + ('B' * 50)
        lw.write(data)
        expected = ('A' * 50) + '\n      ' + ('B' * 50)
        self.assertEqual(expected, lw.finish())

        lw = mojofmt.LineWrapper(base_indent=2, already_indented=False)
        lw.write(data)
        self.assertEqual('  ' + expected, lw.finish())

    def testBreakWithExtension(self):
        lw = mojofmt.LineWrapper()
        data = ('A' * 85) + ' ' + ('B' * 20)
        lw.write(data)
        expected = ('A' * 85) + '\n    ' + ('B' * 20)
        self.assertEqual(expected, lw.finish())
