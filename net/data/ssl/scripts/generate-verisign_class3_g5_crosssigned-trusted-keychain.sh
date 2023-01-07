#!/bin/sh

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

rm -rf out
mkdir out

echo "Generating keychain"
./generate-keychain.sh \
    $PWD/out/verisign_class3_g5_crosssigned-trusted.keychain --trusted \
    "../certificates/verisign_class3_g5_crosssigned.pem"

echo "Copying outputs"
cp out/verisign_class3_g5_crosssigned-trusted.keychain ../certificates

