// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CANVAS_INPUT_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CANVAS_INPUT_PROVIDER_H_

#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class EventListener;
class HTMLCanvasElement;
class PointerEvent;
class XRInputSource;
class XRSession;

class XRCanvasInputProvider : public GarbageCollected<XRCanvasInputProvider>,
                              public NameClient {
 public:
  XRCanvasInputProvider(XRSession*, HTMLCanvasElement*);
  virtual ~XRCanvasInputProvider();

  XRSession* session() const { return session_; }
  HTMLCanvasElement* canvas() const { return canvas_; }

  // Remove all event listeners.
  void Stop();

  bool ShouldProcessEvents();

  void OnPointerDown(PointerEvent*);
  void OnPointerUp(PointerEvent*);

  XRInputSource* GetInputSource();

  virtual void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "XRCanvasInputProvider";
  }

 private:
  void UpdateInputSource(PointerEvent*);
  void ClearInputSource();

  const Member<XRSession> session_;
  Member<HTMLCanvasElement> canvas_;
  Member<EventListener> listener_;
  Member<XRInputSource> input_source_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_CANVAS_INPUT_PROVIDER_H_
