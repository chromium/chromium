// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_TREE_LINKER_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_TREE_LINKER_REGISTRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ModuleTreeLinker;

// ModuleTreeLinkerRegistry keeps active ModuleTreeLinkers alive.
class CORE_EXPORT ModuleTreeLinkerRegistry final
    : public GarbageCollected<ModuleTreeLinkerRegistry>,
      public NameClient {
 public:
  ModuleTreeLinkerRegistry() = default;

  void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "ModuleTreeLinkerRegistry";
  }

 private:
  friend class ModuleTreeLinker;
  void AddFetcher(ModuleTreeLinker*);
  void ReleaseFinishedFetcher(ModuleTreeLinker*);

  HeapHashSet<Member<ModuleTreeLinker>> active_tree_linkers_;
};

}  // namespace blink

#endif
