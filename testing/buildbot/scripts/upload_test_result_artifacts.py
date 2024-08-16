#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Uploads test results artifacts.

This script takes a list of json test results files, the format of which is
described in
https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md.
For each file, it looks for test artifacts embedded in each test. It detects
this by looking for the top level "artifact_type_info" key.

The script, by default, uploads every artifact stored on the local disk (a URI
with the 'file' scheme) to google storage.
"""

import argparse
import copy
import json
import hashlib
import os
import shutil
import sys
import tempfile

root_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))

# //build/android imports.
sys.path.append(os.path.join(root_dir, 'build', 'android'))
from pylib.utils import google_storage_helper


def get_file_digest(filepath):
  """Get the cloud storage path for uploading a file.

  Hashes the file contents to determine the filename.
  """
  with open(filepath, 'rb') as f:
    # TODO: switch to sha256. crbug.com/787113
    m = hashlib.sha1()
    while True:
      chunk = f.read(64 * 1024)
      if not chunk:
        break
      m.update(chunk)
    return m.hexdigest()


def get_tests(test_trie):
  """Gets all tests in this test trie.

  It detects if an entry is a test by looking for the 'expected' and 'actual'
  keys in the dictionary.

  The keys of the dictionary are tuples of the keys. A test trie like
  "foo": {
    "bar": {
      "baz": {
        "actual": "PASS",
        "expected": "PASS",
      }
    }
  }

  Would give you
  {
    ('foo', 'bar', 'baz'): {
      "actual": "PASS",
      "expected": "PASS",
    }
  }

  NOTE: If you are getting an error with a stack trace ending in this function,
  file a bug with crbug.com/new and cc martiniss@.
  """
  if not isinstance(test_trie, dict):
    raise ValueError('expected %s to be a dict' % test_trie)

  tests = {}

  for k, v in test_trie.items():
    if 'expected' in v and 'actual' in v:
      tests[(k,)] = v
    else:
      for key, val in get_tests(v).items():
        tests[(k,) + key] = val

  return tests


def upload_directory_to_gs(local_path, bucket, gs_path, dry_run):
  if dry_run:
    print('would have uploaded %s to %s' % (local_path, gs_path))
    return

  # -m does multithreaded uploads, which is needed because we upload multiple
  # files. -r copies the whole directory.
  google_storage_helper.upload(
      gs_path, local_path, bucket, gs_args=['-m'], command_args=['-r'])


def hash_artifacts(tests, artifact_root):
  hashed_artifacts = []
  # Sort for testing consistency.
  for test_obj in sorted(tests.values()):
    for name, location in sorted(list(test_obj.get('artifacts', {}).items()),
                                 key=lambda pair: pair[0]):
      absolute_filepath = os.path.join(artifact_root, location)
      file_digest = get_file_digest(absolute_filepath)
      # Location is set to file digest because it's relative to the google
      # storage root.
      test_obj['artifacts'][name] = file_digest
      hashed_artifacts.append((file_digest, absolute_filepath))

  return hashed_artifacts


def prep_artifacts_for_gs_upload(hashed_artifacts, tempdir):
  for file_digest, absolute_filepath in hashed_artifacts:
    new_location = os.path.join(tempdir, file_digest)

    # Since we used content addressed hashing, the file might already exist.
    if not os.path.exists(new_location):
      shutil.copyfile(absolute_filepath, new_location)


def upload_artifacts(data, artifact_root, dry_run, bucket):
  """Uploads artifacts to google storage.

  Args:
    * data: The test results data to upload. Assumed to include 'tests' and
      'artifact_type_info' top level keys.
    * artifact_root: The local directory where artifact locations are relative
      to.
    * dry_run: If true, this run is a test run, and no actual changes should be
      made. This includes no uploading any data to cloud storage.
  Returns:
    The test results data, with rewritten artifact locations.
  """
  local_data = copy.deepcopy(data)
  type_info = local_data['artifact_type_info']

  # Put the hashing algorithm as part of the filename, so that it's
  # easier to change the algorithm if we need to in the future.
  gs_path = 'sha1'

  tests = get_tests(local_data['tests'])
  # Do a validation pass first. Makes sure no filesystem operations occur if
  # there are invalid artifacts.
  for test_obj in tests.values():
    for artifact_name in test_obj.get('artifacts', {}):
      if artifact_name not in type_info:
        raise ValueError(
            'Artifact %r type information not present' % artifact_name)

  tempdir = tempfile.mkdtemp(prefix='upload_test_artifacts')
  try:
    hashed_artifacts = hash_artifacts(tests, artifact_root)
    prep_artifacts_for_gs_upload(hashed_artifacts, tempdir)

    # Add * to include all files in that directory.
    upload_directory_to_gs(
        os.path.join(tempdir, '*'), bucket, gs_path, dry_run)

    local_data['artifact_permanent_location'] = 'gs://%s/%s' % (
        bucket, gs_path)
    return local_data
  finally:
    shutil.rmtree(tempdir)

def main():
  parser = argparse.ArgumentParser()
  # This would be test-result-file, but argparse doesn't translate
  # test-result-file to args.test_result_file automatically, and dest doesn't
  # seem to work on positional arguments.
  parser.add_argument('test_result_file')
  parser.add_argument('--output-file', type=os.path.realpath,
                      help='If set, the input json test results file will be'
                      ' rewritten to include new artifact location data, and'
                      ' dumped to this value.')
  parser.add_argument('-n', '--dry-run', action='store_true',
                      help='If true, this script will not upload any files, and'
                           ' will instead just print to stdout what path it'
                           ' would have uploaded each file. Useful for testing.'
                      )
  parser.add_argument('--artifact-root', required=True, type=os.path.realpath,
                      help='The file path where artifact locations are rooted.')
  parser.add_argument('--bucket', default='chromium-test-artifacts',
                      help='The google storage bucket to upload artifacts to.'
                      ' The default bucket is public and accessible by anyone.')
  parser.add_argument('-q', '--quiet', action='store_true',
                      help='If set, does not print the transformed json file'
                           ' to stdout.')

  args = parser.parse_args()

  with open(args.test_result_file) as f:
    data = json.load(f)

  type_info = data.get('artifact_type_info')
  if not type_info:
    print('File %r did not have %r top level key. Not processing.' %
          (args.test_result_file, 'artifact_type_info'))
    return 1

  new_data = upload_artifacts(
      data, args.artifact_root, args.dry_run, args.bucket)
  if args.output_file:
    with open(args.output_file, 'w') as f:
      json.dump(new_data, f)

  if new_data and not args.quiet:
    print(json.dumps(new_data, indent=2, separators=(',', ': '),
                     sort_keys=True))
  return 0

if __name__ == '__main__':
  sys.exit(main())
