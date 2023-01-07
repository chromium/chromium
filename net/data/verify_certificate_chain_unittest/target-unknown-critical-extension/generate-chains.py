#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the target certificate contains an unknown X.509v3
extension (OID=1.2.3.4) that is marked as critical."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate (has unknown critical extension).
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().add_property('1.2.3.4',
                                     'critical,DER:01:02:03:04')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
