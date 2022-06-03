// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_EYE_DROPPER_H_
#define CONTENT_PUBLIC_BROWSER_EYE_DROPPER_H_

namespace content {

// Interface for an eye dropper window.
class EyeDropper {
 public:
  virtual ~EyeDropper() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_EYE_DROPPER_H_
