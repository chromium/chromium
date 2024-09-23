#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

mkdir "$PREFIX/lib/"
# Will be autocleaned by 3pp.
mkdir ./out
# Remove -release 11 option when crbug/1409661 is fixed.
find ./lib/dx/src -name *.java | xargs $DEPS_PREFIX/bin/javac -d ./out --release 11
$DEPS_PREFIX/bin/jar cvf "$PREFIX/lib/dx.jar" -C ./out .
