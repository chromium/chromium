#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -x
set -o pipefail

PREFIX="$1"
FILENAME=$(ls *.jar)

unzip $FILENAME aapt2 -d "$PREFIX"/
