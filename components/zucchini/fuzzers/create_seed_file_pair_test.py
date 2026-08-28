#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for create_seed_file_pair.py."""

import os
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

import create_seed_file_pair
from google.protobuf import descriptor_pb2
from google.protobuf import descriptor_pool
from google.protobuf import message_factory
from google.protobuf import text_format


def _create_file_pair_class():
    """Creates a dynamic message class matching file_pair.proto."""
    required = descriptor_pb2.FieldDescriptorProto.LABEL_REQUIRED
    optional = descriptor_pb2.FieldDescriptorProto.LABEL_OPTIONAL
    bytes_type = descriptor_pb2.FieldDescriptorProto.TYPE_BYTES
    string_type = descriptor_pb2.FieldDescriptorProto.TYPE_STRING
    file_descriptor = descriptor_pb2.FileDescriptorProto(
        name='file_pair_test.proto',
        package='zucchini.fuzzers',
        syntax='proto2',
        message_type=[
            descriptor_pb2.DescriptorProto(
                name='FilePair',
                field=[
                    descriptor_pb2.FieldDescriptorProto(
                        name='old_file',
                        number=1,
                        label=required,
                        type=bytes_type),
                    descriptor_pb2.FieldDescriptorProto(
                        name='new_or_patch_file',
                        number=2,
                        label=required,
                        type=bytes_type),
                    descriptor_pb2.FieldDescriptorProto(
                        name='imposed_matches',
                        number=3,
                        label=optional,
                        type=string_type),
                ])
        ])
    pool = descriptor_pool.DescriptorPool()
    descriptor = pool.AddSerializedFile(file_descriptor.SerializeToString())
    message_descriptor = descriptor.message_types_by_name['FilePair']
    return message_factory.GetMessageClass(message_descriptor)


