#!/usr/bin/env bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Exit if any command fails
set -e

revision="$1"

if [ -z "$revision" ]
then
  echo "Usage: $0 git-revision-full-sha1" >&2
  exit 1
fi

[ -f "chromium-bidi.zip" ] && rm "chromium-bidi.zip"
[ -d "src" ] && rm --recursive --force "src"


wget --output-document="chromium-bidi.zip" "https://github.com/GoogleChromeLabs/chromium-bidi/archive/$revision.zip"
unzip "chromium-bidi.zip"
mv "chromium-bidi-$revision" src

date=$(date "+%Y-%m-%d")
sha512=$(sha512sum --binary "chromium-bidi.zip" | cut --fields=1 --delimiter=' ')
rm "chromium-bidi.zip"
echo "$date,$revision,$sha512" > "revision.info"

