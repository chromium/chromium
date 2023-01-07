#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate has a smaller validity range
than the other certificates, making it easy to violate just its validity.

  Root:          2015/01/01 -> 2016/01/01
  Intermediate:  2015/03/01 -> 2015/09/01
  Target:        2015/01/01 -> 2016/01/01
"""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')
root.set_validity_range(gencerts.JANUARY_1_2015_UTC,
                        gencerts.JANUARY_1_2016_UTC)

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
intermediate.set_validity_range(gencerts.MARCH_1_2015_UTC,
                                gencerts.SEPTEMBER_1_2015_UTC)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
target.set_validity_range(gencerts.JANUARY_1_2015_UTC,
                          gencerts.JANUARY_1_2016_UTC)

chain = [target, intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
