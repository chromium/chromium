#!/usr/bin/env bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ ! -f 'revision.info' ]
then
  echo "File not found: revision.info" >&2
  echo "Execute pull.sh first" >&2
  exit 1
fi

dt=$(cut --fields=1 --delimiter=',' "revision.info")
revision=$(cut --fields=2 --delimiter=',' "revision.info")
sha512=$(cut --fields=3 --delimiter=',' "revision.info")

if [ -z "$dt" -o -z "$revision" -o -z "$sha512" ]
then
  echo "Incorrect format of revision.info" >&2
  echo "Execute pull.sh first" >&2
  exit 1
fi


npm --prefix src install
npm --prefix src run build

cp src/lib/iife/mapperTab.js ./mapper.js

cp README.chromium.in README.chromium
sed --in-place --regexp-extended "s/\\$\\{DATE\\}/$dt/gi" README.chromium
sed --in-place --regexp-extended "s/\\$\\{REVISION\\}/$revision/gi" README.chromium
sed --in-place --regexp-extended "s/\\$\\{TAR-SHA512\\}/$sha512/gi" README.chromium
