#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate lacks a basic constraints
extension (yet is used to issue another certificate)."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate that lacks basic constraints.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.get_extensions().remove_property('basicConstraints')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
