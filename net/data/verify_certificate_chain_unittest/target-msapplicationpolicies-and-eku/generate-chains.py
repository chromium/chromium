#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Certificate chain where the target certificate contains an
MSApplicationPolicies extension that is marked as critical and
also contains an extendedKeyUsage extension."""

import sys

sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate (has unknown critical extension).
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().add_property('1.3.6.1.4.1.311.21.10',
                                     'critical,DER:01:02:03:04')
target.get_extensions().set_property('extendedKeyUsage', 'serverAuth')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
