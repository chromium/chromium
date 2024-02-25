// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_STRING_UTIL_ICU_H_
#define DEVICE_BLUETOOTH_STRING_UTIL_ICU_H_

#include <string_view>

#include "device/bluetooth/bluetooth_export.h"

namespace device {
// Returns true if the string contains any Unicode Graphic characters as defined
// by http://www.unicode.org/reports/tr18/#graph
bool DEVICE_BLUETOOTH_EXPORT HasGraphicCharacter(std::string_view s);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_STRING_UTIL_ICU_H_
