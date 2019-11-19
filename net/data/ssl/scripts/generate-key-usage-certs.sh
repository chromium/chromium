#!/bin/sh

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

try openssl genrsa -out out/key_usage_rsa.key 2048
try openssl ecparam -genkey -name prime256v1 -noout -out out/key_usage_p256.key

certs=" \
  rsa_no_extension \
  rsa_keyencipherment \
  rsa_digitalsignature \
  rsa_both \
  p256_no_extension \
  p256_keyagreement \
  p256_digitalsignature \
  p256_both"
for cert in $certs; do
  key=${cert%%_*}
  SUBJECT_NAME="subj_${cert}" \
    try openssl req \
    -new \
    -key "out/key_usage_${key}.key" \
    -out "out/key_usage_${cert}.csr" \
    -config ee.cnf
  try openssl x509 \
    -req \
    -in "out/key_usage_${cert}.csr" \
    -signkey "out/key_usage_${key}.key" \
    -days 3650 \
    -extfile ee.cnf \
    -extensions "ext_${cert}" \
    -out "out/key_usage_${cert}.pem" \
    -text

  try /bin/sh -c "cat out/key_usage_${key}.key out/key_usage_${cert}.pem \
      > ../certificates/key_usage_${cert}.pem"
done

try cp "out/key_usage_rsa.key" ../certificates
try cp "out/key_usage_p256.key" ../certificates
