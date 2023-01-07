#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate has a policies extension (not
marked as critical) which contains an unknown policy qualifer (1.2.3.4)."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate that has a non-critical policies extension containing an unknown
# policy qualifer.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.get_extensions().add_property(
    '2.5.29.32', ('DER:30:13:30:11:06:02:2a:03:30:0b:30:09:06:03:'
                  '2a:03:04:0c:02:68:69'))

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
