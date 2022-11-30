#!/bin/bash -e

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Purpose: Create a corrupt (invalid ZIP) version of a Chromium crx.
if test $# -ne 2; then
  echo "Usage: bad_zip.sh <extension dir> <pem path>"
  exit 1
fi
dir=$1
key=$2
name=$(basename "$dir")
crx="$name.crx"
pub="$name.pub"
sig="$name.sig"
zip="$name.zip"
trap 'rm -f "$pub" "$sig" "$zip"' EXIT
# zip up the crx dir
cwd=$(pwd -P)
(cd "$dir" && zip -qr -9 -X "$cwd/$zip" .)
# truncate the zip to 8K so it's invalid
dd if=$zip of=$zip+ bs=1024 count=8 2>/dev/null && mv $zip+ $zip
# signature
openssl sha1 -sha1 -binary -sign "$key" < "$zip" > "$sig"
# public key
openssl rsa -pubout -outform DER < "$key" > "$pub" 2>/dev/null
byte_swap () {
  # Take "abcdefgh" and return it as "ghefcdab"
  echo "${1:6:2}${1:4:2}${1:2:2}${1:0:2}"
}
crmagic_hex="4372 3234" # Cr24
version_hex="0200 0000" # 2
pub_len_hex=$(byte_swap $(printf '%08x\n' $(ls -l "$pub" | awk '{print $5}')))
sig_len_hex=$(byte_swap $(printf '%08x\n' $(ls -l "$sig" | awk '{print $5}')))
(
  echo "$crmagic_hex $version_hex $pub_len_hex $sig_len_hex" | xxd -r -p
  cat "$pub" "$sig" "$zip"
) > "$crx"
echo "Wrote $crx with invalid ZIP"
