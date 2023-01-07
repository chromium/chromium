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

def GenerateDeviceDefinitions(table):
  output = ""

  for vendor_id in sorted(table.keys()):
    vendor = table[vendor_id]
    if len(vendor["products"]) == 0:
      continue

    output += "static const UsbProduct vendor_%.4x_products[] = {\n" % \
        vendor["id"]
    for product in vendor["products"]:
      output += "  {0x%.4x, \"%s\"},\n" % (product["id"],
                                           EscapeName(product["name"]))
    output += "};\n"

  return output

def GenerateVendorDefinitions(table):
  output = "const size_t UsbIds::vendor_size_ = %d;\n" % len(table.keys())
  output += "const UsbVendor UsbIds::vendors_[] = {\n"

  for vendor_id in sorted(table.keys()):
    vendor = table[vendor_id]

    product_table = "nullptr"
    if len(vendor["products"]) != 0:
      product_table = "vendor_%.4x_products" % (vendor["id"])
    output += "  {\"%s\", %s, 0x%.4x, %d},\n" % (EscapeName(vendor["name"]),
                                                 product_table,
                                                 vendor["id"],
                                                 len(vendor["products"]))

  output += "};\n"
  return output

if __name__ == "__main__":
  parser = optparse.OptionParser(
      description="Generates a C++ USB ID lookup table.")
  parser.add_option("-i", "--input", help="Path to usb.ids")
  parser.add_option("-o", "--output", help="Output file path")

  (opts, args) = parser.parse_args()
  table = ParseTable(opts.input)

  output = """// Generated from %s
#ifndef GENERATED_USB_IDS_H_
#define GENERATED_USB_IDS_H_

#include <stddef.h>

#include "services/device/public/cpp/usb/usb_ids.h"

namespace device {

""" % (opts.input)
  output += GenerateDeviceDefinitions(table)
  output += GenerateVendorDefinitions(table)
  output += """

}  // namespace device

#endif  // GENERATED_USB_IDS_H_
"""

  output_file = open(opts.output, "w+")
  output_file.write(output)
  output_file.close()
