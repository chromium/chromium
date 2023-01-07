#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Certificate chain where the intermediate used MD5 to sign the target
certificate."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.set_signature_hash('sha1')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
