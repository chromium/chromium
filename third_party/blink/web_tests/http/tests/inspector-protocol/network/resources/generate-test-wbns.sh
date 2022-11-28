#!/bin/sh

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then

    echo "gen-bundle is not installed. Please run:"
    echo "  go install github.com/WICG/webpackage/go/bundle/cmd/...@latest"
    echo '  export PATH=$PATH:$(go env GOPATH)/bin'
    exit 1
fi

gen-bundle \
  -version b2 \
  -har webbundle.har \
  -primaryURL uuid-in-package:020111b3-437a-4c5c-ae07-adb6bbffb720 \
  -o webbundle.wbn
