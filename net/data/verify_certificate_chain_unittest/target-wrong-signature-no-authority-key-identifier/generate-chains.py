#!/usr/bin/env python
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the target was signed by a different
certificate with a different key. The target lacks an
authorityKeyIdentifier extension as some verifiers will not try verifying with
the bogus intermediate if the authorityKeyIdentifier does not match the
intermediate's subjectKeyIdentifier.
"""

import sys
sys.path += ['../..']

import gencerts

# Self-signed root certificate.
root = gencerts.create_self_signed_root_certificate('Root')

# Intermediate certificate, which actually signed the leaf.
intermediate = gencerts.create_intermediate_certificate('Intermediate', root)
section = intermediate.config.get_section('signing_ca_ext')
# Don't include authorityKeyIdentifier extension in the target issued by
# the real intermediate, otherwise the verifier might not even try to validate
# the signature against the bogus intermediate.
section.remove_property('authorityKeyIdentifier')

# Target certificate.
target = gencerts.create_end_entity_certificate('Target', intermediate)
# TODO(eroman): Set subjectAltName by default rather than specifically in
# this test.
target.get_extensions().set_property('subjectAltName', 'DNS:test.example')

# Intermediate certificate issued by root, but which did not sign target.
bogus_intermediate = gencerts.create_intermediate_certificate('Intermediate',
                                                              root)

chain = [target, bogus_intermediate, root]
gencerts.write_chain(__doc__, chain, 'chain.pem')
