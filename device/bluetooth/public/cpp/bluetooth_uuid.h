// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_UUID_H_
#define DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_UUID_H_

#include <string>
#include <vector>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <rpc.h>

#include "base/strings/string_piece_forward.h"
#endif  // defined(OS_WIN)

namespace device {

// Opaque wrapper around a Bluetooth UUID. Instances of UUID represent the
// 128-bit universally unique identifiers (UUIDs) of profiles and attributes
// used in Bluetooth based communication, such as a peripheral's services,
// characteristics, and characteristic descriptors. An instance are
// constructed using a string representing 16, 32, or 128 bit UUID formats.
class BluetoothUUID {
 public:
  // Possible representation formats used during construction.
  enum Format { kFormatInvalid, kFormat16Bit, kFormat32Bit, kFormat128Bit };

  // Single argument constructor. |uuid| can be a 16, 32, or 128 bit UUID
  // represented as a 4, 8, or 36 character string with the following
  // formats:
  //   xxxx
  //   0xxxxx
  //   xxxxxxxx
  //   0xxxxxxxxx
  //   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  //
  // 16 and 32 bit UUIDs will be internally converted to a 128 bit UUID using
  // the base UUID defined in the Bluetooth specification, hence custom UUIDs
  // should be provided in the 128-bit format. If |uuid| is in an unsupported
  // format, the result might be invalid. Use IsValid to check for validity
  // after construction.
  explicit BluetoothUUID(const std::string& uuid);

#if defined(OS_WIN)
  // Windows exclusive constructor converting a GUID structure to a
  // BluetoothUUID. This will always result in a 128 bit Format.
  explicit BluetoothUUID(GUID uuid);
#endif  // defined(OS_WIN)

  // Default constructor does nothing. Since BluetoothUUID is copyable, this
  // constructor is useful for initializing member variables and assigning a
  // value to them later. The default constructor will initialize an invalid
  // UUID by definition and the string accessors will return an empty string.
  BluetoothUUID();
  ~BluetoothUUID();

#if defined(OS_WIN)
  // The canonical UUID string format is device::BluetoothUUID.value().
  static GUID GetCanonicalValueAsGUID(base::StringPiece uuid);
#endif  // defined(OS_WIN)

  // Returns true, if the UUID is in a valid canonical format.
  bool IsValid() const;

  // Returns the representation format of the UUID. This reflects the format
  // that was provided during construction.
  Format format() const { return format_; }

  // Returns the value of the UUID as a string. The representation format is
  // based on what was passed in during construction. For the supported sizes,
  // this representation can have the following formats:
  //   - 16 bit:  xxxx
  //   - 32 bit:  xxxxxxxx
  //   - 128 bit: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  // where x is a lowercase hex digit.
  const std::string& value() const { return value_; }

  // Returns the underlying 128-bit value as a string in the following format:
  //   xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  // where x is a lowercase hex digit.
  const std::string& canonical_value() const { return canonical_value_; }

  // Returns the bytes of the canonical 128-bit UUID, or empty vector if
  // invalid.
  std::vector<uint8_t> GetBytes() const;

  // Permit sufficient comparison to allow a UUID to be used as a key in a
  // std::map.
  bool operator<(const BluetoothUUID& uuid) const;

  // Equality operators.
  bool operator==(const BluetoothUUID& uuid) const;
  bool operator!=(const BluetoothUUID& uuid) const;

 private:
  // String representation of the UUID that was used during construction. For
  // the supported sizes, this representation can have the following formats:
  //   - 16 bit:  xxxx
  //   - 32 bit:  xxxxxxxx
  //   - 128 bit: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  Format format_;
  std::string value_;

  // The 128-bit string representation of the UUID.
  std::string canonical_value_;
};

// This is required by gtest to print a readable output on test failures.
void PrintTo(const BluetoothUUID& uuid, std::ostream* out);

struct BluetoothUUIDHash {
  size_t operator()(const device::BluetoothUUID& uuid) const {
    return std::hash<std::string>()(uuid.canonical_value());
  }
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_PUBLIC_CPP_BLUETOOTH_UUID_H_
