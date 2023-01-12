#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Single certificate chain for serverAuth which is self-issued but which
does not contain the signer cert."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate with same name as target.
root = gencerts.create_self_signed_root_certificate('Target')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', root)
target.get_extensions().set_property('extendedKeyUsage', 'serverAuth')

chain = [target]
gencerts.write_chain(__doc__, chain, 'chain.pem')
