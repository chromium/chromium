#!/bin/bash
# Copyright 2021 The Chromium Authors. All rights reserved.
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
mv "$PLATFORM-x86/${_3PP_VERSION}" "$PREFIX"
