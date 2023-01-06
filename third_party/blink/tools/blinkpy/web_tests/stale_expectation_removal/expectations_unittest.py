#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import six

if six.PY3:
    import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest, fake_filesystem

from blinkpy.web_tests.stale_expectation_removal import constants
from blinkpy.web_tests.stale_expectation_removal import expectations


def CreateFile(fs: fake_filesystem.FakeFilesystem, *args, **kwargs) -> None:
    # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
    # Catapult is updated to a newer version of pyfakefs that is compatible
    # with Chromium's version.
    if hasattr(fs, 'create_file'):
        fs.create_file(*args, **kwargs)
    else:
        fs.CreateFile(*args, **kwargs)


class GetExpectationFilepathsUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self) -> None:
        self.setUpPyfakefs()
        self.instance = expectations.WebTestExpectations()
        CreateFile(
            self.fs,
            os.path.join(constants.WEB_TEST_ROOT_DIR, 'FlagExpectations',
                         'README.txt'))

    def testRealFilesCanBeFound(self) -> None:
        """Tests that real files are returned."""
        with fake_filesystem_unittest.Pause(self):
            filepaths = self.instance.GetExpectationFilepaths()
            self.assertTrue(len(filepaths) > 0)
            for f in filepaths:
                self.assertTrue(os.path.exists(f))

    def testTopLevelFiles(self) -> None:
        """Tests that top-level expectation files are properly returned."""
        top_level_filepath = os.path.join(constants.WEB_TEST_ROOT_DIR, 'foo')
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=['foo']):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, [top_level_filepath])

    def testFlagSpecificFiles(self) -> None:
        """Tests that flag-specific files are properly returned."""
        flag_filepath = os.path.join(constants.WEB_TEST_ROOT_DIR,
                                     'FlagExpectations', 'foo-flag')
        CreateFile(self.fs, flag_filepath)
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=[]):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, [flag_filepath])

    def testAllExpectationFiles(self) -> None:
        """Tests that both top level and flag-specific files are returned."""
        top_level_filepath = os.path.join(constants.WEB_TEST_ROOT_DIR, 'foo')
        flag_filepath = os.path.join(constants.WEB_TEST_ROOT_DIR,
                                     'FlagExpectations', 'foo-flag')
        CreateFile(self.fs, flag_filepath)
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=['foo']):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, [top_level_filepath, flag_filepath])


class GetExpectationFileTagHeaderUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self) -> None:
        self.setUpPyfakefs()
        self.instance = expectations.WebTestExpectations()

    def testRealContentsCanBeLoaded(self) -> None:
        """Tests that some sort of valid content can be read from the file."""
        with fake_filesystem_unittest.Pause(self):
            header = self.instance._GetExpectationFileTagHeader(
                expectations.MAIN_EXPECTATION_FILE)
        self.assertIn('tags', header)
        self.assertIn('results', header)

    def testContentLoading(self) -> None:
        """Tests that the header is properly loaded."""
        header_contents = """\

# foo
#   bar

# baz

not a comment
"""
        CreateFile(self.fs, expectations.MAIN_EXPECTATION_FILE)
        with open(expectations.MAIN_EXPECTATION_FILE, 'w') as f:
            f.write(header_contents)
        header = self.instance._GetExpectationFileTagHeader(
            expectations.MAIN_EXPECTATION_FILE)
        expected_header = """\
# foo
#   bar
# baz
"""
        self.assertEqual(header, expected_header)


class GetKnownTagsUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self) -> None:
        self.setUpPyfakefs()
        self.instance = expectations.WebTestExpectations()

    def testTagsLowerCased(self) -> None:
        """Tests that capitalized tags are made lower case."""
        header_contents = """\
# tags: [ Mac Win Linux ]
# results: [ Failure Skip ]
"""
        CreateFile(self.fs, expectations.MAIN_EXPECTATION_FILE)
        with open(expectations.MAIN_EXPECTATION_FILE, 'w') as f:
            f.write(header_contents)
        with mock.patch.object(
                self.instance,
                'GetExpectationFilepaths',
                return_value=[expectations.MAIN_EXPECTATION_FILE]):
            self.assertEqual(self.instance._GetKnownTags(),
                             {'mac', 'win', 'linux'})


if __name__ == '__main__':
    unittest.main(verbosity=2)
