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
#   - O (end-entity, X25519) -> E -> C (self-signed root)
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

keygen() {
  local out_path=$1; shift
  local final_path=$1; shift
  if [ -f "$final_path" ]; then
    echo "Reusing $final_path to generate $out_path"
    cp "$final_path" "$out_path"
  else
    # OpenSSL defaults to the less common (and technically unsound) "both"
    # format for ML-DSA. Override the bad defaults, needed to interoperate with
    # other libraries like BoringSSL.
    try openssl genpkey  -provparam ml-dsa.output_formats=seed-only \
      -outform pem -out "$out_path" "$@"
  fi
}

echo Generate the keys.
keygen out/A.key ../certificates/client_1.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/B.key ../certificates/client_1_ca.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/C.key ../certificates/client_root_ca.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/D.key ../certificates/client_2.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/E.key ../certificates/client_2_ca.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/F.key ../certificates/client_3.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:2048
keygen out/G.key ../certificates/client_p256.key \
  -algorithm EC -pkeyopt ec_paramgen_curve:prime256v1
keygen out/H.key ../certificates/client_p384.key \
  -algorithm EC -pkeyopt ec_paramgen_curve:secp384r1
keygen out/I.key ../certificates/client_p521.key \
  -algorithm EC -pkeyopt ec_paramgen_curve:secp521r1
keygen out/J.key ../certificates/client_rsa1024.key \
  -algorithm RSA -pkeyopt rsa_keygen_bits:1024
keygen out/K.key ../certificates/client_ed25519.key -algorithm ed25519
keygen out/L.key ../certificates/client_mldsa44.key -algorithm ML-DSA-44
keygen out/M.key ../certificates/client_mldsa65.key -algorithm ML-DSA-65
keygen out/N.key ../certificates/client_mldsa87.key -algorithm ML-DSA-87
keygen out/O.key ../certificates/client_x25519.key -algorithm x25519

echo "Store the keys also in PKCS#8 format."
for id in A D F G H I J K L M N O
do
  try openssl pkcs8 \
    -provparam ml-dsa.output_formats=seed-only \
    -topk8 -nocrypt \
    -in out/$id.key \
    -outform DER \
    -out out/$id.pk8
done

validity="-not_before 20200101000000Z -not_after 20401231235959Z"
serial=8192

echo C signs itself
COMMON_NAME="C Root CA" \
  CA_DIR=out \
  try openssl req \
    -new -x509 -text \
    $validity -set_serial $((serial++)) \
    -key out/C.key \
    -out out/C.pem \
    -extensions ca_cert \
    -config client-certs.cnf

echo Generate the intermediates
COMMON_NAME="B CA" \
  CA_DIR=out \
  try openssl req \
    -new -text \
    -CA out/C.pem -CAkey out/C.key \
    $validity -set_serial $((serial++)) \
    -key out/B.key \
    -out out/B.pem \
    -extensions ca_cert \
    -config client-certs.cnf

COMMON_NAME="E CA" \
  CA_DIR=out \
  try openssl req \
    -new -text \
    -CA out/C.pem -CAkey out/C.key \
    $validity -set_serial $((serial++)) \
    -key out/E.key \
    -out out/E.pem \
    -extensions ca_cert \
    -config client-certs.cnf

echo B signs A
COMMON_NAME="Client Cert A" \
  CA_DIR=out \
  try openssl req \
    -new -text \
    -CA out/B.pem -CAkey out/B.key \
    $validity -set_serial $((serial++)) \
    -key out/A.key \
    -out out/A.pem \
    -extensions user_cert \
    -config client-certs.cnf

echo E signs D
COMMON_NAME="Client Cert D" \
  CA_DIR=out \
  try openssl req \
    -new -text \
    -CA out/E.pem -CAkey out/E.key \
    $validity -set_serial $((serial++)) \
    -key out/D.key \
    -out out/D.pem \
    -extensions user_cert \
    -config client-certs.cnf

echo E signs F
COMMON_NAME="Client Cert F" \
  CA_DIR=out \
  try openssl req \
    -new -text \
    -CA out/E.pem -CAkey out/E.key \
    $validity -set_serial $((serial++)) \
    -key out/F.key \
    -out out/F.pem \
    -extensions san_user_cert \
    -config client-certs.cnf

for id in G H I J K L M N O
do
  echo E signs $id
  COMMON_NAME="Client Cert $id" \
    CA_DIR=out \
    try openssl req \
      -new -text \
      -CA out/E.pem -CAkey out/E.key \
      $validity -set_serial $((serial++)) \
      -key out/$id.key \
      -out out/$id.pem \
      -extensions user_cert \
      -config client-certs.cnf
done

echo Package the client certs and private keys into PKCS12 files
# This is done for easily importing all of the certs needed for clients.
try /bin/sh -c "cat out/A.pem out/A.key out/B.pem out/C.pem > out/A-chain.pem"
for id in D F G H I J K L M N O
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
  -in out/O-chain.pem \
  -out out/client_x25519.p12 \
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
try cp out/B.key ../certificates/client_1_ca.key

try cp out/D.pem ../certificates/client_2.pem
try cp out/D.key ../certificates/client_2.key
try cp out/D.pk8 ../certificates/client_2.pk8
try cp out/E.pem ../certificates/client_2_ca.pem
try cp out/E.key ../certificates/client_2_ca.key

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

try cp out/O.pem ../certificates/client_x25519.pem
try cp out/O.key ../certificates/client_x25519.key
try cp out/O.pk8 ../certificates/client_x25519.pk8
try cp out/E.pem ../certificates/client_x25519_ca.pem

for name in 1 1_u16_password 2 3 p256 p384 p521 rsa1024 ed25519 mldsa44 mldsa65 mldsa87 x25519;
do
  try cp out/client_$name.p12 ../certificates/client_$name.p12
done

try cp out/C.pem ../certificates/client_root_ca.pem
try cp out/C.key ../certificates/client_root_ca.key
