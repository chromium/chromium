#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This utility takes a JSON input that describes a CRLSet and produces a
CRLSet from it.

The input is taken on stdin and is a dict with the following keys:
  - BlockedBySPKI: An array of strings, where each string is a filename
      containing a PEM certificate, from which an SPKI will be extracted.
  - BlockedByHash: A dict of string to an array of strings. The dict key is
      a filename containing a PEM certificate, representing the issuer cert,
      while the array of strings contain the filenames of PEM format
      certificates whose serials are blocked.
  - LimitedSubjects: A dict of string to an array of strings, where the key is
      a filename containing a PEM format certificate, and the strings are the
      filenames of PEM format certificates. Certificates that share a Subject
      with the key will be restricted to the set of SPKIs extracted from the
      files in the values.
  - Sequence: An optional integer sequence number to use for the CRLSet. If
      not present, defaults to 1.

For example:

{
  "BlockedBySPKI": ["/tmp/blocked-certificate"],
  "BlockedByHash": {
    "/tmp/intermediate-certificate": [1, 2, 3]
  },
  "LimitedSubjects": {
    "/tmp/limited-certificate": [
        "/tmp/limited-certificate",
        "/tmp/limited-certificate2"
    ]
  },
  "Sequence": 23
}
"""

import base64
import collections
import hashlib
import json
import optparse
import six
import struct
import sys


def _pem_cert_to_binary(pem_filename):
  """Decodes the first PEM-encoded certificate in a given file into binary

  Args:
    pem_filename: A filename that contains a PEM-encoded certificate. It may
        contain additional data (keys, textual representation) which will be
        ignored

  Returns:
    A byte array containing the decoded certificate data
  """
  pem_data = ""
  started = False

  with open(pem_filename, 'r') as pem_file:
    for line in pem_file:
      if not started:
        if line.startswith('-----BEGIN CERTIFICATE'):
          started = True
      else:
        if line.startswith('-----END CERTIFICATE'):
          break
        pem_data += line[:-1].strip()

  return base64.b64decode(pem_data)


def _parse_asn1_element(der_bytes):
  """Parses a DER-encoded tag/Length/Value into its component parts

  Args:
    der_bytes: A DER-encoded ASN.1 data type

  Returns:
    A tuple of the ASN.1 tag value, the length of the ASN.1 header that was
    read, the sequence of bytes for the value, and then any data from der_bytes
    that was not part of the tag/Length/Value.
  """
  tag = six.indexbytes(der_bytes, 0)
  length = six.indexbytes(der_bytes, 1)
  header_length = 2

  if length & 0x80:
    num_length_bytes = length & 0x7f
    length = 0
    for i in range(2, 2 + num_length_bytes):
      length <<= 8
      length += six.indexbytes(der_bytes, i)
    header_length = 2 + num_length_bytes

  contents = der_bytes[:header_length + length]
  rest = der_bytes[header_length + length:]

  return (tag, header_length, contents, rest)


class ASN1Iterator(object):
  """Iterator that parses and iterates through a ASN.1 DER structure"""

  def __init__(self, contents):
    self._tag = 0
    self._header_length = 0
    self._rest = None
    self._contents = contents
    self.step_into()

  def step_into(self):
    """Begins processing the inner contents of the next ASN.1 element"""
    (self._tag, self._header_length, self._contents, self._rest) = (
        _parse_asn1_element(self._contents[self._header_length:]))

  def step_over(self):
    """Skips/ignores the next ASN.1 element"""
    (self._tag, self._header_length, self._contents, self._rest) = (
        _parse_asn1_element(self._rest))

  def tag(self):
    """Returns the ASN.1 tag of the current element"""
    return self._tag

  def contents(self):
    """Returns the raw data of the current element"""
    return self._contents

  def encoded_value(self):
    """Returns the encoded value of the current element (i.e. without header)"""
    return self._contents[self._header_length:]


def _der_cert_to_spki(der_bytes):
  """Returns the subjectPublicKeyInfo of a DER-encoded certificate

  Args:
    der_bytes: A DER-encoded certificate (RFC 5280)

  Returns:
    A byte array containing the subjectPublicKeyInfo
  """
  iterator = ASN1Iterator(der_bytes)
  iterator.step_into()  # enter certificate structure
  iterator.step_into()  # enter TBSCertificate
  iterator.step_over()  # over version
  iterator.step_over()  # over serial
  iterator.step_over()  # over signature algorithm
  iterator.step_over()  # over issuer name
  iterator.step_over()  # over validity
  iterator.step_over()  # over subject name
  return iterator.contents()


def der_cert_to_spki_hash(der_cert):
  """Gets the SHA-256 hash of the subjectPublicKeyInfo of a DER encoded cert

  Args:
    der_cert: A string containing the DER-encoded certificate

  Returns:
    The SHA-256 hash of the certificate, as a byte sequence
  """
  return hashlib.sha256(_der_cert_to_spki(der_cert)).digest()


def pem_cert_file_to_spki_hash(pem_filename):
  """Gets the SHA-256 hash of the subjectPublicKeyInfo of a cert in a file

  Args:
    pem_filename: A file containing a PEM-encoded certificate.

  Returns:
    The SHA-256 hash of the first certificate in the file, as a byte sequence
  """
  return der_cert_to_spki_hash(_pem_cert_to_binary(pem_filename))


def der_cert_to_subject_hash(der_bytes):
  """Returns SHA256(subject) of a DER-encoded certificate

  Args:
    der_bytes: A DER-encoded certificate (RFC 5280)

  Returns:
    The SHA-256 hash of the certificate's subject.
  """
  iterator = ASN1Iterator(der_bytes)
  iterator.step_into()  # enter certificate structure
  iterator.step_into()  # enter TBSCertificate
  iterator.step_over()  # over version
  iterator.step_over()  # over serial
  iterator.step_over()  # over signature algorithm
  iterator.step_over()  # over issuer name
  iterator.step_over()  # over validity
  return hashlib.sha256(iterator.contents()).digest()


def pem_cert_file_to_subject_hash(pem_filename):
  """Gets the SHA-256 hash of the subject of a cert in a file

  Args:
    pem_filename: A file containing a PEM-encoded certificate.

  Returns:
    The SHA-256 hash of the subject of the first certificate in the file, as a
    byte sequence
  """
  return der_cert_to_subject_hash(_pem_cert_to_binary(pem_filename))


def der_cert_to_serial(der_bytes):
  """Gets the serial of a DER-encoded certificate, omitting leading 0x00

  Args:
    der_bytes: A DER-encoded certificates (RFC 5280)

  Returns:
    The encoded serial number value (omitting tag and length), and omitting
    any leading 0x00 used to indicate it is a positive INTEGER.
  """
  iterator = ASN1Iterator(der_bytes)
  iterator.step_into()  # enter certificate structure
  iterator.step_into()  # enter TBSCertificate
  iterator.step_over()  # over version
  raw_serial = iterator.encoded_value()
  if six.indexbytes(raw_serial, 0) == 0x00 and len(raw_serial) > 1:
    raw_serial = raw_serial[1:]
  return raw_serial


def pem_cert_file_to_serial(pem_filename):
  """Gets the DER-encoded serial of a cert in a file, omitting leading 0x00

  Args:
    pem_filename: A file containing a PEM-encoded certificate.

  Returns:
    The DER-encoded serial as a byte sequence
  """
  return der_cert_to_serial(_pem_cert_to_binary(pem_filename))


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option('-o', '--output',
                    help='Specifies the output file. The default is stdout.')
  options, _ = parser.parse_args()
  outfile = sys.stdout
  if options.output and options.output != '-':
    outfile = open(options.output, 'wb')

  config = json.load(sys.stdin)
  blocked_spkis = [
      base64.b64encode(pem_cert_file_to_spki_hash(pem_file)).decode('ascii')
      for pem_file in config.get('BlockedBySPKI', [])
  ]
  parents = {
      pem_cert_file_to_spki_hash(pem_file): [
          pem_cert_file_to_serial(issued_cert_file)
          for issued_cert_file in issued_certs
      ]
      for pem_file, issued_certs in config.get('BlockedByHash', {}).items()
  }
  limited_subjects = {
      base64.b64encode(pem_cert_file_to_subject_hash(pem_file)).decode('ascii'):
      [
          base64.b64encode(pem_cert_file_to_spki_hash(filename)).decode('ascii')
          for filename in allowed_pems
      ]
      for pem_file, allowed_pems in config.get('LimitedSubjects', {}).items()
  }
  known_interception_spkis = [
      base64.b64encode(pem_cert_file_to_spki_hash(pem_file)).decode('ascii')
      for pem_file in config.get('KnownInterceptionSPKIs', [])
  ]
  blocked_interception_spkis = [
      base64.b64encode(pem_cert_file_to_spki_hash(pem_file)).decode('ascii')
      for pem_file in config.get('BlockedInterceptionSPKIs', [])
  ]
  header_json = {
      'Version': 0,
      'ContentType': 'CRLSet',
      'Sequence': int(config.get("Sequence", 1)),
      'NumParents': len(parents),
      'BlockedSPKIs': blocked_spkis,
      'LimitedSubjects': limited_subjects,
      'KnownInterceptionSPKIs': known_interception_spkis,
      'BlockedInterceptionSPKIs': blocked_interception_spkis
  }
  header = json.dumps(header_json)
  outfile.write(struct.pack('<H', len(header)))
  outfile.write(header.encode('utf-8'))
  for spki, serials in sorted(parents.items()):
    outfile.write(spki)
    outfile.write(struct.pack('<I', len(serials)))
    for serial in serials:
      raw_serial = []
      if not serial:
        raw_serial = b'\x00'
      else:
        raw_serial = serial

      outfile.write(struct.pack('<B', len(raw_serial)))
      outfile.write(raw_serial)
  return 0


if __name__ == '__main__':
  sys.exit(main())
