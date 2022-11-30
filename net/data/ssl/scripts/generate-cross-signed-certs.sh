#!/bin/sh

# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a two roots - one legacy one signed with MD5, and
# another (newer) one signed with SHA256 - and has a leaf certificate signed
# by these without any distinguishers.
#
# The "cross-signed" comes from the fact that both the MD5 and SHA256 roots
# share the same Authority Key ID, Subject Key ID, Subject, and Subject Public
# Key Info. When the chain building algorithm is evaluating paths, if it prefers
# untrusted over trusted, then it will see the MD5 certificate as a self-signed
# cert that is "cross-signed" by the trusted SHA256 root.
#
# The SHA256 root should be (temporarily) trusted, and the resulting chain
# should be leaf -> SHA256root, not leaf -> MD5root, leaf -> SHA256root ->
# MD5root, or leaf -> MD5root -> SHA256root

try() {
  "$@" || (e=$?; echo "$@" > /dev/stderr; exit $e)
}

try rm -rf out
try mkdir out

try /bin/sh -c "echo 01 > out/2048-sha256-root-serial"
try /bin/sh -c "echo 02 > out/2048-md5-root-serial"
touch out/2048-sha256-root-index.txt
touch out/2048-md5-root-index.txt

# Generate the key
try openssl genrsa -out out/2048-sha256-root.key 2048

# Generate the root certificate
CA_COMMON_NAME="Test Dup-Hash Root CA" \
  try openssl req \
    -new \
    -key out/2048-sha256-root.key \
    -out out/2048-sha256-root.req \
    -config ca.cnf

CA_COMMON_NAME="Test Dup-Hash Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -sha256 \
    -in out/2048-sha256-root.req \
    -out out/2048-sha256-root.pem \
    -text \
    -signkey out/2048-sha256-root.key \
    -extfile ca.cnf \
    -extensions ca_cert

CA_COMMON_NAME="Test Dup-Hash Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -md5 \
    -in out/2048-sha256-root.req \
    -out out/2048-md5-root.pem \
    -text \
    -signkey out/2048-sha256-root.key \
    -extfile ca.cnf \
    -extensions ca_cert

# Generate the leaf certificate request
try openssl req \
  -new \
  -keyout out/ok_cert.key \
  -out out/ok_cert.req \
  -config ee.cnf

# Generate the leaf certificates
CA_COMMON_NAME="Test Dup-Hash Root CA" \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -days 3650 \
    -in out/ok_cert.req \
    -out out/ok_cert.pem \
    -config ca.cnf

try openssl x509 -text \
    -in out/2048-md5-root.pem > ../certificates/cross-signed-root-md5.pem
try openssl x509 -text \
    -in out/2048-sha256-root.pem > ../certificates/cross-signed-root-sha256.pem
try openssl x509 -text \
    -in out/ok_cert.pem > ../certificates/cross-signed-leaf.pem
