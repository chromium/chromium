// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class LocalFrame;
class NativePaintWorkletProxyClient;
class PaintWorkletPaintDispatcher;
class SingleThreadTaskRunner;
class Thread;

// NativePaintWorklet contains the shared information by all kinds of native
// paint worklet. We allow the instance creation of its subclasses, but not this
// class. Each subclass would have its own implementation of the Paint function.
// For example, the BackgroundColorPaintWorklet takes a SkColor in its Paint
// function.
class MODULES_EXPORT NativePaintWorklet
    : public GarbageCollected<NativePaintWorklet> {
  DISALLOW_COPY_AND_ASSIGN(NativePaintWorklet);

 public:
  virtual ~NativePaintWorklet() = default;

  int WorkletId() const { return worklet_id_; }

  // Register the NativePaintWorkletProxyClient to the compositor thread that
  // will hold a cross thread persistent pointer to it. This should be called
  // during the construction of native paint worklets, to ensure that the proxy
  // client is ready on the compositor thread when dispatching a paint job.
  void RegisterProxyClient(NativePaintWorkletProxyClient*);

  // Unregister the painter to ensure that there is no memory leakage on the
  // compositor thread.
  void UnregisterProxyClient();

  virtual void Trace(Visitor*) const {}

 protected:
  explicit NativePaintWorklet(LocalFrame& local_root);

  int worklet_id_;
  base::WeakPtr<PaintWorkletPaintDispatcher> paint_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_host_queue_;
  // The worker thread that does the paint work.
  std::unique_ptr<Thread> worker_thread_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_NATIVE_PAINT_WORKLET_H_
