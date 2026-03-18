// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_STREAMING_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_STREAMING_SANITIZER_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Node;
class Sanitizer;

class StreamingSanitizer : public GarbageCollected<StreamingSanitizer> {
 public:
  static StreamingSanitizer* CreateSafe(const Sanitizer*);
  static StreamingSanitizer* CreateUnsafe(const Sanitizer*);

  // Sanitizes a node insertion operation. Can modify element attributes, change
  // the insertion target, or discard the element. Returns the adjusted
  // insertion target, or null if the element is to be discarded.
  // This is used for streaming.
  bool Sanitize(Node* target);

  bool ShouldReplaceWithChildren(Node* target);

  void Trace(Visitor* visitor) const;

  virtual ~StreamingSanitizer() = default;
  StreamingSanitizer(const Sanitizer* sanitizer, bool safe)
      : sanitizer_(sanitizer), safe_(safe) {}

 private:
  Member<const Sanitizer> sanitizer_;
  bool safe_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_STREAMING_SANITIZER_H_
