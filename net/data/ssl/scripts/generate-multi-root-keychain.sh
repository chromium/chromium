#!/bin/sh

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

rm -rf out
mkdir out

echo "Generating keychain"
./generate-keychain.sh $PWD/out/multi-root.keychain --untrusted \
    ../certificates/multi-root*-by-*.pem

echo "Copying outputs"
cp out/multi-root.keychain ../certificates
