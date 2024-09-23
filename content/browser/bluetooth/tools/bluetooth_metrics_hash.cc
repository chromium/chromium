// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <iostream>

#include "base/hash/hash.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

int main(int argc, char** argv) {
  if (argc <= 2) {
    std::cout << "Generates hash values given UUIDs using the same method\n"
              << "as in bluetooth_metrics.cc.\n"
              << "\n"
              << "Output is formatted for including into histograms.xml.\n"
              << "Note that tools/metrics/histograms/pretty_print.py will\n"
              << "sort enum entries for you.\n"
              << "\n"
              << "Usage: " << argv[0] << " <uuid> <label> [uuid2 label2...]\n"
              << "       The UUIDs may be short UUIDs, and will be made\n"
              << "       canonical before being hashed.\n"
              << "\n"
              << "Example: " << argv[0] << " FEFF foo FEFE bar\n"
              << "  <int value=\"62669585\" "
                 "label=\"foo; 0000feff-0000-1000-8000-00805f9b34fb\"/>\n"
              << "  <int value=\"643543662\" "
                 "label=\"bar; 0000fefe-0000-1000-8000-00805f9b34fb\"/>\n";
    return 1;
  }

  for (int i = 1; i < argc; i = i + 2) {
    std::string uuid_string(argv[i]);
    std::string label_string((i + 1 < argc) ? argv[i + 1] : "");
    device::BluetoothUUID uuid(uuid_string);
    std::string uuid_canonical_string = uuid.canonical_value();
    uint32_t hash = base::PersistentHash(uuid_canonical_string);

    // Strip off the sign bit because UMA doesn't support negative values,
    // but takes a signed int as input.
    hash &= 0x7fffffff;

    std::cout << "  <int value=\"" << hash << "\" label=\"" << label_string
              << "; " << uuid_canonical_string << "\"/>\n";
  }
  return 0;
}
