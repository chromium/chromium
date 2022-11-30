// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_
#define EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_

#include "base/memory/weak_ptr.h"
#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {
class ScriptContext;

// Native functions for JS to run callbacks upon RenderFrame events.
class RenderFrameObserverNatives : public ObjectBackedNativeHandler {
 public:
  explicit RenderFrameObserverNatives(ScriptContext* context);

  RenderFrameObserverNatives(const RenderFrameObserverNatives&) = delete;
  RenderFrameObserverNatives& operator=(const RenderFrameObserverNatives&) =
      delete;

  ~RenderFrameObserverNatives() override;

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void Invalidate() override;

  // Runs a callback upon creation of new document element inside a render frame
  // (document.documentElement).
  void OnDocumentElementCreated(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  void InvokeCallback(v8::Global<v8::Function> callback, bool succeeded);

  base::WeakPtrFactory<RenderFrameObserverNatives> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDER_FRAME_OBSERVER_NATIVES_H_
