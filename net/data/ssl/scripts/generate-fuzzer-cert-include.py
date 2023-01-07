# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate C/C++ source from certificate.

Usage:
  generate-spdy-session-fuzzer-includes.py input_filename output_filename

Load the PEM block from `input_filename` certificate, perform base64 decoding
and hex encoding, and save it in `output_filename` so that it can be directly
included from C/C++ source as a char array.
"""

import base64
import re
import sys
import textwrap

input_filename = sys.argv[1]
output_filename = sys.argv[2]

# Load PEM block.
with open(input_filename, 'r') as f:
  match = re.search(
      r"-----BEGIN CERTIFICATE-----\n(.+)-----END CERTIFICATE-----\n", f.read(),
      re.DOTALL)
text = match.group(1)

# Perform Base64 decoding.
data = base64.b64decode(text)

# Hex format data.
hex_encoded = ", ".join("0x{:02x}".format(c) for c in bytearray(data))

# Write into |output_filename| wrapped at 80 columns.
with open(output_filename, 'w') as f:
  f.write(textwrap.fill(hex_encoded, 80))
  f.write("\n")
