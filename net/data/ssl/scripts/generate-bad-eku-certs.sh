#!/bin/sh

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a set of test (end-entity, root) certificate chains
# whose EEs have (critical, non-critical) eKUs for codeSigning. We then try
# to use them as EEs for a web server in unit tests, to make sure that we
# don't accept such certs as web server certs.

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

eku_test_root="2048-rsa-root"

# Create the serial number files.
try /bin/sh -c "echo 01 > \"out/$eku_test_root-serial\""

# Make sure the signers' DB files exist.
touch "out/$eku_test_root-index.txt"

# Generate one root CA certificate.
try openssl genrsa -out "out/$eku_test_root.key" 2048

CA_COMMON_NAME="2048 RSA Test Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  KEY_SIZE=2048 \
  ALGO=rsa \
  CERT_TYPE=root \
  try openssl req \
    -new \
    -key "out/$eku_test_root.key" \
    -extensions ca_cert \
    -out "out/$eku_test_root.csr" \
    -config ca.cnf

CA_COMMON_NAME="2048 RSA Test Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  try openssl x509 \
    -req -days 3650 \
    -in "out/$eku_test_root.csr" \
    -extensions ca_cert \
    -extfile ca.cnf \
    -signkey "out/$eku_test_root.key" \
    -out "out/$eku_test_root.pem" \
    -text

# Generate EE certs.
for cert_type in non-crit-codeSigning crit-codeSigning
do
  try openssl genrsa -out "out/$cert_type.key" 2048

  try openssl req \
    -new \
    -key "out/$cert_type.key" \
    -out "out/$cert_type.csr" \
    -config eku-test.cnf \
    -reqexts "$cert_type"

  CA_COMMON_NAME="2048 rsa Test Root CA" \
    CA_DIR=out \
    CA_NAME=req_env_dn \
    KEY_SIZE=2048 \
    ALGO=rsa \
    CERT_TYPE=root \
    try openssl ca \
      -batch \
      -in "out/$cert_type.csr" \
      -out "out/$cert_type.pem" \
      -config ca.cnf
done

# Copy to the file names that are actually checked in.
try cp "out/$eku_test_root.pem" ../certificates/eku-test-root.pem
try /bin/sh -c "cat out/crit-codeSigning.key out/crit-codeSigning.pem \
  > ../certificates/crit-codeSigning-chain.pem"
try /bin/sh -c "cat out/non-crit-codeSigning.key out/non-crit-codeSigning.pem \
  > ../certificates/non-crit-codeSigning-chain.pem"
