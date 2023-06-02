#!/bin/sh

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go install github.com/WICG/webpackage/go/bundle/cmd/...@latest"
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

gen-bundle -version b2 -har auction_only.har -o auction_only.wbn
gen-bundle -version b2 -har auction_only_new_name.har \
  -o auction_only_new_name.wbn
gen-bundle -version b2 -har auction_only_both_new_and_old_names.har \
  -o auction_only_both_new_and_old_names.wbn
