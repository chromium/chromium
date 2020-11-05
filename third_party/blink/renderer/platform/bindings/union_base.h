// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

namespace blink {

namespace bindings {

// UnionBase is the common base class of all the IDL union classes.  Most
// importantly this class provides a way of type dispatching (e.g. overload
// resolutions, SFINAE technique, etc.) so that it's possible to distinguish
// IDL unions from anything else.  Also it provides a common implementation of
// IDL unions.
class PLATFORM_EXPORT UnionBase : public GarbageCollected<UnionBase> {
 public:
  virtual ~UnionBase() = default;

  virtual void Trace(Visitor*) const {}

 protected:
  UnionBase() = default;
};

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_UNION_BASE_H_
