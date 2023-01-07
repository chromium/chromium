#!/bin/sh

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

rm -rf out
mkdir out

echo "Generating keychain"
./generate-keychain.sh $PWD/out/multi-root-BFE.keychain --untrusted \
    "../certificates/multi-root-B-by-F.pem" \
    "../certificates/multi-root-F-by-E.pem"

echo "Copying outputs"
cp out/multi-root-BFE.keychain ../certificates
