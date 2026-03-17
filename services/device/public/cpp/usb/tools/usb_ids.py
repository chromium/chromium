# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import itertools
import optparse
import re

VENDOR_PATTERN = re.compile(r"^(?P<id>[0-9a-fA-F]{4})\s+(?P<name>.+)$")
PRODUCT_PATTERN = re.compile(r"^\t(?P<id>[0-9a-fA-F]{4})\s+(?P<name>.+)$")

def EscapeName(name):
  name = name.replace("\\", "\\\\")
  name = name.replace('"', r'\"')
  name = name.replace("?", r"\?")
  name = name.replace('\0', r'\u0000')
  return name

def ParseTable(input_path):
  input_file = io.open(input_path, "r", encoding="ascii", errors="ignore")
  input = input_file.read().split("\n")
  input_file.close()

  table = {}
  vendor = None

  for line in input:
    vendor_match = VENDOR_PATTERN.match(line)
    if vendor_match:
      if vendor:
        table[vendor["id"]] = vendor
      vendor = {}
      vendor["id"] = int(vendor_match.group("id"), 16)
      vendor["name"] = vendor_match.group("name")
      vendor["products"] = []
      continue

    product_match = PRODUCT_PATTERN.match(line)
    if product_match:
      if not vendor:
        raise Exception("Product seems to appear before vendor.")
      product = {}
      product["id"] = int(product_match.group("id"), 16)
      product["name"] = product_match.group("name")
      vendor["products"].append(product)

  return table

def GenerateStringRelPointer(data, output_tables):
  # At time of writing, there are 1232 strings that appear more than once in the
  # USB ID database.
  # Since this script concatenates all strings together, rather than leaving
  # them as individual literals, the linker can't deduplicate them for us
  # anymore; so do it manually here.
  if data not in output_tables["offset_by_string"]:
    data_with_nul = data + '\0'
    output_tables["offset_by_string"][data] = output_tables["string_bytes"]
    output_tables["strings"].append(data_with_nul)
    output_tables["string_bytes"] += len(bytes(data_with_nul, 'utf-8'))
  off = output_tables["offset_by_string"][data]
  return "base::subtle::IndexPointer<char, device::usb_strings>("+str(off)+")"

def GenerateDeviceDefinitions(table, output_tables):
  output = ""

  for vendor_id in sorted(table.keys()):
    vendor = table[vendor_id]
    if len(vendor["products"]) == 0:
      continue

    output += "static const UsbProduct vendor_%.4x_products[] = {\n" % \
        vendor["id"]
    for product in vendor["products"]:
      output += "  {0x%.4x, %s/*%s*/},\n" % (
          product["id"],
          GenerateStringRelPointer(product["name"], output_tables),
          product["name"])
    output += "};\n"

  return output

def GenerateVendorDefinitions(table, output_tables):
  output = "static const UsbVendor vendors[] = {\n"

  for vendor_id in sorted(table.keys()):
    vendor = table[vendor_id]

    product_table = "{}"
    if len(vendor["products"]) != 0:
      product_table = "vendor_%.4x_products" % (vendor["id"])
    output += "  {%s/*%s*/, 0x%.4x, %s},\n" % (
        GenerateStringRelPointer(vendor["name"], output_tables),
        vendor["name"],
        vendor["id"],
        product_table)

  output += "};\n"
  output += "const base::span<const UsbVendor> UsbIds::vendors_ = vendors;\n"
  return output

if __name__ == "__main__":
  parser = optparse.OptionParser(
      description="Generates a C++ USB ID lookup table.")
  parser.add_option("-i", "--input", help="Path to usb.ids")
  parser.add_option("-o", "--output", help="Output file path")

  (opts, args) = parser.parse_args()
  table = ParseTable(opts.input)

  output_tables = {"strings":[], "string_bytes":0, "offset_by_string":{}}

  device_definitions = GenerateDeviceDefinitions(table, output_tables)
  vendor_definitions = GenerateVendorDefinitions(table, output_tables)

  output = """// Generated from %s
#ifndef GENERATED_USB_IDS_H_
#define GENERATED_USB_IDS_H_

#include <stddef.h>

#include "base/memory/index_pointer.h"
#include "services/device/public/cpp/usb/usb_ids.h"

namespace device {

""" % (opts.input)
  output += "const char usb_strings[] = \"" + EscapeName(''.join(output_tables["strings"])) + "\";"
  output += device_definitions
  output += vendor_definitions
  output += """

}  // namespace device

#endif  // GENERATED_USB_IDS_H_
"""

  output_file = open(opts.output, "w+")
  output_file.write(output)
  output_file.close()
