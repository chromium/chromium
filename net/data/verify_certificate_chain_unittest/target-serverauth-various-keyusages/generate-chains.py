#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a variety of chains where the target certificate varies in its key
type and key usages."""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate (used as trust anchor).
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Use either an RSA key, or an EC key for the target certificate. Generate the
# possible keys ahead of time so as not to duplicate the work.

KEYS = {
  'rsa': gencerts.get_or_generate_rsa_key(
      2048, gencerts.create_key_path('Target-rsa')),
  'ec': gencerts.get_or_generate_ec_key(
      'secp384r1', gencerts.create_key_path('Target-ec'))
};

KEY_USAGES = [ 'decipherOnly',
               'digitalSignature',
               'keyAgreement',
               'keyEncipherment' ]

# The proper key usage depends on the key purpose (serverAuth in this case),
# and the key type. Generate a variety of combinations.
for key_type in sorted(KEYS.keys()):
  for key_usage in KEY_USAGES:
    # Target certificate.
    target = gencerts.create_end_entity_certificate('Target', intermediate)
    target.get_extensions().set_property('extendedKeyUsage', 'serverAuth')
    target.get_extensions().set_property('keyUsage',
                                         'critical,%s' % (key_usage))

    # Set the key.
    target.set_key(KEYS[key_type])

    # Write the chain.
    chain = [target, intermediate, root]
    description = ('Certificate chain where the target certificate uses a %s '
                   'key and has the single key usage %s') % (key_type.upper(),
                                                             key_usage)
    gencerts.write_chain(description, chain,
                         '%s-%s.pem' % (key_type, key_usage))
