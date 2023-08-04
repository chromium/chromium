# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.path_finder import PathFinder
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.filesystem_mock import MockFileSystem


class TestPathFinder(unittest.TestCase):
    def test_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder.chromium_base(), '/mock-checkout')

    def test_path_from_chromium_base(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_chromium_base('foo', 'bar.baz'),
            '/mock-checkout/foo/bar.baz')

    def test_web_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder.web_tests_dir(),
                         '/mock-checkout/' + RELATIVE_WEB_TESTS[:-1])

    def test_web_tests_dir_with_backslash_sep(self):
        filesystem = MockFileSystem()
        filesystem.sep = '\\'
        filesystem.path_to_module = \
            lambda _: ('C:\\mock-checkout\\third_party\\blink\\tools\\blinkpy\\foo.py')
        finder = PathFinder(filesystem)
        self.assertEqual(finder.web_tests_dir(),
                         'C:\\mock-checkout\\third_party\\blink\\web_tests')

    def test_perf_tests_dir(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(finder.perf_tests_dir(),
                         '/mock-checkout/third_party/blink/perf_tests')

    def test_path_from_web_tests(self):
        finder = PathFinder(MockFileSystem())
        self.assertEqual(
            finder.path_from_web_tests('external', 'wpt'),
            '/mock-checkout/' + RELATIVE_WEB_TESTS + 'external/wpt')

    def test_depot_tools_base_not_found(self):
        filesystem = MockFileSystem()
        filesystem.path_to_module = \
            lambda _: ('/mock-checkout/third_party/blink/tools/blinkpy/common/'
                       'path_finder.py')
        finder = PathFinder(filesystem)
        self.assertIsNone(finder.depot_tools_base())

    def test_depot_tools_base_exists(self):
        filesystem = MockFileSystem()
        filesystem.path_to_module = \
            lambda _: ('/checkout/third_party/blink/tools/blinkpy/common/'
                       'path_finder.py')
        filesystem.maybe_make_directory('/checkout/third_party/depot_tools')
        finder = PathFinder(filesystem)
        self.assertEqual(finder.depot_tools_base(),
                         '/checkout/third_party/depot_tools')

    def test_strip_web_tests_path(self):
        finder = PathFinder(MockFileSystem())
        path_with_web_tests = '/mock-checkout/' + RELATIVE_WEB_TESTS + 'external/wpt'
        self.assertEqual(
            finder.strip_web_tests_path(path_with_web_tests), 'external/wpt')
        path_without_web_tests = '/checkout/' + RELATIVE_WEB_TESTS + 'external/wpt'
        self.assertEqual(
            finder.strip_web_tests_path(path_without_web_tests),
            path_without_web_tests)
