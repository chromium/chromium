# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import difflib
import filecmp
import os
import shutil
import tempfile
import unittest


@contextlib.contextmanager
def tmp_dir():
    tmp = tempfile.mkdtemp()
    try:
        yield tmp
    finally:
        shutil.rmtree(tmp)


def path_to_test_file(*path):
    return os.path.join(os.path.dirname(__file__), 'tests', *path)


def diff(filename1, filename2):
    with open(filename1) as file1:
        file1_lines = file1.readlines()
    with open(filename2) as file2:
        file2_lines = file2.readlines()

    # Use Python's difflib module so that diffing works across platforms
    return ''.join(difflib.context_diff(file1_lines, file2_lines))


def is_identical_file(reference_filename, output_filename):
    reference_basename = os.path.basename(reference_filename)

    if not os.path.isfile(reference_filename):
        print('Missing reference file!')
        print('(if adding new test, update reference files)')
        print(reference_basename)
        print()
        return False

    if not filecmp.cmp(reference_filename, output_filename):
        # cmp is much faster than diff, and usual case is "no difference",
        # so only run diff if cmp detects a difference
        print('FAIL: %s' % reference_basename)
        print(diff(reference_filename, output_filename))
        return False

    return True


def compare_output_dir(reference_dir, output_dir):
    """
    Compares output files in both reference_dir and output_dir.

    Note: this function ignores subdirectory content in both reference
    dir and output_dir.

    Note: reference_dir should have all ref files ending with .ref suffix.
    '.ref' suffix is added to bypass code formatter on reference files.

    :returns {bool}: Whether files in output dir matches files in ref dir
    """
    ref_content = {
        f[:-4]
        for f in os.listdir(reference_dir) if f.endswith('.ref')
    }
    output_content = set(os.listdir(output_dir))

    if ref_content != output_content:
        print('Output files does not match.')
        print('Following files are extra: {}'.format(output_content -
                                                     ref_content))
        print('Following files are missing: {}'.format(ref_content -
                                                       output_content))
        return False

    for file_name in ref_content:
        ref_file = os.path.join(reference_dir, file_name) + '.ref'
        output_file = os.path.join(output_dir, file_name)

        if os.path.isdir(ref_file) and os.path.isdir(output_file):
            continue
        elif os.path.isdir(ref_file) or os.path.isdir(output_file):
            return False
        elif not is_identical_file(ref_file, output_file):
            return False

    return True


class WriterTest(unittest.TestCase):
    def _test_writer(self, writer_class, json5_files, reference_dir):
        """
        :param writer_class {Writer}: a subclass to Writer
        :param json5_files {List[str]}: json5 test input files
        :param reference_dir {str}: directory to expected output files
        """
        with tmp_dir() as tmp:
            writer = writer_class(json5_files, tmp)
            writer.write_files(tmp)
            writer.cleanup_files(tmp)

            self.assertTrue(compare_output_dir(reference_dir, tmp))
