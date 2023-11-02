// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_RGB_VALUE_H_
#define REMOTING_TEST_RGB_VALUE_H_

#include <stdint.h>

namespace remoting {
namespace test {

// Used to store a RGB color.
// Default constructor will initialize the value to black.
struct RGBValue {
  RGBValue();
  RGBValue(uint8_t r, uint8_t g, uint8_t b);
  ~RGBValue();

  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_RGB_VALUE_H_
