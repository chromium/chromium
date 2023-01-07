#!/bin/sh

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a CA and leaf cert which can be used for the
# quic_server.

try() {
  "$@" || (e=$?; echo "$@" > /dev/stderr; exit $e)
}

try rm -rf out
try mkdir out

try /bin/sh -c "echo 01 > out/2048-sha256-root-serial"
touch out/2048-sha256-root-index.txt

# Generate the key.
try openssl genrsa -out out/2048-sha256-root.key 2048

# Generate the root certificate.
try openssl req \
  -new \
  -key out/2048-sha256-root.key \
  -out out/2048-sha256-root.req \
  -config ca.cnf

try openssl x509 \
  -req -days 3 \
  -in out/2048-sha256-root.req \
  -signkey out/2048-sha256-root.key \
  -extfile ca.cnf \
  -extensions ca_cert \
  -text > out/2048-sha256-root.pem

# Generate the leaf certificate request.
try openssl req \
  -new \
  -keyout out/leaf_cert.key \
  -out out/leaf_cert.req \
  -config leaf.cnf

# Convert the key to pkcs8.
try openssl pkcs8 \
  -topk8 \
  -outform DER \
  -inform PEM \
  -in out/leaf_cert.key \
  -out out/leaf_cert.pkcs8 \
  -nocrypt

# Generate the leaf certificate to be valid for three days.
try openssl ca \
  -batch \
  -days 3 \
  -extensions user_cert \
  -in out/leaf_cert.req \
  -out out/leaf_cert.pem \
  -config ca.cnf
