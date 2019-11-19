#!/usr/bin/env python2.7
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generates certificate chains for testing name normalization.'''

import os
import subprocess
import sys

sys.path.append(os.path.join('..', '..', '..', 'tools', 'testserver'))
import minica


def pretty_print_cert(der):
  command = ["openssl", "x509", "-text", "-inform", "DER"]
  p = subprocess.Popen(command,
                       stdin=subprocess.PIPE,
                       stdout=subprocess.PIPE)
  result = p.communicate(der)
  if p.returncode != 0:
    raise RuntimeError("openssl failed: %s" % p.returncode)
  return result[0]


def writecerts(name, der_certs):
  fn = os.path.join('..', 'certificates', name)

  text_certs = []
  print 'pretty printing', fn
  for der in der_certs:
    text_certs.append(pretty_print_cert(der))

  print 'writing', fn
  with open(fn, 'w') as f:
    f.write('\n'.join(text_certs))


def GenerateCertAndIntermediate(leaf_subject,
                                leaf_issuer,
                                intermediate_subject,
                                ip_sans=None,
                                dns_sans=None,
                                serial=0):
  if serial == 0:
    serial = minica.RandomNumber(16)

  intermediate_serial = minica.RandomNumber(16)

  target_cert_der = minica.MakeCertificate(
      leaf_issuer, leaf_subject, serial, minica.LEAF_KEY,
      minica.INTERMEDIATE_KEY, ip_sans=ip_sans, dns_sans=dns_sans)

  intermediate_cert_der = minica.MakeCertificate(
      minica.ROOT_CN, intermediate_subject, intermediate_serial,
      minica.INTERMEDIATE_KEY, minica.ROOT_KEY, is_ca=True)

  return [target_cert_der, intermediate_cert_der]


def GeneratePrintableStringUtf8StringChain():
  namesuffix = " for PrintableString / Utf8String comparison"
  issuer_name = "Intermediate" + namesuffix
  certs = GenerateCertAndIntermediate(leaf_subject="Leaf" + namesuffix,
                                      leaf_issuer=issuer_name,
                                      intermediate_subject=unicode(issuer_name),
                                      ip_sans=["\x7F\x00\x00\x01"],
                                      dns_sans=["example.test"])
  writecerts('name-normalization-printable-utf8.pem', certs)


def GenerateCaseFoldChain():
  namesuffix = " for case folding comparison"
  issuer_name = "Intermediate" + namesuffix
  certs = GenerateCertAndIntermediate(leaf_subject="Leaf" + namesuffix,
                                      leaf_issuer=issuer_name.replace('I', 'i'),
                                      intermediate_subject=issuer_name,
                                      ip_sans=["\x7F\x00\x00\x01"],
                                      dns_sans=["example.test"])
  writecerts('name-normalization-case-folding.pem', certs)


def GenerateNormalChain():
  namesuffix = " for byte equality comparison"
  issuer_name = "Intermediate" + namesuffix
  certs = GenerateCertAndIntermediate(leaf_subject="Leaf" + namesuffix,
                                      leaf_issuer=issuer_name,
                                      intermediate_subject=issuer_name,
                                      ip_sans=["\x7F\x00\x00\x01"],
                                      dns_sans=["example.test"])
  writecerts('name-normalization-byteequal.pem', certs)


if __name__ == '__main__':
  GeneratePrintableStringUtf8StringChain()
  GenerateCaseFoldChain()
  GenerateNormalChain()
