#!/bin/sh

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

dumpSPKIHash() {
  openssl x509 -noout -pubkey -in $1 | \
      openssl pkey -pubin -outform der | \
      openssl dgst -sha256 -binary | \
      base64
}

rm -rf out
mkdir out
/bin/sh -c "echo 01 > out/serial"
touch out/index.txt

# Generate a "secp256r1 (== prime256v1) ecdsa with sha256" key/cert pair
openssl ecparam -out prime256v1.key -name prime256v1 -genkey

openssl req -new -sha256 -key prime256v1.key -out prime256v1-sha256.csr \
  -subj '/CN=test.example.org/O=Test/C=US'

# Generate a certificate whose validity period starts at 2019-06-01 and
# valid for 90 days.
openssl ca -batch \
  -config ca.cnf \
  -extensions sxg_cert \
  -startdate 190601000000Z \
  -enddate   190830000000Z \
  -in  prime256v1-sha256.csr \
  -out prime256v1-sha256.public.pem

# Generate a certificate without CanSignHttpExchangesDraft extension.
openssl ca -batch \
  -config ca.cnf \
  -startdate 190601000000Z \
  -enddate   190830000000Z \
  -in  prime256v1-sha256.csr \
  -out prime256v1-sha256-noext.public.pem

# Generate a certificate whose validity period starts at 2019-05-01 and
# valid for 91 days.
openssl ca -batch \
  -config ca.cnf \
  -extensions sxg_cert \
  -startdate 190501000000Z \
  -enddate   190731000000Z \
  -in  prime256v1-sha256.csr \
  -out prime256v1-sha256-validity-too-long.public.pem

# Generate a certificate which is valid for 825 days. It is used in
# SignedExchangeRequestHandlerRealCertVerifierBrowserTest, where the SXG cert's
# validity period check is skipped.
openssl ca -batch \
  -config ca.cnf \
  -extensions sxg_cert \
  -days 825 \
  -in  prime256v1-sha256.csr \
  -out prime256v1-sha256-long-validity.public.pem

# Generate a "secp384r1 ecdsa with sha256" key/cert pair for negative test
openssl ecparam -out secp384r1.key -name secp384r1 -genkey

openssl req -new -sha256 -key secp384r1.key -out secp384r1-sha256.csr \
  --subj '/CN=test.example.org/O=Test/C=US'

# Generate a certificate with the secp384r1-sha256 key.
openssl ca -batch \
  -config ca.cnf \
  -extensions sxg_cert \
  -startdate 190601000000Z \
  -enddate   190830000000Z \
  -in  secp384r1-sha256.csr \
  -out secp384r1-sha256.public.pem

echo
echo "Update the test hashes in signed_exchange_test_utils.h"
echo "with the followings:"
echo "===="

echo "constexpr char kPEMECDSAP256SPKIHash[] ="
echo "    \"$(dumpSPKIHash ./prime256v1-sha256.public.pem)\";"
echo "constexpr char kPEMECDSAP384SPKIHash[] ="
echo "    \"$(dumpSPKIHash ./secp384r1-sha256.public.pem)\";"

echo "===="
