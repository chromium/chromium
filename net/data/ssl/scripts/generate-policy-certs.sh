#!/bin/sh

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a (end-entity, intermediate, root) certificate, where
# the root has no explicit policies associated, the intermediate has multiple
# policies, and the leaf has a single policy.
#
# When validating, supplying no policy OID should not result in an error.

try() {
  "$@" || (e=$?; echo "$@" > /dev/stderr; exit $e)
}

try rm -rf out
try mkdir out

# Create the serial number files.
try /bin/sh -c "echo 01 > out/policy-root-serial"
try /bin/sh -c "echo 01 > out/policy-intermediate-serial"

# Create the signers' DB files.
touch out/policy-root-index.txt
touch out/policy-intermediate-index.txt

# Generate the keys
try openssl genrsa -out out/policy-root.key 2048
try openssl genrsa -out out/policy-intermediate.key 2048
try openssl genrsa -out out/policy-cert.key 2048

# Generate the root certificate
COMMON_NAME="Policy Test Root CA" \
  CA_DIR=out \
  CA_NAME=policy-root \
  try openssl req \
    -new \
    -key out/policy-root.key \
    -out out/policy-root.csr \
    -config policy.cnf

COMMON_NAME="Policy Test Root CA" \
  CA_DIR=out \
  CA_NAME=policy-root \
  try openssl x509 \
    -req -days 3650 \
    -in out/policy-root.csr \
    -out out/policy-root.pem \
    -signkey out/policy-root.key \
    -extfile policy.cnf \
    -extensions ca_cert \
    -text

# Generate the intermediate
COMMON_NAME="Policy Test Intermediate CA" \
  CA_DIR=out \
  try openssl req \
    -new \
    -key out/policy-intermediate.key \
    -out out/policy-intermediate.csr \
    -config policy.cnf

COMMON_NAME="UNUSED" \
  CA_DIR=out \
  CA_NAME=policy-root \
  try openssl ca \
    -batch \
    -in out/policy-intermediate.csr \
    -out out/policy-intermediate.pem \
    -config policy.cnf \
    -extensions intermediate_cert

# Generate the leaf
COMMON_NAME="policy_test.example" \
CA_DIR=out \
CA_NAME=policy-intermediate \
try openssl req \
  -new \
  -key out/policy-cert.key \
  -out out/policy-cert.csr \
  -config policy.cnf

COMMON_NAME="Policy Test Intermediate CA" \
  SAN="policy_test.example" \
  CA_DIR=out \
  CA_NAME=policy-intermediate \
  try openssl ca \
    -batch \
    -in out/policy-cert.csr \
    -out out/policy-cert.pem \
    -config policy.cnf \
    -extensions user_cert

try /bin/sh -c "cat out/policy-cert.pem \
    out/policy-intermediate.pem \
    out/policy-root.pem >../certificates/explicit-policy-chain.pem"
