#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the target certificate is signed using a weak RSA
key (512-bit modulus)."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate with a very weak key size (512-bit RSA).
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.set_key(gencerts.get_or_generate_rsa_key(
    512, gencerts.create_key_path(intermediate.name)))

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
