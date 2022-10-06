#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

# An auto-created directory whose content will ultimately be uploaded to CIPD.
# So the commands below should output the built product to this directory.
PREFIX="$1"

PLATFORM="$(uname -s)"
PLATFORM=${PLATFORM,,} # lowercase

# We're simply moving things between git and CIPD here, so we don't actually
# build anything. Instead just transfer the relevant content into the output
# directory to be packaged by CIPD.
for x in $(ls "$PLATFORM-x86/${_3PP_VERSION}"); do
  mv "$PLATFORM-x86/${_3PP_VERSION}/$x" "$PREFIX/"
done

# gn has built-in ability to make a rust-project.json which can be consumed
# by rust-analyzer to add IDE comprehension of our Rust layout. It relies on
# the precise layout of Rust stdlib source code as used by the Fuchsia team.
# This is slightly different from that used by the Android team. Adjust the
# Android layout to match Fuchsia.
# (See //docs/security/rust-toolchain.md for how to actually use this.)
mkdir -p "$PREFIX/lib/rustlib/src/library"
mv "$PREFIX/src/stdlibs" "$PREFIX/lib/rustlib/src/rust"
