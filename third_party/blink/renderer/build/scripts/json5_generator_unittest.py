# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import shutil
import tempfile
import unittest

from json5_generator import Json5File, Writer


@contextlib.contextmanager
def tmp_dir():
    tmp = tempfile.mkdtemp(prefix='json5_generator_')
    try:
        yield tmp
    finally:
        shutil.rmtree(tmp)


class CleanupWriter(Writer):
    def __init__(self, output_dir, cleanup):
        super(CleanupWriter, self).__init__([], output_dir)
        self._cleanup = cleanup


class Json5FileTest(unittest.TestCase):
    def path_of_test_file(self, file_name):
        return os.path.join(
            os.path.dirname(os.path.realpath(__file__)), 'tests', file_name)

    def test_valid_dict_value_parse(self):
        actual = Json5File.load_from_files([
            self.path_of_test_file('json5_generator_valid_dict_value.json5')
        ]).name_dictionaries
        expected = [{
            'name': 'item1',
            'param1': {
                'keys': 'valid',
                'random': 'values'
            }
        }, {
            'name': 'item2',
            'param1': {
                'random': 'values',
                'default': 'valid'
            }
        }]
        self.assertEqual(len(actual), len(expected))
        for exp, act in zip(expected, actual):
            self.assertDictEqual(exp['param1'], act['param1'])

    def test_no_valid_keys(self):
        with self.assertRaises(AssertionError):
            Json5File.load_from_files([
                self.path_of_test_file('json5_generator_no_valid_keys.json5')
            ])

    def test_value_not_in_valid_values(self):
        with self.assertRaises(Exception):
            Json5File.load_from_files([
                self.path_of_test_file('json5_generator_invalid_value.json5')
            ])

    def test_key_not_in_valid_keys(self):
        with self.assertRaises(Exception):
            Json5File.load_from_files(
                [self.path_of_test_file('json5_generator_invalid_key.json5')])

    def test_cleanup_multiple_files(self):
        with tmp_dir() as tmp:
            path1 = os.path.join(tmp, 'file1.h')
            path2 = os.path.join(tmp, 'file2.h')

            with open(path1, 'wb') as f:
                f.write(b'File1')
            with open(path2, 'wb') as f:
                f.write(b'File2')

            self.assertTrue(os.path.exists(path1))
            self.assertTrue(os.path.exists(path2))
            CleanupWriter(tmp, set(['file1.h', 'file2.h'])).cleanup_files(tmp)
            self.assertFalse(os.path.exists(path1))
            self.assertFalse(os.path.exists(path2))

    def test_cleanup_partial_files(self):
        with tmp_dir() as tmp:
            path1 = os.path.join(tmp, 'file1.h')
            path2 = os.path.join(tmp, 'file2.h')

            with open(path1, 'wb') as f:
                f.write(b'File1')
            with open(path2, 'wb') as f:
                f.write(b'File2')

            self.assertTrue(os.path.exists(path1))
            self.assertTrue(os.path.exists(path2))
            CleanupWriter(tmp, set(['file2.h'])).cleanup_files(tmp)
            self.assertTrue(os.path.exists(path1))
            self.assertFalse(os.path.exists(path2))

    def test_cleanup_nonexisting(self):
        with tmp_dir() as tmp:
            path1 = os.path.join(tmp, 'file1.h')
            with open(path1, 'wb') as f:
                f.write(b'File1')
            self.assertTrue(os.path.exists(path1))
            # Don't throw when trying to clean up something that doesn't exist.
            CleanupWriter(tmp, set(['file1.h', 'file2.h'])).cleanup_files(tmp)
            self.assertFalse(os.path.exists(path1))


if __name__ == "__main__":
    unittest.main()
