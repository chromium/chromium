#!/bin/sh
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script downloads test files used by some remoting perf tests.The files
# are stored on Google Cloud Storage.

set -e

SRC_DIR="$(readlink -f "$(dirname "$0")")"

for file_index in 1 2; do
  file_name=test_frame${file_index}.png
  file_path="${SRC_DIR}/${file_name}"
  if [ ! -e "${file_path}" ] ; then
    curl -L "https://storage.googleapis.com/chromoting-test-data/${file_name}" \
      > "${file_path}"
  fi
done
