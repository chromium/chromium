# Copyright 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Makes sure that injected JavaScript is clang-format clean."""

def CheckChangeOnUpload(input_api, output_api):
  """Special Top level function called by git_cl."""
  return input_api.canned_checks.CheckPatchFormatted(
      input_api, output_api, check_js=True)
