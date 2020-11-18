// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_PROXY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_PROXY_CLIENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_painter.h"

namespace blink {

// This class contains the shared bits for all kinds of
// NativePaintWorkletProxyClient. Instance creation of this class is not
// allowed, but its subclasses are allowed. Each subclass should have its own
// implementation of the Paint function from the PaintWorkletPainter.
class MODULES_EXPORT NativePaintWorkletProxyClient
    : public GarbageCollected<NativePaintWorkletProxyClient>,
      public PaintWorkletPainter {
  DISALLOW_COPY_AND_ASSIGN(NativePaintWorkletProxyClient);

 public:
  ~NativePaintWorkletProxyClient() override = default;

  // PaintWorkletPainter implementation.
  int GetWorkletId() const override { return worklet_id_; }

 protected:
  explicit NativePaintWorkletProxyClient(int worklet_id)
      : worklet_id_(worklet_id) {}

 private:
  const int worklet_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_PROXY_CLIENT_H_
