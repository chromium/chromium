#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the leaf certificate asserts the keyCertSign key
usage, however does not have CA=true in the basic constraints extension to
indicate it is a CA."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate (end entity but has keyCertSign bit set).
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().set_property('keyUsage',
    'critical,digitalSignature,keyEncipherment,keyCertSign')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
