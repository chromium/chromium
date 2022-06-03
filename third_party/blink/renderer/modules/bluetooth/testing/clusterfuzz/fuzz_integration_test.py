# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test that the fuzzer works the way ClusterFuzz invokes it."""

import glob
import os
import shutil
import sys
import tempfile
import unittest

import setup


class WebBluetoothFuzzerTest(unittest.TestCase):
    def setUp(self):
        self._output_dir = tempfile.mkdtemp()
        self._resources_path = setup.RetrieveResources()

    def tearDown(self):
        shutil.rmtree(self._output_dir)
        shutil.rmtree(self._resources_path)

    def testCanGenerate100Files(self):
        sys.argv = [
            'fuzz_main_run.py', '--no_of_files=100', '--input_dir={}'.format(
                self._output_dir), '--output_dir={}'.format(self._output_dir)
        ]

        import fuzz_main_run
        fuzz_main_run.main()

        written_files = glob.glob(os.path.join(self._output_dir, '*.html'))

        self.assertEquals(100, len(written_files), 'Should have written 100 '
                          'test files.')

        for test_case in written_files:
            self.assertFalse('TRANSFORM' in open(test_case).read())


if __name__ == '__main__':
    unittest.main()
