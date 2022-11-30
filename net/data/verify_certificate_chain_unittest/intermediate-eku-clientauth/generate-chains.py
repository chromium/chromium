#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate restricts the extended key usage to
clientAuth, and the target asserts serverAuth + clientAuth."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.get_extensions().set_property('extendedKeyUsage',
                                           'clientAuth')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().set_property('extendedKeyUsage',
                                     'serverAuth,clientAuth')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
