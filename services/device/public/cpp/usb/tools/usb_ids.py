# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Parses usb.ids and emits a compact little-endian binary blob suitable for
# shipping as a Chrome resource (loaded via ResourceBundle at runtime).
#
# Binary layout (all integers little-endian):
#
#   header (16 bytes):
#     u32 magic                = 'USBI' (0x49425355)
#     u32 version              = 2
#     u32 vendor_count
#     u32 string_blob_size
#
#   vendor table (vendor_count * 12 bytes, sorted by vid):
#     u16 vid
#     u16 product_count
#     u32 vendor_name_offset    // offset into string blob
#     u32 products_offset       // absolute file offset; 0 if product_count == 0
#
#   product blocks, concatenated in vendor order:
#     product entries (product_count * 4 bytes, sorted by pid within vendor):
#       u16 pid
#       u16 product_name_offset  // offset into this vendor's product strings
#     product strings:
#       concatenated NUL-terminated UTF-8 product strings, deduplicated within
#       the vendor. The first byte is a NUL so offset 0 is valid.
#
#   vendor string blob (string_blob_size bytes):
#     concatenated NUL-terminated UTF-8 vendor strings, deduplicated. The first
#     byte is a NUL so that offset 0 is valid.

import io
import optparse
import re
import struct

VENDOR_PATTERN = re.compile(r"^(?P<id>[0-9a-fA-F]{4})\s+(?P<name>.+)$")
PRODUCT_PATTERN = re.compile(r"^\t(?P<id>[0-9a-fA-F]{4})\s+(?P<name>.+)$")

MAGIC = 0x49425355  # 'USBI'
VERSION = 2
HEADER_SIZE = 16
VENDOR_ENTRY_SIZE = 12
PRODUCT_ENTRY_SIZE = 4


def ParseTable(input_path):
    with io.open(input_path, "r", encoding="ascii", errors="ignore") as f:
        lines = f.read().split("\n")

    table = {}
    vendor = None

    for line in lines:
        vendor_match = VENDOR_PATTERN.match(line)
        if vendor_match:
            vendor = {
                "id": int(vendor_match.group("id"), 16),
                "name": vendor_match.group("name"),
                "products": [],
            }
            table[vendor["id"]] = vendor
            continue

        product_match = PRODUCT_PATTERN.match(line)
        if product_match:
            if not vendor:
                raise Exception("Product appears before any vendor.")
            vendor["products"].append(
                {
                    "id": int(product_match.group("id"), 16),
                    "name": product_match.group("name"),
                }
            )

    return table


class StringBlob(object):
    """Deduplicated, NUL-terminated UTF-8 string pool."""

    def __init__(self):
        # First byte is a NUL so offset 0 is a valid empty string.
        self._buf = bytearray(b"\0")
        self._offsets = {}

    def Add(self, s):
        cached = self._offsets.get(s)
        if cached is not None:
            return cached
        encoded = s.encode("utf-8") + b"\0"
        offset = len(self._buf)
        self._buf.extend(encoded)
        self._offsets[s] = offset
        return offset

    def Bytes(self):
        return bytes(self._buf)


def Build(table):
    vendor_ids = sorted(table.keys())
    vendor_count = len(vendor_ids)

    # Compute the fixed-position offset of the product blocks section.
    vendor_table_off = HEADER_SIZE
    products_section_off = vendor_table_off + vendor_count * VENDOR_ENTRY_SIZE

    vendor_strings = StringBlob()

    vendor_entries = bytearray()
    product_blocks = bytearray()
    cursor = products_section_off

    for vid in vendor_ids:
        vendor = table[vid]
        products = sorted(vendor["products"], key=lambda p: p["id"])
        name_off = vendor_strings.Add(vendor["name"])
        if products:
            products_off = cursor
            product_strings = StringBlob()
            product_entries = bytearray()
            for p in products:
                product_name_off = product_strings.Add(p["name"])
                if product_name_off > 0xFFFF:
                    raise Exception("Product string offset exceeds u16.")
                product_entries += struct.pack("<HH", p["id"], product_name_off)
            product_block = product_entries + product_strings.Bytes()
            product_blocks += product_block
            cursor += len(product_block)
        else:
            products_off = 0
        vendor_entries += struct.pack(
            "<HHII", vid, len(products), name_off, products_off
        )

    string_bytes = vendor_strings.Bytes()

    header = struct.pack(
        "<IIII", MAGIC, VERSION, vendor_count, len(string_bytes)
    )

    out = bytearray()
    out += header
    out += vendor_entries
    out += product_blocks
    out += string_bytes

    expected = cursor + len(string_bytes)
    assert len(out) == expected, "size mismatch: %d vs %d" % (
        len(out),
        expected,
    )
    return bytes(out)


if __name__ == "__main__":
    parser = optparse.OptionParser(
        description="Generates a compact binary USB ID lookup table."
    )
    parser.add_option("-i", "--input", help="Path to usb.ids")
    parser.add_option("-o", "--output", help="Output blob path")

    (opts, args) = parser.parse_args()
    table = ParseTable(opts.input)
    blob = Build(table)

    with open(opts.output, "wb") as f:
        f.write(blob)
