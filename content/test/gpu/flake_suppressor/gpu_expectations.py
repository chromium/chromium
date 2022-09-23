# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import os
import posixpath
from typing import List
import urllib.request

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import expectations as expectations_module
import gpu_path_util

CHROMIUM_SRC_DIR = gpu_path_util.CHROMIUM_SRC_DIR
RELATIVE_EXPECTATION_FILE_DIRECTORY = os.path.join('content', 'test', 'gpu',
                                                   'gpu_tests',
                                                   'test_expectations')
ABSOLUTE_EXPECTATION_FILE_DIRECTORY = os.path.join(
    CHROMIUM_SRC_DIR, RELATIVE_EXPECTATION_FILE_DIRECTORY)

# For most test suites reported to ResultDB, we can chop off "_integration_test"
# and get the name used for the expectation file. However, there are a few
# special cases, so map those there.
EXPECTATION_FILE_OVERRIDE = {
    'info_collection_test': 'info_collection',
    'trace': 'trace_test',
}
# For whatever reason, these suites are not supported and will be skipped over
# when trying to modify expectations. However, the collected data will still be
# available in the generated results.
UNSUPPORTED_SUITES = {
    'webgpu_cts_integration_test',  # Expectation file in Dawn, not Chromium.
}


class GpuExpectationProcessor(expectations_module.ExpectationProcessor):
  def IsSuiteUnsupported(self, suite: str) -> bool:
    return suite in UNSUPPORTED_SUITES

  def GetExpectationFileListFromOrigin(self) -> List[str]:
    origin_dir = RELATIVE_EXPECTATION_FILE_DIRECTORY.replace(os.sep, '/')
    origin_dir_url = posixpath.join(
        expectations_module.GITILES_URL,
        origin_dir) + expectations_module.TEXT_FORMAT_ARG
    response = urllib.request.urlopen(origin_dir_url).read()
    # Response is a base64 encoded, newline-separated list of files in the
    # directory in the format: `mode file_type hash name`
    files = []
    decoded_text = base64.b64decode(response).decode('utf-8')
    for line in decoded_text.splitlines():
      files.append(line.split()[-1])

    efs = []
    for f in (f for f in files if f.endswith('.txt')):
      origin_file_path = posixpath.join(origin_dir, f)
      efs.append(origin_file_path)

    return efs

  def GetExpectationFileListFromLocal(self) -> List[str]:
    files = os.listdir(ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
    efs = []
    for f in (f for f in files if f.endswith('.txt')):
      efs.append(os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY, f))
    return efs

  def GetExpectationFileForSuite(self, suite: str,
                                 typ_tags: ct.TagTupleType) -> str:
    truncated_suite = suite.replace('_integration_test', '')
    if truncated_suite == 'webgl_conformance':
      if 'webgl-version-2' in typ_tags:
        truncated_suite = 'webgl2_conformance'

    expectation_file = EXPECTATION_FILE_OVERRIDE.get(truncated_suite,
                                                     truncated_suite)
    expectation_file += '_expectations.txt'
    expectation_file = os.path.join(ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                                    expectation_file)
    return expectation_file
