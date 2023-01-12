#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Certificate chain where the leaf has a basic constraints extension with
CA=true"""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate (end entity, but has pathlen set).
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().set_property('basicConstraints', 'critical,CA:true')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
gencerts.write_chain(__doc__, [target], 'target_only.pem')
