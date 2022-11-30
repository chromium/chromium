#!/bin/sh

# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a set of test (end-entity, intermediate, root)
# certificates with (weak, strong), (RSA, DSA, ECDSA) key pairs.

key_types="768-rsa 1024-rsa 2048-rsa prime256v1-ecdsa"

try () {
  echo "$@"
  "$@" || exit 1
}

generate_key_command () {
  case "$1" in
    dsa)
      echo "dsaparam -genkey"
      ;;
    ecdsa)
      echo "ecparam -genkey"
      ;;
    rsa)
      echo genrsa
      ;;
    *)
      exit 1
  esac
}

try rm -rf out
try mkdir out

# Create the serial number files.
try /bin/sh -c "echo 01 > out/2048-rsa-root-serial"
for key_type in $key_types
do
  try /bin/sh -c "echo 01 > out/$key_type-intermediate-serial"
done

# Generate one root CA certificate.
try openssl genrsa -out out/2048-rsa-root.key 2048

CA_COMMON_NAME="2048 RSA Test Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  KEY_SIZE=2048 \
  ALGO=rsa \
  CERT_TYPE=root \
  try openssl req \
    -new \
    -key out/2048-rsa-root.key \
    -extensions ca_cert \
    -out out/2048-rsa-root.csr \
    -config ca.cnf

CA_COMMON_NAME="2048 RSA Test Root CA" \
  CA_DIR=out \
  CA_NAME=req_env_dn \
  try openssl x509 \
    -req -days 3650 \
    -in out/2048-rsa-root.csr \
    -extensions ca_cert \
    -extfile ca.cnf \
    -signkey out/2048-rsa-root.key \
    -out out/2048-rsa-root.pem \
    -text

# Generate private keys of all types and strengths for intermediate CAs and
# end-entities.
for key_type in $key_types
do
  key_size=$(echo "$key_type" | sed -E 's/-.+//')
  algo=$(echo "$key_type" | sed -E 's/.+-//')

  if [ ecdsa = $algo ]
  then
    key_size="-name $key_size"
  fi

  try openssl $(generate_key_command $algo) \
    -out out/$key_type-intermediate.key $key_size
done

for key_type in $key_types
do
  key_size=$(echo "$key_type" | sed -E 's/-.+//')
  algo=$(echo "$key_type" | sed -E 's/.+-//')

  if [ ecdsa = $algo ]
  then
    key_size="-name $key_size"
  fi

  for signer_key_type in $key_types
  do
    try openssl $(generate_key_command $algo) \
      -out out/$key_type-ee-by-$signer_key_type-intermediate.key $key_size
  done
done

# The root signs the intermediates.
for key_type in $key_types
do
  key_size=$(echo "$key_type" | sed -E 's/-.+//')
  algo=$(echo "$key_type" | sed -E 's/.+-//')

  CA_COMMON_NAME="$key_size $algo Test intermediate CA" \
    CA_DIR=out \
    CA_NAME=req_env_dn \
    KEY_SIZE=$key_size \
    ALGO=$algo \
    CERT_TYPE=intermediate \
    try openssl req \
      -new \
      -key out/$key_type-intermediate.key \
      -out out/$key_type-intermediate.csr \
      -config ca.cnf

  # Make sure the signer's DB file exists.
  touch out/2048-rsa-root-index.txt

  CA_COMMON_NAME="2048 RSA Test Root CA" \
    CA_DIR=out \
    CA_NAME=req_env_dn \
    KEY_SIZE=2048 \
    ALGO=rsa \
    CERT_TYPE=root \
    try openssl ca \
      -batch \
      -extensions ca_cert \
      -in out/$key_type-intermediate.csr \
      -out out/$key_type-intermediate.pem \
      -config ca.cnf
done

# The intermediates sign the end-entities.
for key_type in $key_types
do
  for signer_key_type in $key_types
  do
    key_size=$(echo "$key_type" | sed -E 's/-.+//')
    algo=$(echo "$key_type" | sed -E 's/.+-//')
    signer_key_size=$(echo "$signer_key_type" | sed -E 's/-.+//')
    signer_algo=$(echo "$signer_key_type" | sed -E 's/.+-//')
    touch out/$signer_key_type-intermediate-index.txt

    KEY_SIZE=$key_size \
      try openssl req \
        -new \
        -key out/$key_type-ee-by-$signer_key_type-intermediate.key \
        -out out/$key_type-ee-by-$signer_key_type-intermediate.csr \
        -config ee.cnf

    CA_COMMON_NAME="$signer_key_size $algo Test intermediate CA" \
      CA_DIR=out \
      CA_NAME=req_env_dn \
      KEY_SIZE=$signer_key_size \
      ALGO=$signer_algo \
      CERT_TYPE=intermediate \
      try openssl ca \
        -batch \
        -extensions user_cert \
        -in out/$key_type-ee-by-$signer_key_type-intermediate.csr \
        -out out/$key_type-ee-by-$signer_key_type-intermediate.pem \
        -config ca.cnf
  done
done

# Copy final outputs.
try cp out/*root*pem out/*intermediate*pem ../certificates
