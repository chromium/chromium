#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates CrsRootIds labels in enums.xml with values from
net/data/ssl/chrome_root_store/

Usage:
autoninja -C out/Default net/cert:root_store_proto_full && \
        vpython3 net/tools/root_store_tool/update_crs_root_ids_enum.py
"""

import hashlib
import itertools
import os
import pathlib
import re
import sys
from collections import defaultdict

# Assumes the script is run with src/ as the cwd. This could be made smarter.
CHROMIUM_SRC_PATH = pathlib.Path('.')

sys.path.append(
    str((CHROMIUM_SRC_PATH / 'tools' / 'metrics' / 'histograms').resolve()))
import update_histogram_enum

# Add pyproto to path. Assume out/Default for now. This could be made an option.
PYPROTO_DIR = CHROMIUM_SRC_PATH / 'out' / 'Default' / 'pyproto' / \
        'net' / 'cert' / 'root_store_proto_full'
sys.path.append(str(PYPROTO_DIR.resolve()))

try:
  import root_store_pb2  # type: ignore[import-not-found]
except ImportError:
  print(f"Error: Failed to import root_store_pb2 from {PYPROTO_DIR}",
        file=sys.stderr)
  print(
      "Please make sure you have built the 'net/cert:root_store_proto_full' "
      "target in out/Default.",
      file=sys.stderr)
  sys.exit(1)

from google.protobuf import text_format  # type: ignore[import-untyped]
from cryptography import x509
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

ROOT_STORE_TEXTPROTO_PATH =\
        'net/data/ssl/chrome_root_store/root_store.textproto'
ROOT_CERTS_PATH = 'net/data/ssl/chrome_root_store/root_store.certs'
ADDITIONAL_CERTS_PATH = 'net/data/ssl/chrome_root_store/additional.certs'
ENUMS_XML_PATH = 'tools/metrics/histograms/metadata/net/enums.xml'


def load_certs_from_file(file_path):
  certs = {}
  try:
    with open(file_path, 'r') as f:
      content = f.read()
  except IOError as e:
    print(f"Error reading certs file {file_path}: {e}", file=sys.stderr)
    sys.exit(1)

  pem_blocks = re.findall(
      r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----', content,
      re.DOTALL)
  for pem in pem_blocks:
    try:
      cert = x509.load_pem_x509_certificate(pem.encode('utf-8'))
      der = cert.public_bytes(Encoding.DER)
      sha256_hex = hashlib.sha256(der).hexdigest()
      certs[sha256_hex] = cert
    except Exception as e:
      print(f"Warning: Failed to load cert from PEM in {file_path}: {e}",
            file=sys.stderr)
  return certs


def get_spki_hash(cert):
  spki_bytes = cert.public_key().public_bytes(Encoding.DER,
                                              PublicFormat.SubjectPublicKeyInfo)
  return hashlib.sha256(spki_bytes).hexdigest()


def relative_oid_to_string(data):
  parts = []
  val = 0
  for b in data:
    val = (val << 7) | (b & 0x7f)
    if not (b & 0x80):
      parts.append(str(val))
      val = 0
  return ".".join(parts)


def main():
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  # Load all certs
  all_certs = {}
  all_certs.update(load_certs_from_file(CHROMIUM_SRC_PATH / ROOT_CERTS_PATH))
  all_certs.update(
      load_certs_from_file(CHROMIUM_SRC_PATH / ADDITIONAL_CERTS_PATH))

  # Parse root_store.textproto
  root_store = root_store_pb2.RootStore()
  try:
    with open(CHROMIUM_SRC_PATH / ROOT_STORE_TEXTPROTO_PATH, 'r') as f:
      text_data = f.read()
    text_format.Parse(text_data, root_store)
  except Exception as e:
    print(f"Error parsing {ROOT_STORE_TEXTPROTO_PATH}: {e}", file=sys.stderr)
    sys.exit(1)

  crs_root_ids_enum = {}

  # Group classical anchors by crs_root_id
  classical_groups = defaultdict(list)
  for anchor in itertools.chain(root_store.trust_anchors,
                                root_store.additional_certs):
    if anchor.HasField('crs_root_id'):
      cert = all_certs.get(anchor.sha256_hex)
      if not cert:
        print(
            f"Error: Certificate not found for sha256_hex {anchor.sha256_hex}",
            file=sys.stderr)
        sys.exit(1)
      classical_groups[anchor.crs_root_id].append((anchor, cert))

  # Generate labels for classical anchors.
  for crs_root_id, group in classical_groups.items():
    if len(group) == 1:
      # If there is only one anchor with this id, just use it.
      cert = group[0][1]
    else:
      # If there are multiple anchors with the same id, choose the root based
      # on which one is self-issued. This assumes exactly one of them will be,
      # which is currently true.
      group_root_certs = [
          cert for anchor, cert in group if cert.issuer == cert.subject
      ]
      if len(group_root_certs) != 1:
        print(
            f"count of root certs for {crs_root_id} is "
            f"{len(group_root_certs)}",
            file=sys.stderr)
        sys.exit(1)
      cert = group_root_certs[0]

    spki_sha256 = get_spki_hash(cert)
    subject = cert.subject.rfc4514_string()
    label = f"{spki_sha256} {subject}"
    crs_root_ids_enum[crs_root_id] = label

  # Generate labels for MTC anchors.
  # TODO(crbug.com/452983502): switch to using the signer_set proto, and
  # include friendly name in addition to log id.
  for anchor in root_store.mtc_anchors:
    if anchor.HasField('crs_root_id'):
      crs_root_id = anchor.crs_root_id
      log_id_text = relative_oid_to_string(anchor.log_id)
      crs_root_ids_enum[crs_root_id] = log_id_text

  update_histogram_enum.UpdateHistogramFromDict(ENUMS_XML_PATH, 'CrsRootIds',
                                                crs_root_ids_enum,
                                                ROOT_STORE_TEXTPROTO_PATH,
                                                os.path.basename(__file__))


if __name__ == '__main__':
  main()
