// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_ATTRIBUTE_VALUE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_ATTRIBUTE_VALUE_BLUEZ_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "device/bluetooth/bluetooth_export.h"

namespace bluez {

// This class contains a Bluetooth service attribute. A service attribute is
// defined by the following fields,
// type:  This is the type of the attribute. Along with being any of the
//        fixed types, an attribute can also be of type sequence, which means
//        that it contains an array of other attributes.
// size:  This is the size of the attribute. This can be variable for each type.
//        For example, a UUID can have the sizes, 2, 4 or 16 bytes.
// value: This is the raw value of the attribute. For example, for a UUID, it
//        will be the string representation of the UUID. For a sequence, it
//        will be an array of other attributes.
class DEVICE_BLUETOOTH_EXPORT BluetoothServiceAttributeValueBlueZ {
 public:
  enum Type { NULLTYPE = 0, UINT, INT, UUID, STRING, BOOL, SEQUENCE, URL };

  using Sequence = std::vector<BluetoothServiceAttributeValueBlueZ>;

  BluetoothServiceAttributeValueBlueZ();
  BluetoothServiceAttributeValueBlueZ(Type type,
                                      size_t size,
                                      std::optional<base::Value> value);
  explicit BluetoothServiceAttributeValueBlueZ(
      std::unique_ptr<Sequence> sequence);
  BluetoothServiceAttributeValueBlueZ(
      const BluetoothServiceAttributeValueBlueZ& attribute);
  BluetoothServiceAttributeValueBlueZ& operator=(
      const BluetoothServiceAttributeValueBlueZ& attribute);
  ~BluetoothServiceAttributeValueBlueZ();

  Type type() const { return type_; }
  size_t size() const { return size_; }
  bool is_sequence() const { return type_ == Type::SEQUENCE; }
  const Sequence& sequence() const { return *sequence_.get(); }
  const base::Value& value() const { return *value_; }

 private:
  Type type_;
  size_t size_;
  std::optional<base::Value> value_;
  std::unique_ptr<Sequence> sequence_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_SERVICE_ATTRIBUTE_VALUE_BLUEZ_H_
