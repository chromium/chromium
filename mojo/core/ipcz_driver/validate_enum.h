// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_VALIDATE_ENUM_H_
#define MOJO_CORE_IPCZ_DRIVER_VALIDATE_ENUM_H_

namespace mojo::core::ipcz_driver {

// Enum must have kMinValue and kMaxValue members and be non-sparse. Use this to
// validate that values from the wire are inside the expected range on receipt.
template <typename T>
bool ValidateEnum(T& value) {
  using UnderlyingType = std::underlying_type<T>::type;
  return static_cast<UnderlyingType>(value) >=
             static_cast<UnderlyingType>(T::kMinValue) &&
         static_cast<UnderlyingType>(value) <=
             static_cast<UnderlyingType>(T::kMaxValue);
}

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_VALIDATE_ENUM_H_
