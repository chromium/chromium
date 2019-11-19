#!/bin/sh

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

for cmd in gen-bundle sign-bundle; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
        exit 1
    fi
done

sxg_test_data_dir=../../../../content/test/data/sxg
signature_date=2019-07-28T00:00:00Z

gen-bundle \
  -version b1 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir hello/ \
  -manifestURL https://test.example.org/manifest.webmanifest \
  -o hello.wbn

sign-bundle \
  -i hello.wbn \
  -certificate $sxg_test_data_dir/test.example.org.public.pem.cbor \
  -privateKey $sxg_test_data_dir/prime256v1.key \
  -date $signature_date \
  -expire 168h \
  -validityUrl https://test.example.org/resource.validity.msg \
  -o hello_signed.wbn
