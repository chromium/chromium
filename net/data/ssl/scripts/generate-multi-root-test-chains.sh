#!/bin/sh

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The following documentation uses the annotation approach from RFC 4158.
# CAs (entities that share the same name and public key) are denoted in boxes,
# while the indication that a CA Foo signed a certificate for CA Bar is denoted
# by directed arrows.
#
#   +---+    +-----+
#   | D |    |  E  |
#   +---+    +-----+
#     |       |   |
#     +--v v--+   |
#       +---+   +---+
#       | C |   | F |
#       +---+   +---+
#         |       |
#         v   v---+
#        +-----+
#        |  B  |
#        +-----+
#           |
#           v
#         +---+
#         | A |
#         +---+
#
# To validate A, there are several possible paths, using A(B) to indicate
# the certificate A signed by B:
#
# 1. A(B) -> B(C) -> C(D) -> D(D)
# 3. A(B) -> B(C) -> C(E) -> E(E)
# 4. A(B) -> B(F) -> F(E) -> E(E)
#
# That is, there are two different versions of C (signed by D and E) and
# two versions of B (signed by C and F). Possible trust anchors are D and E,
# which are both self-signed.
#
# The goal is to ensure that, as long as at least one of C or F is still valid,
# clients are able to successfully build a valid path.

# Exit script as soon a something fails.
set -e

rm -rf out
mkdir out

echo Create the serial and index number files.
for i in B C D E F
do
  openssl rand -hex -out "out/${i}-serial" 16
  touch "out/${i}-index.txt"
done

echo Generate the keys.
for i in A B C D E F
do
  openssl genrsa -out "out/${i}.key" 2048
done

echo "Generating the self-signed roots"
for i in D E
do
  echo "Generating CSR ${i}"
  CA_COMMON_NAME="${i} Root CA - Multi-root" \
  CERTIFICATE="${i}" \
  openssl req \
    -config redundant-ca.cnf \
    -new \
    -key "out/${i}.key" \
    -out "out/${i}.csr"

  echo "Generating self-signed ${i}"
  CA_COMMON_NAME="${i} Root CA - Multi-root" \
  CERTIFICATE="${i}" \
  openssl ca \
    -config redundant-ca.cnf \
    -batch \
    -startdate 160102000000Z \
    -enddate 260102000000Z \
    -extensions ca_cert \
    -extfile redundant-ca.cnf \
    -selfsign \
    -in "out/${i}.csr" \
    -out "out/${i}.pem"
done

echo "Generating intermediate CSRs"
for i in B C F
do
  echo "Generating CSR ${i}"
  CA_COMMON_NAME="${i} CA - Multi-root" \
  CERTIFICATE="${i}" \
  openssl req \
    -config redundant-ca.cnf \
    -new \
    -key "out/${i}.key" \
    -out "out/${i}.csr"
done

echo D signs C
CA_COMMON_NAME="D CA - Multi-root" \
CERTIFICATE=D \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -startdate 160103000000Z \
  -enddate 260102000000Z \
  -extensions ca_cert \
  -extfile redundant-ca.cnf \
  -in out/C.csr \
  -out out/C.pem

echo C signs B
CA_COMMON_NAME="C CA - Multi-root" \
CERTIFICATE=C \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -startdate 160104000000Z \
  -enddate 260102000000Z \
  -extensions ca_cert \
  -extfile redundant-ca.cnf \
  -in out/B.csr \
  -out out/B.pem

echo E signs C2
CA_COMMON_NAME="E CA - Multi-root" \
CERTIFICATE=E \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -startdate 160105000000Z \
  -enddate 260102000000Z \
  -extensions ca_cert \
  -extfile redundant-ca.cnf \
  -in out/C.csr \
  -out out/C2.pem

echo E signs F
CA_COMMON_NAME="E CA - Multi-root" \
CERTIFICATE=E \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -startdate 160102000000Z \
  -enddate 260102000000Z \
  -extensions ca_cert \
  -extfile redundant-ca.cnf \
  -in out/F.csr \
  -out out/F.pem

# Note: The startdate for B-by-F MUST be different than that of B-by-C; to make
# B-by-F more preferable, the startdate is chosen to be GREATER (later) than
# B-by-C.
echo F signs B2
CA_COMMON_NAME="F CA - Multi-root" \
CERTIFICATE=F \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -startdate 160105000000Z \
  -enddate 260102000000Z \
  -extensions ca_cert \
  -extfile redundant-ca.cnf \
  -in out/B.csr \
  -out out/B2.pem

echo "Generating leaf CSRs"
for i in A
do
  echo "Generating leaf ${i}"
  openssl req \
    -config ee.cnf \
    -new \
    -key "out/${i}.key" \
    -out "out/${i}.csr"
done

echo "Signing leaves"
CA_COMMON_NAME="B CA - Multi-root" \
CERTIFICATE=B \
openssl ca \
  -config redundant-ca.cnf \
  -batch \
  -days 3650 \
  -extensions user_cert \
  -extfile redundant-ca.cnf \
  -in out/A.csr \
  -out out/A.pem

echo "Copying outputs"
/bin/sh -c "cat out/A.key out/A.pem > ../certificates/multi-root-A-by-B.pem"
/bin/sh -c "cat out/A.pem out/B.pem out/C.pem out/D.pem \
    > ../certificates/multi-root-chain1.pem"
/bin/sh -c "cat out/A.pem out/B.pem out/C2.pem out/E.pem \
    > ../certificates/multi-root-chain2.pem"
cp out/B.pem ../certificates/multi-root-B-by-C.pem
cp out/B2.pem ../certificates/multi-root-B-by-F.pem
cp out/C.pem ../certificates/multi-root-C-by-D.pem
cp out/C2.pem ../certificates/multi-root-C-by-E.pem
cp out/F.pem ../certificates/multi-root-F-by-E.pem
cp out/D.pem ../certificates/multi-root-D-by-D.pem
cp out/E.pem ../certificates/multi-root-E-by-E.pem

echo "Generating CRLSets"
# Block D and E by SPKI; invalidates all paths.
python crlsetutil.py -o ../certificates/multi-root-crlset-D-and-E.raw \
<<CRLSETDOCBLOCK
{
  "BlockedBySPKI": [
    "out/D.pem",
    "out/E.pem"
  ]
}
CRLSETDOCBLOCK

# Block E by SPKI.
python crlsetutil.py -o ../certificates/multi-root-crlset-E.raw \
<<CRLSETDOCBLOCK
{
  "BlockedBySPKI": [
    "out/E.pem"
  ]
}
CRLSETDOCBLOCK

# Block C-by-D and F-by-E by way of serial number.
python crlsetutil.py -o ../certificates/multi-root-crlset-CD-and-FE.raw \
<<CRLSETDOCBLOCK
{
  "BlockedByHash": {
    "out/D.pem": ["out/C.pem"],
    "out/E.pem": ["out/F.pem"]
  }
}
CRLSETDOCBLOCK

# Block C (all versions) by way of SPKI
python crlsetutil.py -o ../certificates/multi-root-crlset-C.raw \
<<CRLSETDOCBLOCK
{
  "BlockedBySPKI": [ "out/C.pem" ]
}
CRLSETDOCBLOCK

# Block an unrelated/unissued serial (D, not issued by E) to enable all paths.
python crlsetutil.py -o ../certificates/multi-root-crlset-unrelated.raw \
<<CRLSETDOCBLOCK
{
  "BlockedByHash": {
    "out/E.pem": ["out/D.pem"]
  }
}
CRLSETDOCBLOCK
