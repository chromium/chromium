// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CONTEXT_SNAPSHOT_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CONTEXT_SNAPSHOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class DOMWrapperWorld;

// This class contains helper functions to take and use a V8 context snapshot.
//
// The V8 context snapshot is taken by tools/v8_context_snapshot/ when Chromium
// is built, and is used when Blink creates a new V8 context. When to build or
// to use the V8 context snapshot, you have a table of references of C++
// callbacks exposed to V8.
//
// A V8 context snapshot contains:
// - Interface templates of Window, EventTarget, Node, Document, and
//   HTMLDocument.
// - Two types of V8 contexts; one is for the main world, and the other is for
//   other worlds.
// - HTMLDocument's wrapper (window.document) in the context for the main
//   world.
//
// Currently, the V8 context snapshot supports only the main thread. If it is
// the main world, we need a special logic to serialize / deserialize
// window.document (so only HTMLDocument is supported on the main world).
// Worker threads are not yet supported.
class CORE_EXPORT V8ContextSnapshot {
  STATIC_ONLY(V8ContextSnapshot);

 public:
  static v8::Local<v8::Context> CreateContextFromSnapshot(
      v8::Isolate*,
      const DOMWrapperWorld&,
      v8::ExtensionConfiguration*,
      v8::Local<v8::Object> global_proxy,
      Document*);

  // If the context was created from the snapshot, installs conditionally
  // enabled features on some v8::Object's in the context and returns true.
  // Otherwise, does nothing and returns false.
  static bool InstallConditionalFeatures(v8::Local<v8::Context>, Document*);

  static void EnsureInterfaceTemplates(v8::Isolate*);

  // Do not call this in production.
  static v8::StartupData TakeSnapshot();

 private:
  static v8::StartupData SerializeInternalField(v8::Local<v8::Object> holder,
                                                int index,
                                                void* data);
  static void DeserializeInternalField(v8::Local<v8::Object> holder,
                                       int index,
                                       v8::StartupData payload,
                                       void* data);
  static bool CanCreateContextFromSnapshot(v8::Isolate*,
                                           const DOMWrapperWorld&,
                                           Document*);

  static void EnsureInterfaceTemplatesForWorld(v8::Isolate*,
                                               const DOMWrapperWorld&);

  static void TakeSnapshotForWorld(v8::SnapshotCreator*,
                                   const DOMWrapperWorld&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_CONTEXT_SNAPSHOT_H_
