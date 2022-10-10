# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Calculates the pervasive payload checksum"""

import hashlib

INCLUDE_HEADERS = frozenset([
    "access-control-allow-credentials", "access-control-allow-headers",
    "access-control-allow-methods", "access-control-allow-origin",
    "access-control-expose-headers", "access-control-max-age",
    "access-control-request-headers", "access-control-request-method",
    "clear-site-data", "content-encoding", "content-security-policy",
    "content-type", "cross-origin-embedder-policy",
    "cross-origin-opener-policy", "cross-origin-resource-policy", "location",
    "sec-websocket-accept", "sec-websocket-extensions", "sec-websocket-key",
    "sec-websocket-protocol", "sec-websocket-version", "upgrade", "vary"
])


def calculate_checksum(headers, raw_body):
  """Calculates the pervasive payload checksum for a given resource

  `headers` should be a list of name, value tuples.
  `raw_body` should be the response body exactly as returned by the server,
  without decompression or other filtering applied.
  Returns the SHA-256 checksum of the resource, calculated per the cache
  transparency serialization algorithm.
  """

  checksum_input = ""

  headers = [(name.lower(), value) for name, value in headers]
  headers.sort()

  for header in headers:
    if header[0] in INCLUDE_HEADERS:
      checksum_input += header[0] + ": " + header[1] + "\n"

  checksum_input += "\n"
  checksum_input = checksum_input.encode()

  checksum_input += raw_body

  return hashlib.sha256(checksum_input).hexdigest().upper()
