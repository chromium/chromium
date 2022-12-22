#!/usr/bin/env python3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Certificate chain where the root keyUsage extension is present but does not
contain keyCertSign."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate without keyCertSign.
root = gencerts.create_self_signed_root_certificate('Root')
root.get_extensions().set_property('keyUsage',
                                   'critical,digitalSignature,keyEncipherment')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
