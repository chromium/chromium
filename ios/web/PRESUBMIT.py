# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

def CheckChangeOnUpload(input_api, output_api):
  results = []

  # Import shared iOS NFU linter
  original_sys_path = sys.path[:]
  try:
    sys.path.append(input_api.change.RepositoryRoot())
    from build.ios import presubmit_support
    results.extend(
      presubmit_support.CheckNotFatalUntilAdoption(input_api, output_api)
    )
  finally:
    sys.path = original_sys_path

  return results


def CheckChangeOnCommit(input_api, output_api):
  return CheckChangeOnUpload(input_api, output_api)
