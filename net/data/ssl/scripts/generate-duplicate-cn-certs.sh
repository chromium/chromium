#!/bin/sh

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates two chains of test certificates:
#    1. A1 (end-entity) -> B (self-signed root)
#    2. A2 (end-entity) -> B (self-signed root)
#
# In which A1 and A2 share the same key, the same subject common name, but have
# distinct O values in their subjects.
#
# This is used to test that NSS can properly generate unique certificate
# nicknames for both certificates.

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

echo Create the serial number and index files.
try /bin/sh -c "echo 01 > out/B-serial"
try touch out/B-index.txt

echo Generate the keys.
try openssl genrsa -out out/A.key 2048
try openssl genrsa -out out/B.key 2048

echo Generate the B CSR.
CA_COMMON_NAME="B Root CA" \
  CERTIFICATE=B \
  try openssl req \
    -new \
    -key out/B.key \
    -out out/B.csr \
    -config redundant-ca.cnf

echo B signs itself.
CA_COMMON_NAME="B Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -in out/B.csr \
    -extfile redundant-ca.cnf \
    -extensions ca_cert \
    -signkey out/B.key \
    -out out/B.pem

echo Generate the A1 end-entity CSR.
SUBJECT_NAME=req_duplicate_cn_1 \
  try openssl req \
    -new \
    -key out/A.key \
    -out out/A1.csr \
    -config ee.cnf

echo Generate the A2 end-entity CSR
SUBJECT_NAME=req_duplicate_cn_2 \
  try openssl req \
    -new \
    -key out/A.key \
    -out out/A2.csr \
    -config ee.cnf


echo B signs A1.
CA_COMMON_NAME="B CA" \
  CERTIFICATE=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A1.csr \
    -out out/A1.pem \
    -config redundant-ca.cnf

echo B signs A2.
CA_COMMON_NAME="B CA" \
  CERTIFICATE=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A2.csr \
    -out out/A2.pem \
    -config redundant-ca.cnf

echo Exporting the certificates to PKCS#12
try openssl pkcs12 \
  -export \
  -inkey out/A.key \
  -in out/A1.pem \
  -out ../certificates/duplicate_cn_1.p12 \
  -passout pass:chrome

try openssl pkcs12 \
  -export \
  -inkey out/A.key \
  -in out/A2.pem \
  -out ../certificates/duplicate_cn_2.p12 \
  -passout pass:chrome

try cp out/A1.pem ../certificates/duplicate_cn_1.pem
try cp out/A2.pem ../certificates/duplicate_cn_2.pem
