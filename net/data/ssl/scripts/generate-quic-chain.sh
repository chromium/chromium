#!/bin/sh

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a test chain of (end-entity, intermediate, root)
# certificates used to run a test QUIC server.

try() {
  "$@" || (e=$?; echo "$@" > /dev/stderr; exit $e)
}

try rm -rf out
try mkdir out

# Create the serial number files.
try /bin/sh -c "echo 01 > out/quic-test-root-serial"
try /bin/sh -c "echo 01 > out/quic-test-intermediate-serial"

# Create the signers' DB files.
touch out/quic-test-root-index.txt
touch out/quic-test-intermediate-index.txt

# Generate the keys
try openssl genrsa -out out/quic-test-root.key 2048
try openssl genrsa -out out/quic-test-intermediate.key 2048
try openssl genrsa -out out/quic-test-cert.key 2048

# Generate the root certificate
CA_COMMON_NAME="Test Root CA" \
  CA_DIR=out \
  CA_NAME=test-root \
  try openssl req \
    -new \
    -key out/quic-test-root.key \
    -out out/quic-test-root.csr \
    -config quic-test.cnf

CA_COMMON_NAME="Test Root CA" \
  CA_DIR=out \
  CA_NAME=quic-test-root \
  try openssl x509 \
    -req -days 3650 \
    -in out/quic-test-root.csr \
    -out out/quic-test-root.pem \
    -signkey out/quic-test-root.key \
    -extfile quic-test.cnf \
    -extensions ca_cert \
    -text

# Generate the intermediate
CA_COMMON_NAME="Test Intermediate CA" \
  CA_DIR=out \
  CA_NAME=quic-test-root \
  try openssl req \
    -new \
    -key out/quic-test-intermediate.key \
    -out out/quic-test-intermediate.csr \
    -config quic-test.cnf

CA_COMMON_NAME="Test Intermediate CA" \
  CA_DIR=out \
  CA_NAME=quic-test-root \
  try openssl ca \
    -batch \
    -in out/quic-test-intermediate.csr \
    -out out/quic-test-intermediate.pem \
    -config quic-test.cnf \
    -extensions ca_cert

# Generate the leaf
CA_COMMON_NAME="test.example.com" \
CA_DIR=out \
CA_NAME=quic-test-intermediate \
try openssl req \
  -new \
  -key out/quic-test-cert.key \
  -out out/quic-test-cert.csr \
  -config quic-test.cnf

CA_COMMON_NAME="Test Intermediate CA" \
  HOST_NAME="test.example.com" \
  CA_DIR=out \
  CA_NAME=quic-test-intermediate \
  try openssl ca \
    -batch \
    -in out/quic-test-cert.csr \
    -out out/quic-test-cert.pem \
    -config quic-test.cnf \
    -extensions user_cert

# Copy to the file names that are actually checked in.
try openssl pkcs8 -topk8 -inform pem -outform der -in out/quic-test-cert.key -out ../certificates/quic-leaf-cert.key -nocrypt
try cat out/quic-test-cert.pem out/quic-test-intermediate.pem > ../certificates/quic-chain.pem
try cp out/quic-test-root.pem ../certificates/quic-root.pem
try openssl pkcs8 -nocrypt -inform der -outform pem -in ../certificates/quic-leaf-cert.key -out ../certificates/quic-leaf-cert.key.pkcs8.pem
