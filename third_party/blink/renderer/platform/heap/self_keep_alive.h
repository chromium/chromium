// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_H_

#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

// SelfKeepAlive<Object> is the idiom to use for objects that have to keep
// themselves temporarily alive and cannot rely on there being some
// external reference in that interval:
//
//  class Opener : public GarbageCollected<Opener> {
//   public:
//    Opener() : keep_alive_(PERSISTENT_FROM_HERE) {}
//    ...
//    void Open() {
//      // Retain a self-reference while in an Open()ed state:
//      keep_alive_ = this;
//      ....
//    }
//
//    void Close() {
//      // Clear self-reference that ensured we were kept alive while opened.
//      keep_alive_.Clear();
//      ....
//    }
//
//   private:
//    ...
//    SelfKeepAlive<Opener> keep_alive_;
//  };
//
// The responsibility to call Clear() in a timely fashion resides with the
// implementation of the object.
//
//
template <typename Self>
class SelfKeepAlive final {
  DISALLOW_NEW();

 public:
  explicit SelfKeepAlive(const PersistentLocation& location)
      : keep_alive_(location) {}
  SelfKeepAlive(const PersistentLocation& location, Self* self)
      : keep_alive_(location) {
    Assign(self);
  }

  SelfKeepAlive& operator=(Self* self) {
    Assign(self);
    return *this;
  }

  void Clear() { keep_alive_.Clear(); }

  explicit operator bool() const { return keep_alive_; }

 private:
  void Assign(Self* self) {
    DCHECK(!keep_alive_ || keep_alive_.Get() == self);
    keep_alive_ = self;
  }

  GC_PLUGIN_IGNORE("420515")
  Persistent<Self> keep_alive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_H_
