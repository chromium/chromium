#!/usr/bin/env bash

# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [[ ! -f 'revision.info' ]]
then
  echo "File not found: revision.info" >&2
  echo "Execute pull.sh first" >&2
  exit 1
fi

dt="$(cut --fields=1 --delimiter=',' "revision.info")"
revision="$(cut --fields=2 --delimiter=',' "revision.info")"
sha512="$(cut --fields=3 --delimiter=',' "revision.info")"

if [[ -z "$dt" || -z "$revision" || -z "$sha512" ]]
then
  echo "Incorrect format of revision.info" >&2
  echo "Execute pull.sh first" >&2
  exit 1
fi

npm --prefix src install
npm --prefix src run build

cp src/lib/iife/mapperTab.js ./mapper.js

sed --regexp-extended \
  --expression "s/\\$\\{DATE\\}/$dt/gi" \
  --expression "s/\\$\\{REVISION\\}/$revision/gi" \
  --expression "s/\\$\\{TAR-SHA512\\}/$sha512/gi" \
  README.chromium.in > README.chromium
