// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_

#include "third_party/blink/renderer/core/workers/worklet.h"
#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_pending_generator_registry.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

extern DocumentPaintDefinition* const kInvalidDocumentPaintDefinition;

class CSSPaintImageGeneratorImpl;

// Manages a paint worklet:
// https://drafts.css-houdini.org/css-paint-api/#dom-css-paintworklet
class MODULES_EXPORT PaintWorklet : public Worklet,
                                    public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(PaintWorklet);
  WTF_MAKE_NONCOPYABLE(PaintWorklet);

 public:
  static const char kSupplementName[];

  // At this moment, paint worklet allows at most two global scopes at any time.
  static const wtf_size_t kNumGlobalScopes;
  static PaintWorklet* From(LocalDOMWindow&);
  static PaintWorklet* Create(LocalFrame*);

  ~PaintWorklet() override;

  void AddPendingGenerator(const String& name, CSSPaintImageGeneratorImpl*);
  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const String& name,
                             const ImageResourceObserver&,
                             const FloatSize& container_size,
                             const CSSStyleValueVector*);

  typedef HeapHashMap<String, Member<DocumentPaintDefinition>>
      DocumentDefinitionMap;
  DocumentDefinitionMap& GetDocumentDefinitionMap() {
    return document_definition_map_;
  }
  void Trace(blink::Visitor*) override;

 protected:
  explicit PaintWorklet(LocalFrame*);

  // Since paint worklet has more than one global scope, we MUST override this
  // function and provide our own selection logic.
  wtf_size_t SelectGlobalScope() final;
  wtf_size_t GetActiveGlobalScopeForTesting() { return active_global_scope_; }

 private:
  friend class PaintWorkletTest;

  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  // This function calculates the number of paints to use before switching
  // global scopes.
  virtual int GetPaintsBeforeSwitching();
  // This function calculates the next global scope to switch to.
  virtual wtf_size_t SelectNewGlobalScope();

  Member<PaintWorkletPendingGeneratorRegistry> pending_generator_registry_;
  DocumentDefinitionMap document_definition_map_;

  // The last document paint frame a paint worklet painted on. This is used to
  // tell when we begin painting on a new frame.
  size_t active_frame_count_ = 0u;
  // The current global scope being used for painting.
  wtf_size_t active_global_scope_ = 0u;
  // The number of paint calls remaining before Paint will select a new global
  // scope. SelectGlobalScope resets this at the beginning of each frame.
  int paints_before_switching_global_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_WORKLET_H_
