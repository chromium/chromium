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
$DEPS_PREFIX/current/bin/jar cvf "$PREFIX/lib/dx.jar" ./lib/dx/src
