# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for upload_test_result_artifacts."""

from __future__ import print_function

import json
import mock
import os
import random
import string
import tempfile
import unittest

import upload_test_result_artifacts


class UploadTestResultArtifactsTest(unittest.TestCase):
  def setUp(self):
    # Used for load tests
    self._temp_files = []

  def tearDown(self):
    # Used for load tests
    for fname in self._temp_files:
      os.unlink(fname)

  ### These are load tests useful for seeing how long it takes to upload
  ### different kinds of test results files. They won't be run as part of
  ### presubmit testing, since they take a while and talk to the network,
  ### but the code will stay here in case anyone wants to edit the code
  ### and wants to check performance. Change the test names from 'loadTestBlah'
  ### to 'testBlah' to get them to run.

  def makeTemp(self, size):
    _, fname = tempfile.mkstemp()
    with open(fname, 'w') as f:
      f.write(random.choice(string.ascii_letters) * size)
    self._temp_files.append(fname)

    return os.path.basename(fname)

  def makeTestJson(self, num_tests, artifact_size):
    return {
      'tests': {
        'suite': {
          'test%d' % i: {
            'artifacts': {
              'artifact': self.makeTemp(artifact_size),
            },
            'expected': 'PASS',
            'actual': 'PASS',
          } for i in range(num_tests)
        }
      },
      'artifact_type_info': {
        'artifact': 'text/plain'
      }
    }

  def _loadTest(self, json_data, upload):
    return upload_test_result_artifacts.upload_artifacts(
        json_data, '/tmp', upload, 'test-bucket')


  def loadTestEndToEndSimple(self):
    test_data = self.makeTestJson(1, 10)
    print(self._loadTest(test_data, False))

  def loadTestEndToEndManySmall(self):
    test_data = self.makeTestJson(1000, 10)
    self._loadTest(test_data, False)

  def loadTestEndToEndSomeBig(self):
    test_data = self.makeTestJson(100, 10000000)
    self._loadTest(test_data, False)

  def loadTestEndToEndVeryBig(self):
    test_data = self.makeTestJson(2, 1000000000)
    self._loadTest(test_data, False)

  ### End load test section.

  def testGetTestsSimple(self):
    self.assertEqual(upload_test_result_artifacts.get_tests({
      'foo': {
        'expected': 'PASS',
        'actual': 'PASS',
      },
    }), {
      ('foo',): {
          'actual': 'PASS',
          'expected': 'PASS',
      }
    })

  def testGetTestsNested(self):
    self.assertEqual(upload_test_result_artifacts.get_tests({
      'foo': {
        'bar': {
          'baz': {
            'actual': 'PASS',
            'expected': 'PASS',
          },
          'bam': {
            'actual': 'PASS',
            'expected': 'PASS',
          },
        },
      },
    }), {
      ('foo', 'bar', 'baz'): {
          'actual': 'PASS',
          'expected': 'PASS',
      },
      ('foo', 'bar', 'bam'): {
          'actual': 'PASS',
          'expected': 'PASS',
      }
    })

  def testGetTestsError(self):
    with self.assertRaises(ValueError):
      upload_test_result_artifacts.get_tests([])

  def testUploadArtifactsMissingType(self):
    """Tests that the type information is used for validation."""
    data = {
        'artifact_type_info': {
            'log': 'text/plain'
        },
        'tests': {
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
            'artifacts': {
                'screenshot': 'foo.png',
            }
          }
        }
    }
    with self.assertRaises(ValueError):
      upload_test_result_artifacts.upload_artifacts(
          data, '/tmp', True, 'test-bucket')

  @mock.patch('upload_test_result_artifacts.get_file_digest')
  @mock.patch('upload_test_result_artifacts.tempfile.mkdtemp')
  @mock.patch('upload_test_result_artifacts.shutil.rmtree')
  @mock.patch('upload_test_result_artifacts.shutil.copyfile')
  def testUploadArtifactsNoUpload(
      self, copy_patch, rmtree_patch, mkd_patch, digest_patch):
    """Simple test; no artifacts, so data shouldn't change."""
    mkd_patch.return_value = 'foo_dir'
    data = {
        'artifact_type_info': {
            'log': 'text/plain'
        },
        'tests': {
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
          }
        }
    }
    self.assertEqual(upload_test_result_artifacts.upload_artifacts(
        data, '/tmp', True, 'test-bucket'), data)
    mkd_patch.assert_called_once_with(prefix='upload_test_artifacts')
    digest_patch.assert_not_called()
    copy_patch.assert_not_called()
    rmtree_patch.assert_called_once_with('foo_dir')

  @mock.patch('upload_test_result_artifacts.get_file_digest')
  @mock.patch('upload_test_result_artifacts.tempfile.mkdtemp')
  @mock.patch('upload_test_result_artifacts.shutil.rmtree')
  @mock.patch('upload_test_result_artifacts.shutil.copyfile')
  @mock.patch('upload_test_result_artifacts.os.path.exists')
  def testUploadArtifactsBasic(
      self, exists_patch, copy_patch, rmtree_patch, mkd_patch, digest_patch):
    """Upload a single artifact."""
    mkd_patch.return_value = 'foo_dir'
    exists_patch.return_value = False
    digest_patch.return_value = 'deadbeef'

    data = {
        'artifact_type_info': {
            'log': 'text/plain'
        },
        'tests': {
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
            'artifacts': {
                'log': 'foo.txt',
            }
          }
        }
    }
    self.assertEqual(upload_test_result_artifacts.upload_artifacts(
        data, '/tmp', True, 'test-bucket'), {
        'artifact_type_info': {
            'log': 'text/plain'
        },
        'tests': {
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
            'artifacts': {
                'log': 'deadbeef',
            }
          }
        },
        'artifact_permanent_location': 'gs://chromium-test-artifacts/sha1',
    })
    mkd_patch.assert_called_once_with(prefix='upload_test_artifacts')
    digest_patch.assert_called_once_with('/tmp/foo.txt')
    copy_patch.assert_called_once_with('/tmp/foo.txt', 'foo_dir/deadbeef')
    rmtree_patch.assert_called_once_with('foo_dir')

  @mock.patch('upload_test_result_artifacts.get_file_digest')
  @mock.patch('upload_test_result_artifacts.tempfile.mkdtemp')
  @mock.patch('upload_test_result_artifacts.shutil.rmtree')
  @mock.patch('upload_test_result_artifacts.shutil.copyfile')
  @mock.patch('upload_test_result_artifacts.os.path.exists')
  def testUploadArtifactsComplex(
      self, exists_patch, copy_patch, rmtree_patch, mkd_patch, digest_patch):
    """Upload multiple artifacts."""
    mkd_patch.return_value = 'foo_dir'
    exists_patch.return_value = False
    digest_patch.side_effect = [
        'deadbeef1', 'deadbeef2', 'deadbeef3', 'deadbeef4']

    data = {
        'artifact_type_info': {
            'log': 'text/plain',
            'screenshot': 'image/png',
        },
        'tests': {
          'bar': {
            'baz': {
              'actual': 'PASS',
              'expected': 'PASS',
              'artifacts': {
                  'log': 'baz.log.txt',
                  'screenshot': 'baz.png',
              }
            }
          },
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
            'artifacts': {
                'log': 'foo.log.txt',
                'screenshot': 'foo.png',
            }
          },
        }
    }
    self.assertEqual(upload_test_result_artifacts.upload_artifacts(
        data, '/tmp', True, 'test-bucket'), {
        'artifact_type_info': {
            'log': 'text/plain',
            'screenshot': 'image/png',
        },
        'tests': {
          'bar': {
            'baz': {
              'actual': 'PASS',
              'expected': 'PASS',
              'artifacts': {
                  'log': 'deadbeef1',
                  'screenshot': 'deadbeef2',
              }
            }
          },
          'foo': {
            'actual': 'PASS',
            'expected': 'PASS',
            'artifacts': {
                'log': 'deadbeef3',
                'screenshot': 'deadbeef4',
            }
          },
        },
        'artifact_permanent_location': 'gs://chromium-test-artifacts/sha1',
    })
    mkd_patch.assert_called_once_with(prefix='upload_test_artifacts')
    digest_patch.assert_has_calls([
        mock.call('/tmp/baz.log.txt'), mock.call('/tmp/baz.png'),
        mock.call('/tmp/foo.log.txt'), mock.call('/tmp/foo.png')])
    copy_patch.assert_has_calls([
        mock.call('/tmp/baz.log.txt', 'foo_dir/deadbeef1'),
        mock.call('/tmp/baz.png', 'foo_dir/deadbeef2'),
        mock.call('/tmp/foo.log.txt', 'foo_dir/deadbeef3'),
        mock.call('/tmp/foo.png', 'foo_dir/deadbeef4'),
    ])
    rmtree_patch.assert_called_once_with('foo_dir')

  def testFileDigest(self):
    _, path = tempfile.mkstemp(prefix='file_digest_test')
    with open(path, 'w') as f:
      f.write('a')

    self.assertEqual(
        upload_test_result_artifacts.get_file_digest(path),
        '86f7e437faa5a7fce15d1ddcb9eaeaea377667b8')

if __name__ == '__main__':
  unittest.main()
