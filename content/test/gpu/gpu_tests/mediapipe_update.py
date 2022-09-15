# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import shutil
import subprocess
import sys
import zipfile
import filecmp


def GetChromiumSrcDir() -> str:
  return os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))


_TEST_ROOT = os.path.join(GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')


def main() -> int:
  sha1_src = os.path.join(_TEST_ROOT, 'mediapipe_zip',
                          'mediapipe_chromium_tests.zip.sha1')
  sha1_dest = os.path.join(_TEST_ROOT, 'mediapipe',
                           'mediapipe_chromium_tests.zip.sha1')

  download_script = 'download_from_google_storage'
  if os.name == 'nt':
    download_script += '.bat'

  subprocess.check_call([
      download_script, '--no_resume', '--no_auth', '--bucket',
      'chromium-telemetry', '-s', sha1_src
  ])

  # Check the SHA1 of the downloaded ZIP with the one we recorded
  # previously. If we can't find the recorded SHA1, treat it like
  # it's new.
  sha1_recorded = os.path.isfile(sha1_dest)
  if not (sha1_recorded and filecmp.cmp(sha1_src, sha1_dest)):
    data_path = os.path.join(_TEST_ROOT, 'mediapipe')

    # Remove the existing unzipped tree and recreate it.
    if os.path.isdir(data_path):
      shutil.rmtree(data_path)
    os.mkdir(data_path)
    zip_file_path = os.path.join(_TEST_ROOT, 'mediapipe_zip',
                                 'mediapipe_chromium_tests.zip')
    zip_file = zipfile.ZipFile(zip_file_path)
    zip_file.extractall(data_path)
    zip_file.close()

    # Store the SHA1 we used so we can detect if a new version
    # got synced.
    shutil.copyfile(sha1_src, sha1_dest)

  return 0


if __name__ == '__main__':
  sys.exit(main())
