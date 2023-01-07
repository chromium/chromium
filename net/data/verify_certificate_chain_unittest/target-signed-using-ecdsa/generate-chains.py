#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the root certificate holds an RSA key, intermediate
certificate holds an EC key, and target certificate holds an RSA key. The
target certificate has a valid signature using ECDSA."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate using an RSA key.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate using an EC key for the P-384 curve.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.set_key(gencerts.get_or_generate_ec_key(
    'secp384r1', gencerts.create_key_path(intermediate.name)))

# Target certificate contains an RSA key (but is signed using ECDSA).
target = gencerts.create_end_entity_certificate('Target', intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
