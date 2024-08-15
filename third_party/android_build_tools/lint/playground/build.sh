#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Usage:
# Make edits to *.java
# Then run: ./build.sh | less

set -xe
cd $(dirname $0)

# Returns the first parameter after ensuring the path exists.
function get_path() {
  if [[ ! -f $1 ]]; then
    >&2 echo "Pattern matched no files: $1"
    exit 1
  fi
  echo "$1"
}

LINT_PATH=$(get_path ../cipd/lint.jar)
JAVA_PATH=$(get_path ../../../jdk/current/bin/java)

$JAVA_PATH -cp $LINT_PATH com.android.tools.lint.Main --project ./project.xml
