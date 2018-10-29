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

# Generate a "secp256r1 (== prime256v1) ecdsa with sha256" key/cert pair
openssl ecparam -out prime256v1.key -name prime256v1 -genkey

openssl req -new -sha256 -key prime256v1.key -out prime256v1-sha256.csr \
  -subj '/CN=test.example.org/O=Test/C=US'

openssl x509 -req -days 360 -in prime256v1-sha256.csr \
  -CA ../../../../net/data/ssl/certificates/root_ca_cert.pem \
  -out prime256v1-sha256.public.pem -set_serial 1 \
  -extfile x509.ext

openssl x509 -req -days 360 -in prime256v1-sha256.csr \
  -CA ../../../../net/data/ssl/certificates/root_ca_cert.pem \
  -out prime256v1-sha256-noext.public.pem -set_serial 1

# Generate a "secp384r1 ecdsa with sha256" key/cert pair for negative test
openssl ecparam -out secp384r1.key -name secp384r1 -genkey

openssl req -new -sha256 -key secp384r1.key -out secp384r1-sha256.csr \
  --subj '/CN=test.example.org/O=Test/C=US'

openssl x509 -req -days 360 -in secp384r1-sha256.csr \
  -CA ../../../../net/data/ssl/certificates/root_ca_cert.pem \
  -out secp384r1-sha256.public.pem -set_serial 1

echo
echo "Update the test certs in signed_exchange_signature_verifier_unittest.cc"
echo "with the followings:"
echo "===="

echo 'constexpr char kCertPEMRSA[] = R"('
sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' \
  ../../../../net/data/ssl/certificates/wildcard.pem
echo ')";'
echo 'constexpr char kCertPEMECDSAP256[] = R"('
cat ./prime256v1-sha256.public.pem
echo ')";'
echo 'constexpr char kCertPEMECDSAP384[] = R"('
cat ./secp384r1-sha256.public.pem
echo ')";'

echo "constexpr char kPEMECDSAP256SPKIHash = "
echo "    \"$(dumpSPKIHash ./prime256v1-sha256.public.pem)\";"
echo "constexpr char kPEMECDSAP384SPKIHash = "
echo "    \"$(dumpSPKIHash ./secp384r1-sha256.public.pem)\";"

echo "===="
