#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used by Clusterfuzz bots. For local fuzzing, including
# trying to reproduce bugs found in Clusterfuzz, use run_fuzz_test.py
# directly instead of using this script.

function main() {
  local test_path=$1
  local device_type="iPhone 14 Pro"
  local ios_version="16.1"
  local script_dir=$(dirname "$0")
  local vpython_path="/opt/vpython"
  local python_path="/opt/python3/bin"
  local cipd_path="/opt/infra-tools"
  PATH="${vpython_path}:${python_path}:${cipd_path}:${PATH}"

  # Ensure that the ~/Library/Developer/CoreSimulator directory is created.
  xcrun simctl list > /dev/null

  ${script_dir}/run_fuzz_test.py --os="${ios_version}" \
    --device="${device_type}" ${test_path}
}

main "$@"