class CreateSeedFilePairTest(unittest.TestCase):

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.temp_dir)
        self.old_file = os.path.join(self.temp_dir, 'old')
        # self.new_file supplies FilePair.new_or_patch_file: a new target for
        # Gen seeds or a pre-generated patch for Apply seeds.
        self.new_file = os.path.join(self.temp_dir, 'new')
        self.old_payload = b'\x001"\\\xff'
        self.new_payload = b'\nA'
        with open(self.old_file, 'wb') as f:
            f.write(self.old_payload)
        with open(self.new_file, 'wb') as f:
            f.write(self.new_payload)

    def _expected_proto_text(self):
        # CEscape(as_utf8=False) formats each non-ASCII byte as \%03o: 0xff
        # becomes \377, and UTF-8 'é' (0xc3, 0xa9) becomes \303\251.
        old_escape = b'\\000' + b'1' + b'\\"' + b'\\\\' + b'\\377'
        imposed_escape = b'\\"' + b'\\\\' + b'\\303' + b'\\251'
        return b'\n'.join([
            b'old_file: "' + old_escape + b'"',
            b'new_or_patch_file: "\\nA"',
            b'imposed_matches: "' + imposed_escape + b'"',
        ])

    def test_proto_escape_uses_protobuf_text_format(self):
        old_escape = b'\\000' + b'1' + b'\\"' + b'\\\\' + b'\\377'
        imposed_escape = b'\\"' + b'\\\\' + b'\\303' + b'\\251'
        self.assertEqual(old_escape,
                         create_seed_file_pair.proto_escape(self.old_payload))
        self.assertEqual(
            imposed_escape,
            create_seed_file_pair.proto_escape('"\\é'.encode('utf-8')))

    def test_builds_file_pair_text(self):
        self.assertEqual(
            self._expected_proto_text(),
            create_seed_file_pair.build_file_pair_text(
                self.old_file, self.new_file, '"\\é'))

    def test_protobuf_text_and_binary_round_trip(self):
        old_payload = bytes(range(256))
        new_payload = b'\\"\x00\n\xff01234567abcdefABCDEF'
        imposed_matches = '1+2=3+4,5+6=7+8 é'
        with open(self.old_file, 'wb') as f:
            f.write(old_payload)
        with open(self.new_file, 'wb') as f:
            f.write(new_payload)

        # Presubmit does not guarantee that protoc has been built. Use the
        # bundled runtime to parse both formats instead.
        file_pair_class = _create_file_pair_class()
        text_message = file_pair_class()
        text_format.Parse(
            create_seed_file_pair.build_file_pair_text(
                self.old_file, self.new_file,
                imposed_matches).decode('ascii'), text_message)
        binary_message = file_pair_class()
        binary_message.ParseFromString(text_message.SerializeToString())

        self.assertEqual(old_payload, binary_message.old_file)
        self.assertEqual(new_payload, binary_message.new_or_patch_file)
        self.assertEqual(imposed_matches, binary_message.imposed_matches)

    @mock.patch.object(create_seed_file_pair.subprocess, 'run')
    def test_encodes_binary_content_and_imposed_matches(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=b'encoded seed')
        output_file = os.path.join(self.temp_dir, 'nested', 'seed.bin')

        returncode = create_seed_file_pair.create_seed_file_pair(
            '/build/protoc', self.old_file, self.new_file, output_file,
            '"\\é')

        run.assert_called_once_with(
            [
                '/build/protoc',
                '--proto_path=%s' % create_seed_file_pair.ABS_PATH,
                '--encode=zucchini.fuzzers.FilePair',
                os.path.join(create_seed_file_pair.ABS_PATH,
                             'file_pair.proto'),
            ],
            input=self._expected_proto_text(),
            stdout=subprocess.PIPE,
            check=False)
        self.assertEqual(0, returncode)
        with open(output_file, 'rb') as f:
            self.assertEqual(b'encoded seed', f.read())

    @mock.patch.object(create_seed_file_pair.subprocess, 'run')
    def test_protoc_failure_preserves_existing_output(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[], returncode=23, stdout=b'invalid output')
        output_file = os.path.join(self.temp_dir, 'seed.bin')
        with open(output_file, 'wb') as f:
            f.write(b'existing seed')

        with self.assertLogs(level='ERROR'):
            returncode = create_seed_file_pair.create_seed_file_pair(
                '/build/protoc', self.old_file, self.new_file, output_file)

        self.assertEqual(23, returncode)
        with open(output_file, 'rb') as f:
            self.assertEqual(b'existing seed', f.read())

    @mock.patch.object(create_seed_file_pair.subprocess, 'run')
    def test_protoc_failure_does_not_create_output(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[], returncode=24, stdout=b'invalid output')
        output_file = os.path.join(self.temp_dir, 'missing', 'seed.bin')

        with self.assertLogs(level='ERROR'):
            returncode = create_seed_file_pair.create_seed_file_pair(
                '/build/protoc', self.old_file, self.new_file, output_file)

        self.assertEqual(24, returncode)
        self.assertFalse(os.path.exists(output_file))

    @mock.patch.object(create_seed_file_pair.subprocess, 'run')
    def test_output_write_failure_preserves_existing_output(self, run):
        run.return_value = subprocess.CompletedProcess(
            args=[], returncode=0, stdout=b'encoded seed')
        output_file = os.path.join(self.temp_dir, 'seed.bin')
        with open(output_file, 'wb') as f:
            f.write(b'existing seed')

        with mock.patch.object(create_seed_file_pair.action_helpers,
                               'atomic_output') as atomic_output:
            writer = atomic_output.return_value.__enter__.return_value
            writer.write.side_effect = OSError('write failed')
            with self.assertRaisesRegex(OSError, 'write failed'):
                create_seed_file_pair.create_seed_file_pair(
                    '/build/protoc', self.old_file, self.new_file, output_file)

        atomic_output.assert_called_once_with(output_file)
        with open(output_file, 'rb') as f:
            self.assertEqual(b'existing seed', f.read())

    @mock.patch.object(create_seed_file_pair, 'create_seed_file_pair')
    def test_main_accepts_explicit_arguments(self, create):
        create.return_value = 7

        returncode = create_seed_file_pair.main([
            '--imposed_matches', '1+2=3+4', '/build/protoc', self.old_file,
            self.new_file, '/output/seed.bin'
        ])

        self.assertEqual(7, returncode)
        create.assert_called_once_with('/build/protoc', self.old_file,
                                       self.new_file, '/output/seed.bin',
                                       '1+2=3+4')


if __name__ == '__main__':
    unittest.main()
