#!/usr/bin/env python3
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chain where the leaf has two policies and the intermediate has anyPolicy."""

import sys
sys.path += ['../../../../net/data']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.get_extensions().set_property('certificatePolicies', 'anyPolicy')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.get_extensions().set_property('certificatePolicies', '1.2.3.4,1.2.6.7')
target.get_extensions().set_property('subjectAltName', 'DNS:test.example')

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
