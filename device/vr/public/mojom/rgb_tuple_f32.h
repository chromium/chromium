// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_VR_PUBLIC_MOJOM_RGB_TUPLE_F32_H_
#define DEVICE_VR_PUBLIC_MOJOM_RGB_TUPLE_F32_H_

#include <stddef.h>

namespace device {

struct RgbTupleF32 {
  using Component = float;
  static constexpr size_t kNumComponents = 3;

  RgbTupleF32() : RgbTupleF32(0, 0, 0) {}
  RgbTupleF32(float red, float green, float blue)
      : components{red, green, blue} {}

  float red() const { return components[0]; }
  void set_red(float red) { components[0] = red; }
  float green() const { return components[1]; }
  void set_green(float green) { components[1] = green; }
  float blue() const { return components[2]; }
  void set_blue(float blue) { components[2] = blue; }

  float components[kNumComponents];
};

static_assert(sizeof(RgbTupleF32) ==
                  sizeof(RgbTupleF32::Component) * RgbTupleF32::kNumComponents,
              "RgbTupleF32 must be contiguous");

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_RGB_TUPLE_F32_H_
