// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_H_

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Navigator;
class PresentationReceiver;
class PresentationRequest;

// Implements the main entry point of the Presentation API corresponding to the
// Presentation.idl
// See https://w3c.github.io/presentation-api/#navigatorpresentation for
// details.
class Presentation final : public ScriptWrappable,
                           public GarbageCollectedMixin {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const unsigned kSupplementIndex;
  static Presentation* presentation(Navigator&);
  explicit Presentation(Navigator&);

  void Trace(Visitor*) const override;

  PresentationRequest* defaultRequest() const;
  void setDefaultRequest(PresentationRequest*);

  PresentationReceiver* receiver();

 private:
  void MaybeInitReceiver();

  Member<Navigator> navigator_;

  // Default PresentationRequest used by the embedder.
  Member<PresentationRequest> default_request_;

  // PresentationReceiver instance. It will always be nullptr if the Blink
  // instance is not running as a presentation receiver.
  Member<PresentationReceiver> receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PRESENTATION_PRESENTATION_H_
