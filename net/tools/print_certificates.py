#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pretty-prints certificates as an openssl-annotated PEM file."""

import argparse
import base64
import errno
import hashlib
import os
import re
import subprocess
import sys
import traceback


def read_file_to_string(path):
  with open(path, 'r') as f:
    return f.read()


def read_certificates_data_from_server(hostname):
  """Uses openssl to fetch the PEM-encoded certificates for an SSL server."""
  p = subprocess.Popen(["openssl", "s_client", "-showcerts",
                        "-servername", hostname,
                        "-connect", hostname + ":443"],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
  result = p.communicate()

  if p.returncode == 0:
    return result[0]

  sys.stderr.write("Failed getting certificates for %s:\n%s\n" % (
      hostname, result[1]))
  return ""


def read_sources_from_commandline(sources):
  """Processes the command lines and returns an array of all the sources
  bytes."""
  sources_bytes = []

  if not sources:
    # If no command-line arguments were given to the program, read input from
    # stdin.
    sources_bytes.append(sys.stdin.read())
  else:
    for arg in sources:
      # If the argument identifies a file path, read it
      if os.path.exists(arg):
        sources_bytes.append(read_file_to_string(arg))
      else:
        # Otherwise treat it as a web server address.
        sources_bytes.append(read_certificates_data_from_server(arg))

  return sources_bytes


def strip_indentation_whitespace(text):
  """Strips leading whitespace from each line."""
  stripped_lines = [line.lstrip() for line in text.split("\n")]
  return "\n".join(stripped_lines)


def strip_all_whitespace(text):
  pattern = re.compile(r'\s+')
  return re.sub(pattern, '', text)


def extract_certificates_from_pem(pem_bytes):
  certificates_der = []

  regex = re.compile(
      r'-----BEGIN (CERTIFICATE|PKCS7)-----(.*?)-----END \1-----', re.DOTALL)

  for match in regex.finditer(pem_bytes):
    der = base64.b64decode(strip_all_whitespace(match.group(2)))
    if match.group(1) == 'CERTIFICATE':
      certificates_der.append(der)
    else:
      certificates_der.extend(extract_certificates_from_der_pkcs7(der))

  return certificates_der


def extract_certificates_from_der_pkcs7(der_bytes):
  pkcs7_certs_pem = process_data_with_command(
      ['openssl','pkcs7','-print_certs', '-inform', 'DER'], der_bytes)
  # The output will be one or more PEM encoded certificates.
  # (Or CRLS, but those will be ignored.)
  if pkcs7_certs_pem:
    return extract_certificates_from_pem(pkcs7_certs_pem)
  return []


def extract_certificates_from_der_ascii(input_text):
  certificates_der = []

  # Look for beginning and end of Certificate SEQUENCE. The indentation is
  # significant. (The SEQUENCE must be non-indented, and the rest of the DER
  # ASCII must be indented until the closing } which again is non-indented.)
  # The output of der2ascii meets this, but it is not a requirement of the DER
  # ASCII language.
  # TODO(mattm): consider alternate approach of doing ascii2der on entire
  # input, and handling the multiple concatenated DER certificates.
  regex = re.compile(r'^(SEQUENCE {.*?^})', re.DOTALL | re.MULTILINE)

  for match in regex.finditer(input_text):
    der_ascii_bytes = match.group(1)
    der_bytes = process_data_with_command(["ascii2der"], der_ascii_bytes)
    if der_bytes:
      certificates_der.append(der_bytes)

  return certificates_der


def decode_netlog_hexdump(netlog_text):
  lines = netlog_text.splitlines()

  # Skip the text preceeding the actual hexdump.
  while lines and 'bytes =' not in lines[0]:
    del lines[0]
  if not lines:
    return None
  del lines[0]

  bytes = []
  hex_re = re.compile('\s*([0-9A-Fa-f ]{48})')
  for line in lines:
    m = hex_re.search(line)
    if not m:
      break
    hex_string = m.group(1)
    bytes.extend(chr(int(part, 16)) for part in hex_string.split())

  return ''.join(bytes)


class ByteReader:
  """Iteratively consume data from a byte string.

  Automatically tracks and advances current position in the string as data is
  consumed, and will throw an exception if attempting to read past the end of
  the string.
  """
  def __init__(self, data):
    self.data = data
    self.pos = 0

  def consume_byte(self):
    i = ord(self.data[self.pos])
    self.pos += 1
    return i

  def consume_int16(self):
    return ((self.consume_byte() << 8) + self.consume_byte())

  def consume_int24(self):
    return ((self.consume_byte() << 16) + (self.consume_byte() << 8) +
            self.consume_byte())

  def consume_bytes(self, n):
    b = self.data[self.pos:self.pos+n]
    if len(b) != n:
      raise IndexError('requested:%d bytes  actual:%d bytes'%(n, len(b)))
    self.pos += n
    return b

  def remaining_byte_count(self):
    return len(self.data) - self.pos


def decode_tls10_certificate_message(reader):
  message_length = reader.consume_int24()
  if reader.remaining_byte_count() != message_length:
    raise RuntimeError(
        'message_length(%d) != remaining_byte_count(%d)\n' % (
            message_length, reader.remaining_byte_count()))

  certificate_list_length = reader.consume_int24()
  if reader.remaining_byte_count() != certificate_list_length:
    raise RuntimeError(
        'certificate_list_length(%d) != remaining_byte_count(%d)\n' % (
            certificate_list_length, reader.remaining_byte_count()))

  certificates_der = []
  while reader.remaining_byte_count():
    cert_len = reader.consume_int24()
    certificates_der.append(reader.consume_bytes(cert_len))

  return certificates_der


def decode_tls13_certificate_message(reader):
  message_length = reader.consume_int24()
  if reader.remaining_byte_count() != message_length:
    raise RuntimeError(
        'message_length(%d) != remaining_byte_count(%d)\n' % (
            message_length, reader.remaining_byte_count()))

  # Ignore certificate_request_context.
  certificate_request_context_length = reader.consume_byte()
  reader.consume_bytes(certificate_request_context_length)

  certificate_list_length = reader.consume_int24()
  if reader.remaining_byte_count() != certificate_list_length:
    raise RuntimeError(
        'certificate_list_length(%d) != remaining_byte_count(%d)\n' % (
            certificate_list_length, reader.remaining_byte_count()))

  certificates_der = []
  while reader.remaining_byte_count():
    # Assume certificate_type is X.509.
    cert_len = reader.consume_int24()
    certificates_der.append(reader.consume_bytes(cert_len))
    # Ignore extensions.
    extension_len = reader.consume_int16()
    reader.consume_bytes(extension_len)

  return certificates_der


def decode_tls_certificate_message(certificate_message):
  reader = ByteReader(certificate_message)
  if reader.consume_byte() != 11:
    sys.stderr.write('HandshakeType != 11. Not a Certificate Message.\n')
    return []

  # The TLS certificate message encoding changed in TLS 1.3. Rather than
  # require pasting in and parsing the whole handshake to discover the TLS
  # version, just try parsing the message with both the old and new encodings.

  # First try the old style certificate message:
  try:
    return decode_tls10_certificate_message(reader)
  except (IndexError, RuntimeError):
    tls10_traceback = traceback.format_exc()

  # Restart the ByteReader and consume the HandshakeType byte again.
  reader = ByteReader(certificate_message)
  reader.consume_byte()
  # Try the new style certificate message:
  try:
    return decode_tls13_certificate_message(reader)
  except (IndexError, RuntimeError):
    tls13_traceback = traceback.format_exc()

  # Neither attempt succeeded, just dump some error info:
  sys.stderr.write("Couldn't parse TLS certificate message\n")
  sys.stderr.write("TLS1.0 parse attempt:\n%s\n" % tls10_traceback)
  sys.stderr.write("TLS1.3 parse attempt:\n%s\n" % tls13_traceback)
  sys.stderr.write("\n")

  return []


def extract_tls_certificate_message(netlog_text):
  raw_certificate_message = decode_netlog_hexdump(netlog_text)
  if not raw_certificate_message:
    return []
  return decode_tls_certificate_message(raw_certificate_message)


def extract_certificates(source_bytes):
  if "BEGIN CERTIFICATE" in source_bytes or "BEGIN PKCS7" in source_bytes:
    return extract_certificates_from_pem(source_bytes)

  if "SEQUENCE {" in source_bytes:
    return extract_certificates_from_der_ascii(source_bytes)

  if "SSL_HANDSHAKE_MESSAGE_RECEIVED" in source_bytes:
    return extract_tls_certificate_message(source_bytes)

  # DER encoding of PKCS #7 signedData OID (1.2.840.113549.1.7.2)
  if "\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x07\x02" in source_bytes:
    return extract_certificates_from_der_pkcs7(source_bytes)

  # Otherwise assume it is the DER for a single certificate
  return [source_bytes]


def process_data_with_command(command, data):
  try:
    p = subprocess.Popen(command,
                         stdin=subprocess.PIPE,
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE)
  except OSError, e:
    if e.errno == errno.ENOENT:
      sys.stderr.write("Failed to execute %s\n" % command[0])
      return ""
    raise

  result = p.communicate(data)

  if p.returncode == 0:
    return result[0]

  # Otherwise failed.
  sys.stderr.write("Failed: %s: %s\n" % (" ".join(command), result[1]))
  return ""


def openssl_text_pretty_printer(certificate_der, unused_certificate_number):
  return process_data_with_command(["openssl", "x509", "-text", "-inform",
                                   "DER", "-noout"], certificate_der)


def pem_pretty_printer(certificate_der, unused_certificate_number):
  return process_data_with_command(["openssl", "x509", "-inform", "DER",
                                   "-outform", "PEM"], certificate_der)


def der2ascii_pretty_printer(certificate_der, unused_certificate_number):
  return process_data_with_command(["der2ascii"], certificate_der)


def header_pretty_printer(certificate_der, certificate_number):
  cert_hash = hashlib.sha256(certificate_der).hexdigest()
  return """===========================================
Certificate%d: %s
===========================================""" % (certificate_number, cert_hash)


# This is actually just used as a magic value, since pretty_print_certificates
# special-cases der output.
def der_printer():
  raise RuntimeError


def pretty_print_certificates(certificates_der, pretty_printers):
  # Need to special-case DER output to avoid adding any newlines, and to
  # only allow a single certificate to be output.
  if pretty_printers == [der_printer]:
    if len(certificates_der) > 1:
      sys.stderr.write("DER output only supports a single certificate, "
                       "ignoring %d remaining certs\n" % (
                           len(certificates_der) - 1))
    return certificates_der[0]

  result = ""
  for i in range(len(certificates_der)):
    certificate_der = certificates_der[i]
    pretty = []
    for pretty_printer in pretty_printers:
      pretty_printed = pretty_printer(certificate_der, i)
      if pretty_printed:
        pretty.append(pretty_printed)
    result += "\n".join(pretty) + "\n"
  return result


def parse_outputs(outputs):
  pretty_printers = []
  output_map = {"der2ascii": der2ascii_pretty_printer,
                "openssl_text": openssl_text_pretty_printer,
                "pem": pem_pretty_printer,
                "header": header_pretty_printer,
                "der": der_printer}
  for output_name in outputs.split(','):
    if output_name not in output_map:
      sys.stderr.write("Invalid output type: %s\n" % output_name)
      return []
    pretty_printers.append(output_map[output_name])
  if der_printer in pretty_printers and len(pretty_printers) > 1:
      sys.stderr.write("Output type der must be used alone.\n")
      return []
  return pretty_printers


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)

  parser.add_argument('sources', metavar='SOURCE', nargs='*',
                      help='''Each SOURCE can be one of:
  (1) A server name such as www.google.com.
  (2) A PEM [*] file containing one or more CERTIFICATE or PKCS7 blocks
  (3) A file containing one or more DER ASCII certificates
  (4) A text NetLog dump of a TLS certificate message
      (must include the SSL_HANDSHAKE_MESSAGE_RECEIVED line)
  (5) A binary file containing DER-encoded PKCS #7 signedData
  (6) A binary file containing DER-encoded certificate

When multiple SOURCEs are listed, all certificates in them
are concatenated. If no SOURCE is given then data will be
read from stdin.

[*] Parsing of PEM files is relaxed - leading indentation
whitespace will be stripped (needed for copy-pasting data
from NetLogs).''')

  parser.add_argument(
      '--output', dest='outputs', action='store',
      default="header,der2ascii,openssl_text,pem",
      help='output formats to use. Default: %(default)s')

  args = parser.parse_args()

  sources_bytes = read_sources_from_commandline(args.sources)

  pretty_printers = parse_outputs(args.outputs)
  if not pretty_printers:
    sys.stderr.write('No pretty printers selected.\n')
    sys.exit(1)

  certificates_der = []
  for source_bytes in sources_bytes:
    certificates_der.extend(extract_certificates(source_bytes))

  sys.stdout.write(pretty_print_certificates(certificates_der, pretty_printers))


if __name__ == "__main__":
  main()
