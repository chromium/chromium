// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NAME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NAME_CLIENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/bindings/buildflags.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// NameClient provides classes with a human-readable name that can be used for
// inspecting the object graph.
//
// NameClient is aimed to provide web developers better idea about what object
// instances (or the object reference subgraph held by the instances) is
// consuming heap. It should provide actionable guidance for reducing memory for
// a given website.
//
// NameClient should be inherited for classes which:
// - is likely to be in a reference chain that is likely to hold a consierable
//   amount of memory,
// - Web developers would have a rough idea what it would mean, and
//   (The name is exposed to DevTools)
// - not ScriptWrappable (ScriptWrappable implements NameClient).
//
// Caveat:
//   NameClient should be inherited near the root of the inheritance graph
//   for Member<BaseClass> edges to be attributed correctly.
//
//   Do:
//     class Foo : public GarbageCollected<Foo>, public NameClient {...};
//
//   Don't:
//     class Bar : public GarbageCollected<Bar> {...};
//     class Baz : public Bar, public NameClient {...};
class PLATFORM_EXPORT NameClient {
 public:
  static constexpr bool HideInternalName() {
#if BUILDFLAG(RAW_HEAP_SNAPSHOTS) && \
    (defined(COMPILER_GCC) || defined(__clang__))
    return false;
#else
    return true;
#endif  // BUILDFLAG(RAW_HEAP_SNAPSHOTS)
  }

  NameClient() = default;
  ~NameClient() = default;

  // Human-readable name of this object. The DevTools heap snapshot uses
  // this method to show the object.
  virtual const char* NameInHeapSnapshot() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NameClient);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_NAME_CLIENT_H_
