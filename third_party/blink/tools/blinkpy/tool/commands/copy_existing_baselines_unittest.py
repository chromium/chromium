# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from blinkpy.tool.commands.rebaseline_unittest import BaseTestCase
from blinkpy.tool.commands.copy_existing_baselines import CopyExistingBaselines


class TestCopyExistingBaselines(BaseTestCase):
    command_constructor = CopyExistingBaselines

    def options(self, **kwargs):
        options_dict = {
            'results_directory': None,
            'suffixes': 'txt',
            'verbose': False,
            'port_name': None,
        }
        options_dict.update(kwargs)
        return optparse.Values(options_dict)

    def baseline_path(self, path_from_web_test_dir):
        port = self.tool.port_factory.get()
        return self.tool.filesystem.join(port.web_tests_dir(),
                                         path_from_web_test_dir)

    # The tests in this class all depend on the fall-back path graph
    # that is set up in |TestPort.FALLBACK_PATHS|.

    def test_copy_baseline_mac_newer_to_older_version(self):
        # The test-mac-mac10.11 baseline is copied over to the test-mac-mac10.10
        # baseline directory because test-mac-mac10.10 is the "immediate
        # predecessor" in the fall-back graph.
        self._write(
            self.baseline_path(
                'platform/test-mac-mac10.11/failures/expected/image-expected.txt'
            ), 'original test-mac-mac10.11 result')
        self.assertFalse(
            self.tool.filesystem.exists(
                self.baseline_path(
                    'platform/test-mac-mac10.10/failures/expected/image-expected.txt'
                )))

        self.command.execute(
            self.options(
                port_name='test-mac-mac10.11',
                test='failures/expected/image.html'), [], self.tool)

        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-mac-mac10.11/failures/expected/image-expected.txt'
                )), 'original test-mac-mac10.11 result')
        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-mac-mac10.10/failures/expected/image-expected.txt'
                )), 'original test-mac-mac10.11 result')

    def test_copy_baseline_to_multiple_immediate_predecessors(self):
        # The test-win-win10 baseline is copied over to the test-linux-trusty
        # and test-win-win7 baseline paths, since both of these are "immediate
        # predecessors".
        self._write(
            self.baseline_path(
                'platform/test-win-win10/failures/expected/image-expected.txt'
            ), 'original test-win-win10 result')
        self.assertFalse(
            self.tool.filesystem.exists(
                self.baseline_path(
                    'platform/test-linux-trusty/failures/expected/image-expected.txt'
                )))

        self.command.execute(
            self.options(
                port_name='test-win-win10',
                test='failures/expected/image.html'), [], self.tool)

        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-win-win10/failures/expected/image-expected.txt'
                )), 'original test-win-win10 result')
        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-linux-trusty/failures/expected/image-expected.txt'
                )), 'original test-win-win10 result')
        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-linux-trusty/failures/expected/image-expected.txt'
                )), 'original test-win-win10 result')

    def test_no_copy_existing_baseline(self):
        # If a baseline exists already for an "immediate predecessor" baseline
        # directory, (e.g. test-linux-trusty), then no "immediate successor"
        # baselines (e.g. test-win-win10) are copied over.
        self._write(
            self.baseline_path(
                'platform/test-win-win10/failures/expected/image-expected.txt'
            ), 'original test-win-win10 result')
        self._write(
            self.baseline_path(
                'platform/test-linux-trusty/failures/expected/image-expected.txt'
            ), 'original test-linux-trusty result')

        self.command.execute(
            self.options(
                port_name='test-win-win10',
                test='failures/expected/image.html'), [], self.tool)

        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-win-win10/failures/expected/image-expected.txt'
                )), 'original test-win-win10 result')
        self.assertEqual(
            self._read(
                self.baseline_path(
                    'platform/test-linux-trusty/failures/expected/image-expected.txt'
                )), 'original test-linux-trusty result')

    def test_no_copy_skipped_test(self):
        # If a test is skipped on some platform, no baselines are copied over
        # to that directory. In this example, the test is skipped on linux,
        # so the test-win-win10 baseline is not copied over.
        port = self.tool.port_factory.get('test-win-win10')
        self._write(
            self.baseline_path(
                'platform/test-win-win10/failures/expected/image-expected.txt'
            ), 'original test-win-win10 result')
        self._write(port.path_to_generic_test_expectations_file(),
                    ('# tags: [ Win Linux ]\n'
                     '# results: [ Failure Skip ]\n'
                     '[ Win ] failures/expected/image.html [ Failure ]\n'
                     '[ Linux ] failures/expected/image.html [ Skip ]\n'))

        self.command.execute(
            self.options(
                port_name='test-win-win10',
                test='failures/expected/image.html'), [], self.tool)

        self.assertFalse(
            self.tool.filesystem.exists(
                self.baseline_path(
                    'platform/test-linux-trusty/failures/expected/image-expected.txt'
                )))

    def test_port_for_primary_baseline(self):
        # Testing a protected method - pylint: disable=protected-access
        self.assertEqual(
            self.command._port_for_primary_baseline('test-linux-trusty').
            name(), 'test-linux-trusty')
        self.assertEqual(
            self.command._port_for_primary_baseline('test-mac-mac10.11').
            name(), 'test-mac-mac10.11')

    def test_port_for_primary_baseline_not_found(self):
        # Testing a protected method - pylint: disable=protected-access
        with self.assertRaises(Exception):
            self.command._port_for_primary_baseline('test-foo-foo4.7')
