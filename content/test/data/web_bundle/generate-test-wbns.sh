#!/bin/sh

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go install github.com/WICG/webpackage/go/bundle/cmd/...@latest"
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

gen-bundle \
  -version b2 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir web_bundle_browsertest/ \
  -o web_bundle_browsertest_b2.wbn

# Generate a base WBN which will used to generate broken WBN.
# This WBN must contains 3 entries:
#   [1]: https://test.example.org/
#   [2]: https://test.example.org/index.html
#   [3]: https://test.example.org/script.html
gen-bundle \
  -version b2 \
  -baseURL https://test.example.org/ \
  -primaryURL https://test.example.org/ \
  -dir broken_bundle/ \
  -o broken_bundle_base_b2.wbn

# Rewrite ":status" (3a737461747573) header of the first entry to ":xxxxxx"
# (3a787878787878).
xxd -p broken_bundle_base_b2.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/' |
  xxd -r -p > broken_bundle_broken_first_entry_b2.wbn

# Rewrite ":status" (3a737461747573) header of the third entry (script.js) to
# ":xxxxxx" (3a787878787878).
xxd -p broken_bundle_base_b2.wbn |
  tr -d '\n' |
  sed 's/3a737461747573/3a787878787878/3' |
  xxd -r -p > broken_bundle_broken_script_entry_b2.wbn

gen-bundle \
  -version b2 \
  -har foo_url.har \
  -primaryURL foo://bar/ \
  -o foo_primary_url_bundle_b2.wbn

gen-bundle \
  -version b2 \
  -har foo_url.har \
  -primaryURL https://test.example.org/ \
  -o foo_base_url_bundle_b2.wbn

# Generate a WBN which will be used as a cross origin bundle.
gen-bundle \
  -version b2 \
  -har cross_origin.har \
  -o cross_origin_b2.wbn

# Generate a WBN which will be used as a same origin bundle.
gen-bundle \
  -version b2 \
  -har same_origin.har \
  -o same_origin_b2.wbn

# Generate a WBN which includes uuid-in-package resources.
gen-bundle \
  -version b2 \
  -har uuid-in-package.har \
  -o uuid-in-package.wbn

# Update Content-Length header in uuid-in-package.wbn.mock-http-headers.
(cat cross_origin_b2.wbn.mock-http-headers; \
 echo Content-Length: `wc -c <uuid-in-package.wbn`) \
    > uuid-in-package.wbn.mock-http-headers
