#!/bin/bash

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates certificates that can be used to test SSL client
# authentication. Outputs for automated tests are stored in
# net/data/ssl/certificates, but may be re-generated for manual testing.
#
# This script generates several chains of test RSA client certificates:
#
#   1. A (end-entity) -> B -> C (self-signed root)
#   2. D (end-entity) -> E -> C (self-signed root)
#   3. F (end-entity) -> E -> C (self-signed root)
#
# It additionally generates several chains that exercise different key types.
#
#   - G (end-entity, P-256) -> E -> C (self-signed root)
#   - H (end-entity, P-384) -> E -> C (self-signed root)
#   - I (end-entity, P-521) -> E -> C (self-signed root)
#   - J (end-entity, RSA-1024) -> E -> C (self-signed root)
#   - K (end-entity, Ed25519) -> E -> C (self-signed root)
#   - L (end-entity, ML-DSA-44) -> E -> C (self-signed root)
#   - M (end-entity, ML-DSA-65) -> E -> C (self-signed root)
#   - N (end-entity, ML-DSA-87) -> E -> C (self-signed root)
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
serial=2000
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
try openssl genpkey -algorithm ed25519 -outform pem -out out/K.key
# OpenSSL defaults to the less common (and technically unsound) "both" format
# for ML-DSA. Override the bad defaults, needed to interoperate with other
# libraries like BoringSSL.
try openssl genpkey -provparam ml-dsa.output_formats=seed-only \
  -algorithm ML-DSA-44 -outform pem -out out/L.key
try openssl genpkey -provparam ml-dsa.output_formats=seed-only \
  -algorithm ML-DSA-65 -outform pem -out out/M.key
try openssl genpkey -provparam ml-dsa.output_formats=seed-only \
  -algorithm ML-DSA-87 -outform pem -out out/N.key

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
for id in A D F G H I J K L M N
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
    -provparam ml-dsa.output_formats=seed-only \
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

for id in G H I J K L M N
do
  echo E signs $id
  COMMON_NAME="E CA" \
    CA_DIR=out \
    ID=E \
    try openssl ca \
      -batch \
      -extensions user_cert \
      -in out/$id.csr \
      -out out/$id.pem \
      -config client-certs.cnf
done

echo Package the client certs and private keys into PKCS12 files
# This is done for easily importing all of the certs needed for clients.
try /bin/sh -c "cat out/A.pem out/A.key out/B.pem out/C.pem > out/A-chain.pem"
for id in D F G H I J K L M N
do
  try /bin/sh -c \
    "cat out/$id.pem out/$id.key out/E.pem out/C.pem > out/$id-chain.pem"
done

try openssl pkcs12 \
  -in out/A-chain.pem \
  -out out/client_1.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/D-chain.pem \
  -out out/client_2.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/F-chain.pem \
  -out out/client_3.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/G-chain.pem \
  -out out/client_p256.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/H-chain.pem \
  -out out/client_p384.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/I-chain.pem \
  -out out/client_p521.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/J-chain.pem \
  -out out/client_rsa1024.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -in out/K-chain.pem \
  -out out/client_ed25519.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -provparam ml-dsa.output_formats=seed-only \
  -in out/L-chain.pem \
  -out out/client_mldsa44.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -provparam ml-dsa.output_formats=seed-only \
  -in out/M-chain.pem \
  -out out/client_mldsa65.p12 \
  -export \
  -passout pass:chrome

try openssl pkcs12 \
  -provparam ml-dsa.output_formats=seed-only \
  -in out/N-chain.pem \
  -out out/client_mldsa87.p12 \
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

try cp out/G.pem ../certificates/client_p256.pem
try cp out/G.key ../certificates/client_p256.key
try cp out/G.pk8 ../certificates/client_p256.pk8
try cp out/E.pem ../certificates/client_p256_ca.pem

try cp out/H.pem ../certificates/client_p384.pem
try cp out/H.key ../certificates/client_p384.key
try cp out/H.pk8 ../certificates/client_p384.pk8
try cp out/E.pem ../certificates/client_p384_ca.pem

try cp out/I.pem ../certificates/client_p521.pem
try cp out/I.key ../certificates/client_p521.key
try cp out/I.pk8 ../certificates/client_p521.pk8
try cp out/E.pem ../certificates/client_p521_ca.pem

try cp out/J.pem ../certificates/client_rsa1024.pem
try cp out/J.key ../certificates/client_rsa1024.key
try cp out/J.pk8 ../certificates/client_rsa1024.pk8
try cp out/E.pem ../certificates/client_rsa1024_ca.pem

try cp out/K.pem ../certificates/client_ed25519.pem
try cp out/K.key ../certificates/client_ed25519.key
try cp out/K.pk8 ../certificates/client_ed25519.pk8
try cp out/E.pem ../certificates/client_ed25519_ca.pem

try cp out/L.pem ../certificates/client_mldsa44.pem
try cp out/L.key ../certificates/client_mldsa44.key
try cp out/L.pk8 ../certificates/client_mldsa44.pk8
try cp out/E.pem ../certificates/client_mldsa44_ca.pem

try cp out/M.pem ../certificates/client_mldsa65.pem
try cp out/M.key ../certificates/client_mldsa65.key
try cp out/M.pk8 ../certificates/client_mldsa65.pk8
try cp out/E.pem ../certificates/client_mldsa65_ca.pem

try cp out/N.pem ../certificates/client_mldsa87.pem
try cp out/N.key ../certificates/client_mldsa87.key
try cp out/N.pk8 ../certificates/client_mldsa87.pk8
try cp out/E.pem ../certificates/client_mldsa87_ca.pem

for name in 1 1_u16_password 2 3 p256 p384 p521 rsa1024 ed25519 mldsa44 mldsa65 mldsa87;
do
  try cp out/client_$name.p12 ../certificates/client_$name.p12
done

try cp out/C.pem ../certificates/client_root_ca.pem
