#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

# The simpleperf binaries are required for some tracing utilities.
mv simpleperf "$PREFIX"
# The sysroot is required for building each platform. Retain the path used by
# the NDK for ease of use by build files that expect NDK-shaped directories.
mkdir -p "$PREFIX/toolchains/llvm/prebuilt/linux-x86_64/"
mv toolchains/llvm/prebuilt/linux-x86_64/sysroot \
  "$PREFIX/toolchains/llvm/prebuilt/linux-x86_64/"
