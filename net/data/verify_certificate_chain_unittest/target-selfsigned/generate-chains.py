#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Single certificate chain for serverAuth which is self-signed."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed target certificate.
target = gencerts.create_self_signed_end_entity_certificate('Target')
target.get_extensions().set_property('extendedKeyUsage', 'serverAuth')

chain = [target]
gencerts.write_chain(__doc__, chain, 'chain.pem')
