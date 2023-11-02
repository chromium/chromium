#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 2 intermediates and one end entity certificate. The
root certificate has a pathlen:1 restriction. Ordinarily this would be an
invalid chain, however constraints on this trust anchor are not enforced."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate (used as trust anchor).
root = gencerts.create_self_signed_root_certificate('Root')
root.get_extensions().set_property('basicConstraints',
                                   'critical,CA:true,pathlen:1')

# Intermediate 1 (no pathlen restriction).
intermediate1 = gencerts.create_intermediate_certificate('Intermediate1', root)

# Intermediate 2 (no pathlen restriction).
intermediate2 = gencerts.create_intermediate_certificate('Intermediate2',
                                                       intermediate1)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate2)

chain = [target, intermediate2, intermediate1, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
