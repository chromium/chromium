#!/bin/sh

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a PKCS#12 (.p12) file that contains a key pair and
# two client certificates for it.

OUT_DIR=out
CLIENT_KEY_NAME=key_for_p12
CLIENT_CERT_NAME_1=cert_1_for_p12
CLIENT_CERT_NAME_2=cert_2_for_p12
P12_NAME=2_client_certs_1_key.p12

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf $OUT_DIR
try mkdir $OUT_DIR

# Generate a private key for the CA.
try openssl genrsa -out $OUT_DIR/test_ca.key 2048
# Generate a root certificate for the CA.
try openssl req -x509 -new -nodes -key $OUT_DIR/test_ca.key -sha256 -days 9999 \
    -out $OUT_DIR/test_ca.pem

# Generate a private key for the client.
openssl genrsa -out $OUT_DIR/$CLIENT_KEY_NAME.key 2048
# Convert the key into the P8 format for the client to import.
openssl pkcs8 -topk8 -inform PEM -outform DER \
    -in $OUT_DIR/$CLIENT_KEY_NAME.key \
    -out $OUT_DIR/$CLIENT_KEY_NAME.p8 -nocrypt

# Generate CSR for the first certificate.
openssl req -new -key $OUT_DIR/$CLIENT_KEY_NAME.key \
    -out $OUT_DIR/csr_1.csr
# Generate first certificate for the client.
openssl x509 -req -in $OUT_DIR/csr_1.csr \
    -CA $OUT_DIR/test_ca.pem -CAkey $OUT_DIR/test_ca.key \
    -CAcreateserial -sha256 -days 9999 \
    -out $OUT_DIR/$CLIENT_CERT_NAME_1.pem

# Generate CSR for the second certificate.
openssl req -new -key $OUT_DIR/$CLIENT_KEY_NAME.key \
    -out $OUT_DIR/csr_2.csr
# Generate second certificate for the client.
openssl x509 -req -in $OUT_DIR/csr_2.csr \
    -CA $OUT_DIR/test_ca.pem -CAkey $OUT_DIR/test_ca.key \
    -CAcreateserial -sha256 -days 9999 \
    -out $OUT_DIR/$CLIENT_CERT_NAME_2.pem

# Generate a PKCS#12 file (.p12) from the two certs and the key.
openssl pkcs12 -export \
    -out $OUT_DIR/$P12_NAME \
    -inkey $OUT_DIR/$CLIENT_KEY_NAME.key \
    -in $OUT_DIR/$CLIENT_CERT_NAME_1.pem \
    -certfile $OUT_DIR/$CLIENT_CERT_NAME_2.pem \
    -passout pass:12345

try cp $OUT_DIR/$P12_NAME ../certificates
