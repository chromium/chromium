// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_DOM_TRANSLATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_DOM_TRANSLATION_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Translation;
class ExecutionContext;

// The class that is exposed to the DOM window or worker for the developers
// to interact with the translation API.
class DOMTranslation final : public GarbageCollected<DOMTranslation>,
                             public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static DOMTranslation& From(ExecutionContext&);
  static Translation* translation(ExecutionContext&);

  explicit DOMTranslation(ExecutionContext&);
  DOMTranslation(const DOMTranslation&) = delete;
  DOMTranslation& operator=(const DOMTranslation&) = delete;

  void Trace(Visitor*) const override;

 private:
  Member<Translation> translation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ON_DEVICE_TRANSLATION_DOM_TRANSLATION_H_
