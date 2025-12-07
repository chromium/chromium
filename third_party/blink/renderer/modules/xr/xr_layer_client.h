// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_CLIENT_H_

#include "base/memory/scoped_refptr.h"

namespace blink {

class StaticBitmapImage;
class XRSession;
class XRFrameTransportDelegate;

class XrLayerClient {
 public:
  virtual ~XrLayerClient() = default;

  virtual XRSession* session() const = 0;
  virtual scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage() = 0;
  virtual XRFrameTransportDelegate* GetTransportDelegate() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_LAYER_CLIENT_H_
