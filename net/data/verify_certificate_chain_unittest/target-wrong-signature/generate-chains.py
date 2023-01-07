#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the target certificate has an incorrect signature."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate to include in the certificate chain.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Actual intermediate that was used to sign the target certificate. It has the
# same subject as expected, but a different RSA key from the certificate
# included in the actual chain.
wrong_intermediate = gencerts.create_intermediate_certificate('Intermediate',
                                                            root)

# Target certificate, signed using |wrong_intermediate| NOT |intermediate|.
target = gencerts.create_end_entity_certificate('Target', wrong_intermediate)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
