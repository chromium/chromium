#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# The commands below should output the built product to this directory.
PREFIX="$1"

# Mirrors install.sh for the macOS host toolchain. The NDK ships a single
# darwin-x86_64 host toolchain that runs on both Intel and Apple Silicon, and
# its host libraries are .dylib rather than .so.
GLOB_INCLUDES=(
  # Used for tracing utilities, see //build/android/pylib/utils/simpleperf.py.
  simpleperf
  # Used for remote debugging, include server / client binaries and libs.
  toolchains/llvm/prebuilt/darwin-x86_64/bin/lldb
  toolchains/llvm/prebuilt/darwin-x86_64/bin/lldb.sh
  toolchains/llvm/prebuilt/darwin-x86_64/lib/clang/*/lib/linux/*/lldb-server
  toolchains/llvm/prebuilt/darwin-x86_64/lib/lib*.dylib*
  toolchains/llvm/prebuilt/darwin-x86_64/lib/python*
  toolchains/llvm/prebuilt/darwin-x86_64/python3
  # Used for compilation.
  toolchains/llvm/prebuilt/darwin-x86_64/sysroot
)

# Move included files to the staging directory. macOS `cp` has no `--parents`,
# so use `rsync -R` to reproduce each pattern's relative path under PREFIX.
for pattern in "${GLOB_INCLUDES[@]}"; do
  rsync -aR $pattern "$PREFIX/"
done

# Unlike install.sh, no sysroot exclusions are needed: the macOS NDK is packaged
# on a case-insensitive filesystem, so the case-colliding headers Linux prunes
# (e.g. xt_CONNMARK.h vs xt_connmark.h) are already collapsed upstream. Removing
# the uppercase names here would delete the surviving lowercase headers.
