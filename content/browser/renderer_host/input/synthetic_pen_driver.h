// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_

#include "content/browser/renderer_host/input/synthetic_mouse_driver.h"

namespace content {

class SyntheticPenDriver : public SyntheticMouseDriver {
 public:
  SyntheticPenDriver();

  SyntheticPenDriver(const SyntheticPenDriver&) = delete;
  SyntheticPenDriver& operator=(const SyntheticPenDriver&) = delete;

  ~SyntheticPenDriver() override;

  void Leave(int index = 0) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_PEN_DRIVER_H_
