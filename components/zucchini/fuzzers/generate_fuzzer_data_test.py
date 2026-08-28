#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_fuzzer_data.py."""

import os
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

import generate_fuzzer_data

_CHILD_FAILURE_RETURN_CODE = 123


class GenerateFuzzerDataTest(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.temp_dir)
        self.old_file = os.path.join(self.temp_dir, 'old')
        self.new_file = os.path.join(self.temp_dir, 'new')
        self.output_file = os.path.join(self.temp_dir, 'output', 'seed.bin')
        self.tools_dir = os.path.join(self.temp_dir, 'tools')
        self.zucchini_path = os.path.join(self.tools_dir, 'zucchini')
        self.protoc_path = os.path.join(self.tools_dir, 'protoc')
        self.generated_patch_file = None

    def _generate_patch(self, command, check):
        self.assertFalse(check)
        self.generated_patch_file = command[-1]
        with open(self.generated_patch_file, 'wb') as f:
            f.write(b'patch')
        return subprocess.CompletedProcess(command, 0)

    def _assert_patch_was_cleaned(self):
        self.assertIsNotNone(self.generated_patch_file)
        self.assertFalse(
            os.path.exists(os.path.dirname(self.generated_patch_file)))

    def _set_up_cleanup_failure(self, temporary_directory):
        scratch_dir = os.path.join(self.temp_dir, 'scratch')
        os.makedirs(scratch_dir)
        temp_dir = temporary_directory.return_value
        temp_dir.name = scratch_dir
        temp_dir.cleanup.side_effect = OSError('cleanup failed')
        return temp_dir

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_generates_normal_seed(self, run, create):
        run.side_effect = self._generate_patch
        create.return_value = 0

        returncode = generate_fuzzer_data.generate_seed(
            self.zucchini_path, self.protoc_path, self.old_file, self.new_file,
            self.output_file, is_raw=False)

        run.assert_called_once_with([
            self.zucchini_path, '-gen', '--v=-1', self.old_file, self.new_file,
            self.generated_patch_file
        ], check=False)
        create.assert_called_once_with(self.protoc_path, self.old_file,
                                       self.generated_patch_file,
                                       self.output_file)
        self.assertEqual(0, returncode)
        self._assert_patch_was_cleaned()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_generates_raw_seed(self, run, create):
        run.side_effect = self._generate_patch
        create.return_value = 0

        returncode = generate_fuzzer_data.generate_seed(
            self.zucchini_path, self.protoc_path, self.old_file, self.new_file,
            self.output_file, is_raw=True)

        run.assert_called_once_with([
            self.zucchini_path, '-gen', '--v=-1', '-raw', self.old_file,
            self.new_file, self.generated_patch_file
        ], check=False)
        create.assert_called_once_with(self.protoc_path, self.old_file,
                                       self.generated_patch_file,
                                       self.output_file)
        self.assertEqual(0, returncode)
        self._assert_patch_was_cleaned()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_generation_failure_cleans_patch_and_skips_encoder(self, run,
                                                                create):
        def fail_generation(command, check):
            self._generate_patch(command, check)
            return subprocess.CompletedProcess(command,
                                               _CHILD_FAILURE_RETURN_CODE)

        run.side_effect = fail_generation
        os.makedirs(os.path.dirname(self.output_file))
        with open(self.output_file, 'wb') as f:
            f.write(b'existing seed')

        with self.assertLogs(level='ERROR'):
            returncode = generate_fuzzer_data.generate_seed(
                self.zucchini_path, self.protoc_path, self.old_file,
                self.new_file, self.output_file, is_raw=False)

        self.assertEqual(_CHILD_FAILURE_RETURN_CODE, returncode)
        create.assert_not_called()
        self._assert_patch_was_cleaned()
        with open(self.output_file, 'rb') as f:
            self.assertEqual(b'existing seed', f.read())

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_encoder_failure_returncode_and_cleanup(self, run, create):
        run.side_effect = self._generate_patch
        create.return_value = _CHILD_FAILURE_RETURN_CODE

        returncode = generate_fuzzer_data.generate_seed(
            self.zucchini_path, self.protoc_path, self.old_file, self.new_file,
            self.output_file, is_raw=False)

        self.assertEqual(_CHILD_FAILURE_RETURN_CODE, returncode)
        self._assert_patch_was_cleaned()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_generation_exception_cleans_patch(self, run, create):
        def raise_from_generation(command, check):
            self._generate_patch(command, check)
            raise OSError('generation failed')

        run.side_effect = raise_from_generation

        with self.assertRaisesRegex(OSError, 'generation failed'):
            generate_fuzzer_data.generate_seed(
                self.zucchini_path, self.protoc_path, self.old_file,
                self.new_file, self.output_file, is_raw=False)

        create.assert_not_called()
        self._assert_patch_was_cleaned()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    def test_encoder_exception_cleans_patch(self, run, create):
        run.side_effect = self._generate_patch
        create.side_effect = OSError('encoding failed')

        with self.assertRaisesRegex(OSError, 'encoding failed'):
            generate_fuzzer_data.generate_seed(
                self.zucchini_path, self.protoc_path, self.old_file,
                self.new_file, self.output_file, is_raw=False)

        self._assert_patch_was_cleaned()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    @mock.patch.object(generate_fuzzer_data.tempfile, 'TemporaryDirectory')
    def test_cleanup_failure_turns_success_into_failure(
            self, temporary_directory, run, create):
        run.side_effect = self._generate_patch
        create.return_value = 0
        temp_dir = self._set_up_cleanup_failure(temporary_directory)

        with self.assertLogs(level='ERROR'):
            returncode = generate_fuzzer_data.generate_seed(
                self.zucchini_path, self.protoc_path, self.old_file,
                self.new_file, self.output_file, is_raw=False)

        self.assertEqual(1, returncode)
        temp_dir.cleanup.assert_called_once_with()

    @mock.patch.object(generate_fuzzer_data.create_seed_file_pair,
                       'create_seed_file_pair')
    @mock.patch.object(generate_fuzzer_data.subprocess, 'run')
    @mock.patch.object(generate_fuzzer_data.tempfile, 'TemporaryDirectory')
    def test_cleanup_failure_preserves_child_returncode(
            self, temporary_directory, run, create):
        run.return_value = subprocess.CompletedProcess(
            [], _CHILD_FAILURE_RETURN_CODE)
        temp_dir = self._set_up_cleanup_failure(temporary_directory)

        with self.assertLogs(level='ERROR'):
            returncode = generate_fuzzer_data.generate_seed(
                self.zucchini_path, self.protoc_path, self.old_file,
                self.new_file, self.output_file, is_raw=False)

        self.assertEqual(_CHILD_FAILURE_RETURN_CODE, returncode)
        create.assert_not_called()
        temp_dir.cleanup.assert_called_once_with()

    @mock.patch.object(generate_fuzzer_data, 'generate_seed')
    @mock.patch.object(generate_fuzzer_data.platform, 'system')
    @mock.patch.object(generate_fuzzer_data.os, 'getcwd')
    def test_main_resolves_linux_paths_without_raw(self, getcwd, system,
                                                   generate):
        getcwd.return_value = self.tools_dir
        system.return_value = 'Linux'
        generate.return_value = 0
        output_file = os.path.abspath(self.output_file)

        returncode = generate_fuzzer_data.main(
            ['old.ztf', 'new.ztf', output_file])

        self.assertEqual(0, returncode)
        generate.assert_called_once_with(
            os.path.join(self.tools_dir, 'zucchini'),
            os.path.join(self.tools_dir, 'protoc'),
            os.path.join(generate_fuzzer_data.ABS_TESTDATA_PATH, 'old.ztf'),
            os.path.join(generate_fuzzer_data.ABS_TESTDATA_PATH, 'new.ztf'),
            output_file,
            False)

    @mock.patch.object(generate_fuzzer_data, 'generate_seed')
    @mock.patch.object(generate_fuzzer_data.platform, 'system')
    @mock.patch.object(generate_fuzzer_data.os, 'getcwd')
    def test_main_resolves_windows_paths_with_raw(self, getcwd, system,
                                                  generate):
        getcwd.return_value = self.tools_dir
        system.return_value = 'Windows'
        generate.return_value = 0
        output_file = os.path.abspath(self.output_file)

        returncode = generate_fuzzer_data.main(
            ['--raw', 'old.ztf', 'new.ztf', output_file])

        self.assertEqual(0, returncode)
        generate.assert_called_once_with(
            os.path.join(self.tools_dir, 'zucchini.exe'),
            os.path.join(self.tools_dir, 'protoc.exe'),
            os.path.join(generate_fuzzer_data.ABS_TESTDATA_PATH, 'old.ztf'),
            os.path.join(generate_fuzzer_data.ABS_TESTDATA_PATH, 'new.ztf'),
            output_file,
            True)


if __name__ == '__main__':
    unittest.main()
