// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_AVAILABILITY_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_AVAILABILITY_CALLBACKS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/presentation/presentation_promise_property.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// PresentationAvailabilityCallback resolves or rejects underlying promise
// depending on the availability result.
// TODO(crbug.com/749327): Consider removing this class and have
// PresentationAvailabilityState use PresentationAvailabilityProperty directly.
class MODULES_EXPORT PresentationAvailabilityCallbacks
    : public GarbageCollected<PresentationAvailabilityCallbacks> {
 public:
  PresentationAvailabilityCallbacks(PresentationAvailabilityProperty*,
                                    const WTF::Vector<KURL>&);
  virtual ~PresentationAvailabilityCallbacks();

  virtual void Resolve(bool value);
  virtual void RejectAvailabilityNotSupported();

  void Trace(blink::Visitor*);

 private:
  Member<PresentationAvailabilityProperty> resolver_;
  const WTF::Vector<KURL> urls_;

  DISALLOW_COPY_AND_ASSIGN(PresentationAvailabilityCallbacks);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_AVAILABILITY_CALLBACKS_H_
