#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the supposed root certificate is wrong:

  * The intermediate's "issuer" does not match the root's "subject"
  * The intermediate's signature was not generated using the root's key
"""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate, which actually signed the intermediate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)

# Self-signed root certificate that has nothing to do with this chain, but will
# be saved as its root certificate.
bogus_root = gencerts.create_self_signed_root_certificate('BogusRoot')

chain = [target, intermediate, bogus_root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
