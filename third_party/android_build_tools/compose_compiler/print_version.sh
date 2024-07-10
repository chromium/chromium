#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
DIR_SRC_ROOT=$(pwd)/
DIR_SRC_ROOT=${DIR_SRC_ROOT%/src/*}/src
SUBDIR=$(cd $(dirname $0); pwd)
SUBDIR="${SUBDIR#$DIR_SRC_ROOT/}"
CIPD_PACKAGE=chromium/$SUBDIR

exec $DIR_SRC_ROOT/build/3pp_common/print_cipd_version.py \
    --subdir "$SUBDIR" \
    --cipd-package "$CIPD_PACKAGE" \
    "$@"
