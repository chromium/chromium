// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNIFIED_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/heap/impl/marking_visitor.h"
#include "v8/include/v8.h"

namespace v8 {
class EmbedderHeapTracer;
}

namespace blink {

struct WrapperTypeInfo;

// Marking visitor for unified heap garbage collections. Extends the regular
// Oilpan marking visitor by also providing write barriers and visitation
// methods that allow for announcing reachable objects to V8. Visitor can be
// used from any thread.
class PLATFORM_EXPORT UnifiedHeapMarkingVisitorBase {
 public:
  virtual ~UnifiedHeapMarkingVisitorBase() = default;

 protected:
  UnifiedHeapMarkingVisitorBase(ThreadState*, v8::Isolate*, int);

  // Visitation methods that announce reachable wrappers to V8.
  void VisitImpl(const TraceWrapperV8Reference<v8::Value>&);

  v8::Isolate* const isolate_;
  v8::EmbedderHeapTracer* const controller_;
  V8ReferencesWorklist::View v8_references_worklist_;

 private:
  int task_id_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedHeapMarkingVisitorBase);
};

// Same as the base visitor with the difference that it is bound to main thread.
// Also implements various sorts of write barriers that should only be called
// from the main thread.
class PLATFORM_EXPORT UnifiedHeapMarkingVisitor
    : public MarkingVisitor,
      public UnifiedHeapMarkingVisitorBase {
 public:
  // Write barriers for annotating a write during incremental marking.
  static void WriteBarrier(const TraceWrapperV8Reference<v8::Value>&);
  static void WriteBarrier(v8::Isolate*,
                           v8::Local<v8::Object>&,
                           const WrapperTypeInfo*,
                           const void*);

  UnifiedHeapMarkingVisitor(ThreadState*, MarkingMode, v8::Isolate*);
  ~UnifiedHeapMarkingVisitor() override = default;

 protected:
  using Visitor::Visit;
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnifiedHeapMarkingVisitor);
};

// Same as the base visitor with the difference that it is bound to a
// concurrent thread.
class PLATFORM_EXPORT ConcurrentUnifiedHeapMarkingVisitor
    : public ConcurrentMarkingVisitor,
      public UnifiedHeapMarkingVisitorBase {
 public:
  ConcurrentUnifiedHeapMarkingVisitor(ThreadState*,
                                      MarkingMode,
                                      v8::Isolate*,
                                      int task_id);
  ~ConcurrentUnifiedHeapMarkingVisitor() override = default;

  void FlushWorklists() override;

 protected:
  using Visitor::Visit;
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConcurrentUnifiedHeapMarkingVisitor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_UNIFIED_HEAP_MARKING_VISITOR_H_
