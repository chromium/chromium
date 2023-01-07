#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate sets pathlen=0, however
violates this by issuing another (non-self-issued) intermediate."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate with pathlen 0
intermediate1 = gencerts.create_intermediate_certificate('Intermediate1', root)
intermediate1.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Another intermediate (with the same pathlen restriction)
intermediate2 = gencerts.create_intermediate_certificate('Intermediate2',
                                                       intermediate1)
intermediate2.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate2)

chain = [target, intermediate2, intermediate1, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
