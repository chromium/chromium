#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the intermediate was signed by a different
certificate with a different key. The intermediate lacks an
authorityKeyIdentifier extension as some verifiers will not try verifying with
the bogus root if the authorityKeyIdentifier does not match the root's
subjectKeyIdentifier.
"""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate, which actually signed the intermediate.
root = gencerts.create_self_signed_root_certificate('Root')
section = root.config.get_section('signing_ca_ext')
# Don't include authorityKeyIdentifier extension in the intermediate issued by
# the real root, otherwise the verifier might not even try to validate the
# signature against the bogus root.
section.remove_property('authorityKeyIdentifier')

# Intermediate certificate.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
# TODO(eroman): Set subjectAltName by default rather than specifically in
# this test.
target.get_extensions().set_property('subjectAltName', 'DNS:test.example')

# Self-signed root certificate that has nothing to do with this chain, but will
# be saved as its root certificate.
bogus_root = gencerts.create_self_signed_root_certificate('Root')

chain = [target, intermediate, bogus_root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
