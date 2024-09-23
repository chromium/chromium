#!/bin/bash

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates certificates that can be used to test SSL client
# authentication. Outputs for automated tests are stored in
# net/data/ssl/certificates, but may be re-generated for manual testing.
#
# This script generates several chains of test client certificates:
#
#   1. A (end-entity) -> B -> C (self-signed root)
#   2. D (end-entity) -> E -> C (self-signed root)
#   3. F (end-entity) -> E -> C (self-signed root)
#   4. G (end-entity, P-256) -> E -> C (self-signed root)
#   5. H (end-entity, P-384) -> E -> C (self-signed root)
#   6. I (end-entity, P-521) -> E -> C (self-signed root)
#   7. J (end-entity, RSA-1024) -> E -> C (self-signed root)
#
# In which the certificates all have distinct keypairs. The client
# certificates share the same root, but are issued by different
# intermediates. The names of these intermediates are hardcoded within
# unit tests, and thus should not be changed.

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

echo Create the serial number files and indices.
serial=1000
for i in B C E
do
  try /bin/sh -c "echo $serial > out/$i-serial"
  serial=$(expr $serial + 1)
  touch out/$i-index.txt
  touch out/$i-index.txt.attr
done

echo Generate the keys.
for i in A B C D E F
do
  try openssl genrsa -out out/$i.key 2048
done

try openssl ecparam -name prime256v1 -genkey -noout -out out/G.key
try openssl ecparam -name secp384r1 -genkey -noout -out out/H.key
try openssl ecparam -name secp521r1 -genkey -noout -out out/I.key
try openssl genrsa -out out/J.key 1024

echo Generate the C CSR
COMMON_NAME="C Root CA" \
  CA_DIR=out \
  ID=C \
  try openssl req \
    -new \
    -key out/C.key \
    -out out/C.csr \
    -config client-certs.cnf

echo C signs itself.
COMMON_NAME="C Root CA" \
  CA_DIR=out \
  ID=C \
  try openssl x509 \
    -req -days 3650 \
    -in out/C.csr \
    -extensions ca_cert \
    -extfile client-certs.cnf \
    -signkey out/C.key \
    -out out/C.pem

echo Generate the intermediates
COMMON_NAME="B CA" \
  CA_DIR=out \
  ID=B \
  try openssl req \
    -new \
    -key out/B.key \
    -out out/B.csr \
    -config client-certs.cnf

COMMON_NAME="C CA" \
  CA_DIR=out \
  ID=C \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/B.csr \
    -out out/B.pem \
    -config client-certs.cnf

COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl req \
    -new \
    -key out/E.key \
    -out out/E.csr \
    -config client-certs.cnf

COMMON_NAME="C CA" \
  CA_DIR=out \
  ID=C \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/E.csr \
    -out out/E.pem \
    -config client-certs.cnf

echo Generate the leaf certs
for id in A D F G H I J
do
  COMMON_NAME="Client Cert $id" \
  ID=$id \
  try openssl req \
    -new \
    -key out/$id.key \
    -out out/$id.csr \
    -config client-certs.cnf
  # Store the private key also in PKCS#8 format.
  try openssl pkcs8 \
    -topk8 -nocrypt \
    -in out/$id.key \
    -outform DER \
    -out out/$id.pk8
done

echo B signs A
COMMON_NAME="B CA" \
  CA_DIR=out \
  ID=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A.csr \
    -out out/A.pem \
    -config client-certs.cnf

echo E signs D
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/D.csr \
    -out out/D.pem \
    -config client-certs.cnf

echo E signs F
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions san_user_cert \
    -in out/F.csr \
    -out out/F.pem \
    -config client-certs.cnf

echo E signs G
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/G.csr \
    -out out/G.pem \
    -config client-certs.cnf

echo E signs H
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/H.csr \
    -out out/H.pem \
    -config client-certs.cnf

echo E signs I
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/I.csr \
    -out out/I.pem \
    -config client-certs.cnf

echo E signs J
COMMON_NAME="E CA" \
  CA_DIR=out \
  ID=E \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/J.csr \
    -out out/J.pem \
    -config client-certs.cnf

echo Package the client certs and private keys into PKCS12 files
# This is done for easily importing all of the certs needed for clients.
try /bin/sh -c "cat out/A.pem out/A.key out/B.pem out/C.pem > out/A-chain.pem"
try /bin/sh -c "cat out/D.pem out/D.key out/E.pem out/C.pem > out/D-chain.pem"
try /bin/sh -c "cat out/F.pem out/F.key out/E.pem out/C.pem > out/F-chain.pem"
try /bin/sh -c "cat out/G.pem out/G.key out/E.pem out/C.pem > out/G-chain.pem"
try /bin/sh -c "cat out/H.pem out/H.key out/E.pem out/C.pem > out/H-chain.pem"
try /bin/sh -c "cat out/I.pem out/I.key out/E.pem out/C.pem > out/I-chain.pem"
try /bin/sh -c "cat out/J.pem out/J.key out/E.pem out/C.pem > out/J-chain.pem"

try openssl pkcs12 \
  -in out/A-chain.pem \
  -out client_1.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/D-chain.pem \
  -out client_2.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/F-chain.pem \
  -out client_3.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/G-chain.pem \
  -out client_4.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/H-chain.pem \
  -out client_5.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/I-chain.pem \
  -out client_6.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/J-chain.pem \
  -out client_7.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -inkey out/A.key \
  -in out/A.pem \
  -out out/client_1_u16_password.p12 \
  -export \
  -passout pass:"Hello, 世界"

echo Package the client certs for unit tests
try cp out/A.pem ../certificates/client_1.pem
try cp out/A.key ../certificates/client_1.key
try cp out/A.pk8 ../certificates/client_1.pk8
try cp out/B.pem ../certificates/client_1_ca.pem

try cp out/D.pem ../certificates/client_2.pem
try cp out/D.key ../certificates/client_2.key
try cp out/D.pk8 ../certificates/client_2.pk8
try cp out/E.pem ../certificates/client_2_ca.pem

try cp out/F.pem ../certificates/client_3.pem
try cp out/F.key ../certificates/client_3.key
try cp out/F.pk8 ../certificates/client_3.pk8
try cp out/E.pem ../certificates/client_3_ca.pem

try cp out/G.pem ../certificates/client_4.pem
try cp out/G.key ../certificates/client_4.key
try cp out/G.pk8 ../certificates/client_4.pk8
try cp out/E.pem ../certificates/client_4_ca.pem

try cp out/H.pem ../certificates/client_5.pem
try cp out/H.key ../certificates/client_5.key
try cp out/H.pk8 ../certificates/client_5.pk8
try cp out/E.pem ../certificates/client_5_ca.pem

try cp out/I.pem ../certificates/client_6.pem
try cp out/I.key ../certificates/client_6.key
try cp out/I.pk8 ../certificates/client_6.pk8
try cp out/E.pem ../certificates/client_6_ca.pem

try cp out/J.pem ../certificates/client_7.pem
try cp out/J.key ../certificates/client_7.key
try cp out/J.pk8 ../certificates/client_7.pk8
try cp out/E.pem ../certificates/client_7_ca.pem

try cp out/client_1_u16_password.p12 ../certificates/client_1_u16_password.p12

try cp out/C.pem ../certificates/client_root_ca.pem
