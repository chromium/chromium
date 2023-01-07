// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_

#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// See the comment on CustomWrappableAdaptor.
class PLATFORM_EXPORT CustomWrappable
    : public GarbageCollected<CustomWrappable>,
      public NameClient {
 public:
  CustomWrappable(const CustomWrappable&) = delete;
  CustomWrappable& operator=(const CustomWrappable&) = delete;
  ~CustomWrappable() override = default;
  virtual void Trace(Visitor*) const {}
  const char* NameInHeapSnapshot() const override { return "CustomWrappable"; }

 protected:
  CustomWrappable() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_CUSTOM_WRAPPABLE_H_
