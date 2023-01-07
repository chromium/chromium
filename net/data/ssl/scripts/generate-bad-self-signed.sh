#!/bin/bash

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates self-signed-invalid-name.pem and
# self-signed-invalid-sig.pem, which are "self-signed" test certificates with
# invalid names/signatures, respectively.
set -e

 rm -rf out
 mkdir out

openssl genrsa -out out/bad-self-signed.key 2048
touch out/bad-self-signed-index.txt

# Create two certificate requests with the same key, but different subjects
SUBJECT_NAME="req_self_signed_a" \
openssl req \
  -new \
  -key out/bad-self-signed.key \
  -out out/ss-a.req \
  -config ee.cnf

SUBJECT_NAME="req_self_signed_b" \
openssl req \
  -new \
  -key out/bad-self-signed.key \
  -out out/ss-b.req \
  -config ee.cnf

# Create a normal self-signed certificate from one of these requests
openssl x509 \
  -req \
  -in out/ss-a.req \
  -out out/bad-self-signed-root-a.pem \
  -signkey out/bad-self-signed.key \
  -days 3650

# To invalidate the signature without changing names, replace two bytes from the
# end of the certificate with 0xdead.
openssl x509 -in out/bad-self-signed-root-a.pem -outform DER \
  | head -c -2 \
  > out/bad-sig.der.1
echo -n -e "\xde\xad" > out/bad-sig.der.2
cat out/bad-sig.der.1 out/bad-sig.der.2 \
  | openssl x509 \
      -inform DER \
      -outform PEM \
      -out out/cert-self-signed-invalid-sig.pem

openssl x509 \
  -text \
  -noout \
  -in out/cert-self-signed-invalid-sig.pem \
  > out/self-signed-invalid-sig.pem
cat out/cert-self-signed-invalid-sig.pem >> out/self-signed-invalid-sig.pem

# Make a "self-signed" certificate with mismatched names
openssl x509 \
  -req \
  -in out/ss-b.req \
  -out out/cert-self-signed-invalid-name.pem \
  -days 3650 \
  -CA out/bad-self-signed-root-a.pem \
  -CAkey out/bad-self-signed.key \
  -CAserial out/bad-self-signed-serial.txt \
  -CAcreateserial

openssl x509 \
  -text \
  -noout \
  -in out/cert-self-signed-invalid-name.pem \
  > out/self-signed-invalid-name.pem
cat out/cert-self-signed-invalid-name.pem >> out/self-signed-invalid-name.pem

