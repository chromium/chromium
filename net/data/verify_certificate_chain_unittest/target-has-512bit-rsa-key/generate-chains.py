#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Valid certificate chain where the target certificate contains a public key
with a 512-bit modulus (weak)."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.set_key(gencerts.get_or_generate_rsa_key(
    512, gencerts.create_key_path(target.name)))

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
