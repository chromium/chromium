// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_VR_PUBLIC_MOJOM_RGBA_TUPLE_F16_H_
#define DEVICE_VR_PUBLIC_MOJOM_RGBA_TUPLE_F16_H_

#include <stddef.h>
#include <stdint.h>

namespace device {

struct RgbaTupleF16 {
  // Because C++ does not have a native 16-bit floating point type,
  // the components are stored as |uint16_t|s.
  using Component = uint16_t;
  static constexpr size_t kNumComponents = 4;

  RgbaTupleF16() : RgbaTupleF16(0, 0, 0, 0) {}
  RgbaTupleF16(uint16_t red, uint16_t green, uint16_t blue, uint16_t alpha)
      : components{red, green, blue, alpha} {}

  uint16_t red() const { return components[0]; }
  void set_red(uint16_t red) { components[0] = red; }
  uint16_t green() const { return components[1]; }
  void set_green(uint16_t green) { components[1] = green; }
  uint16_t blue() const { return components[2]; }
  void set_blue(uint16_t blue) { components[2] = blue; }
  uint16_t alpha() const { return components[3]; }
  void set_alpha(uint16_t alpha) { components[3] = alpha; }

  uint16_t components[kNumComponents];
};

static_assert(sizeof(RgbaTupleF16) == sizeof(RgbaTupleF16::Component) *
                                          RgbaTupleF16::kNumComponents,
              "RgbaTupleF16 must be contiguous");

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_RGBA_TUPLE_F16_H_
