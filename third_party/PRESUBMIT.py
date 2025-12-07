# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

PRESUBMIT_VERSION = '2.0.0'


def CheckThirdPartyMetadataFiles(input_api, output_api):
  """Checks that third party metadata files are correctly formatted
  and valid.
  """
  def readme_filter(f):
    local_path = f.LocalPath()

    # Limit to README.chromium files within //third_party/.
    if (not local_path.endswith('README.chromium')
        or not local_path.startswith('third_party' + input_api.os_path.sep)):
      return False

    # Some folders are currently exempt from being checked.
    skip_dirs = (
      ('third_party', 'blink'),
      ('third_party', 'boringssl'),
      ('third_party', 'closure_compiler', 'externs'),
      ('third_party', 'closure_compiler', 'interfaces'),
      ('third_party', 'feed_library'),
      ('third_party', 'ipcz'),
      ('third_party', 'jni_zero'),
      # TODO(danakj): We should look for the README.chromium file in
      # third_party/rust/CRATE_NAME/vVERSION/.
      ('third_party', 'rust'),
      ('third_party', 'webxr_test_pages'),
    )
    for path in skip_dirs:
      prefix = ''.join([dir_name + input_api.os_path.sep for dir_name in path])
      if local_path.startswith(prefix):
        return False

    return True

  return input_api.canned_checks.CheckChromiumDependencyMetadata(
      input_api, output_api, file_filter=readme_filter)
