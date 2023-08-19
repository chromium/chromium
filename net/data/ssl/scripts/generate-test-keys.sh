#!/bin/sh

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This scripts generates a set of keys of various sizes for use in tests that
# need a key without having to pay the cost of generating one at runtime.

set -e

function genpkey {
  alg="$1"
  optname="$2"
  optval="$3"
  n="$4"
  filename="../certificates/$alg-$optval-$n.key"
  if ! grep -q -- '-----BEGIN.*PRIVATE KEY-----' "$filename" ; then
    echo "generating $filename ..."
    openssl genpkey -algorithm "$alg" -pkeyopt "$optname:$optval" \
      -out "$filename"
  else
    echo "$filename already exists, skipping"
  fi
}

for size in 768 1024 2048
do
  for i in 1 2 3
  do
    genpkey rsa rsa_keygen_bits "$size" "$i"
  done
done

genpkey rsa rsa_keygen_bits 8200 1


for curve in prime256v1
do
  for i in 1 2 3
  do
    genpkey ec ec_paramgen_curve "$curve" "$i"
  done
done
