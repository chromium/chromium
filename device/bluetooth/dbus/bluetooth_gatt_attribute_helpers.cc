// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_gatt_attribute_helpers.h"

#include <string>

#include "dbus/message.h"
#include "dbus/object_path.h"

namespace bluez {

bool ReadOptions(dbus::MessageReader* reader,
                 std::map<std::string, dbus::MessageReader>* options) {
  dbus::MessageReader array_reader(nullptr);
  if (!reader->PopArray(&array_reader) || options == nullptr)
    return false;

  dbus::MessageReader dict_entry_reader(nullptr);
  std::string key;
  while (array_reader.HasMoreData()) {
    if (!array_reader.PopDictEntry(&dict_entry_reader) ||
        !dict_entry_reader.PopString(&key)) {
      options->clear();
      return false;
    }

    options->emplace(key, nullptr);
    if (!dict_entry_reader.PopVariant(&options->at(key))) {
      options->clear();
      return false;
    }
  }
  return true;
}

}  // namespace bluez
