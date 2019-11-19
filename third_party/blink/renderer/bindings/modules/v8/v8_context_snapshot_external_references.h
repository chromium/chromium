// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_V8_CONTEXT_SNAPSHOT_EXTERNAL_REFERENCES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_V8_CONTEXT_SNAPSHOT_EXTERNAL_REFERENCES_H_

#include <cstdint>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// V8ContextSnapshotExternalReferences::GetTable() provides a table of pointers
// of C++ callbacks exposed to V8. The table contains C++ callbacks for DOM
// attribute getters, setters, DOM methods, wrapper type info etc.
class MODULES_EXPORT V8ContextSnapshotExternalReferences {
  STATIC_ONLY(V8ContextSnapshotExternalReferences);

 public:
  // The definition of this method is auto-generated in
  // v8_context_snapshot_external_references.cc.
  static const intptr_t* GetTable();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_MODULES_V8_V8_CONTEXT_SNAPSHOT_EXTERNAL_REFERENCES_H_
