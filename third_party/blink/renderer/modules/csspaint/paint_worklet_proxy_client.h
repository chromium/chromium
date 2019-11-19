// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_style_property_map.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_painter.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

namespace blink {

class DocumentPaintDefinition;
class PaintWorklet;
class WorkletGlobalScope;

// Mediates between the (multiple) PaintWorkletGlobalScopes on the worklet
// thread and the (single) PaintWorkletPaintDispatcher on the non-worklet
// threads. PaintWorkletProxyClient is responsible both for informing the
// dispatcher about its existence once all global scopes are registered, as well
// as choosing the global scope to use for any given paint request.
//
// This class is constructed on the main thread but it is used in the worklet
// backing thread. The entire class is used for off-thread CSS Paint.
class MODULES_EXPORT PaintWorkletProxyClient
    : public GarbageCollected<PaintWorkletProxyClient>,
      public Supplement<WorkerClients>,
      public PaintWorkletPainter {
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorkletProxyClient);
  DISALLOW_COPY_AND_ASSIGN(PaintWorkletProxyClient);

 public:
  // blink::Supplement hook to retrieve the PaintWorkletProxyClient for a given
  // WorkerClients.
  static const char kSupplementName[];
  static PaintWorkletProxyClient* From(WorkerClients*);

  // Create the PaintWorkletProxyClient for a given PaintWorklet, represented by
  // its unique |worklet_id|.
  static PaintWorkletProxyClient* Create(Document*, int worklet_id);

  PaintWorkletProxyClient(
      int worklet_id,
      PaintWorklet*,
      base::WeakPtr<PaintWorkletPaintDispatcher> compositor_paintee,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_host_queue);
  ~PaintWorkletProxyClient() override = default;

  // PaintWorkletPainter implementation.
  int GetWorkletId() const override { return worklet_id_; }
  sk_sp<PaintRecord> Paint(
      const CompositorPaintWorkletInput*,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values) override;

  // Add a global scope to the PaintWorkletProxyClient.
  virtual void AddGlobalScope(WorkletGlobalScope*);

  // Register a paint definition for this PaintWorklet.
  // See https://drafts.css-houdini.org/css-paint-api-1/#paint-definition
  void RegisterCSSPaintDefinition(const String& name,
                                  CSSPaintDefinition*,
                                  ExceptionState&);

  // Dispose of the PaintWorkletProxyClient. Called when the worklet global
  // scopes are being torn down. May be called once per global scope - calls
  // after the first have no effect.
  void Dispose();

  void Trace(blink::Visitor*) override;

  // Hooks for testing.
  const Vector<CrossThreadPersistent<PaintWorkletGlobalScope>>&
  GetGlobalScopesForTesting() const {
    return global_scopes_;
  }
  const HashMap<String, std::unique_ptr<DocumentPaintDefinition>>&
  DocumentDefinitionMapForTesting() const {
    return document_definition_map_;
  }
  scoped_refptr<base::SingleThreadTaskRunner> MainThreadTaskRunnerForTesting()
      const {
    return main_thread_runner_;
  }
  void SetMainThreadTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> runner) {
    main_thread_runner_ = runner;
  }

  double DevicePixelRatio() const { return device_pixel_ratio_; }

 private:
  friend class PaintWorkletGlobalScopeTest;
  friend class PaintWorkletProxyClientTest;
  FRIEND_TEST_ALL_PREFIXES(PaintWorkletProxyClientTest,
                           PaintWorkletProxyClientConstruction);

  void ApplyAnimatedPropertyOverrides(
      PaintWorkletStylePropertyMap* style_map,
      const CompositorPaintWorkletJob::AnimatedPropertyValues&
          animated_property_values);

  // Store the device pixel ratio here so it can be used off main thread
  double device_pixel_ratio_;

  // The |paint_dispatcher_| is shared between all PaintWorklets on the same
  // Renderer process, and is responsible for dispatching paint calls from the
  // non-worklet threads to the correct PaintWorkletProxyClient on its worklet
  // thread. PaintWorkletProxyClient requires a reference to the dispatcher in
  // order to register and unregister itself.
  //
  // PaintWorkletPaintDispatcher is only accessed on the compositor, so we store
  // a base::SingleThreadTaskRunner to post to it.
  base::WeakPtr<PaintWorkletPaintDispatcher> paint_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_host_queue_;

  // The unique id for the PaintWorklet that this class is a proxy client for.
  const int worklet_id_;

  // The set of global scopes registered for this PaintWorklet. Multiple global
  // scopes are used to enforce statelessness - paint instances may have their
  // global scope changed at random which means they cannot easily store state.
  Vector<CrossThreadPersistent<PaintWorkletGlobalScope>> global_scopes_;

  // The current state of the proxy client. PaintWorkletProxyClient is initially
  // uninitialized. Once all global scopes are registered, it is considered
  // working - unless it is disposed of before this happens in which case it
  // stays in the disposed state.
  enum RunState { kUninitialized, kWorking, kDisposed } state_;

  // Stores the paint definitions as they are registered from the global scopes.
  // For a given named paint definition, all global scopes must report the same
  // DocumentPaintDefinition or the definition is invalid. Additionally we
  // cannot tell the main thread about a paint definition until all global
  // scopes have registered it.
  //
  // The value of an entry being nullptr means that it is an invalid definition.
  HashMap<String, std::unique_ptr<DocumentPaintDefinition>>
      document_definition_map_;

  // The main thread needs to know about registered paint definitions so that it
  // can invalidate any associated paint objects and correctly create the paint
  // instance input state for the object, etc. We communicate with it via a
  // handle to the PaintWorklet called via a stored task runner.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;
  CrossThreadWeakPersistent<PaintWorklet> paint_worklet_;
};

void MODULES_EXPORT ProvidePaintWorkletProxyClientTo(WorkerClients*,
                                                     PaintWorkletProxyClient*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_PROXY_CLIENT_H_
