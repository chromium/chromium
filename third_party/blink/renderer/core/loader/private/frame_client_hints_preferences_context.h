// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_FRAME_CLIENT_HINTS_PREFERENCES_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_FRAME_CLIENT_HINTS_PREFERENCES_CONTEXT_H_

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FrameClientHintsPreferencesContext final
    : public ClientHintsPreferences::Context {
  STACK_ALLOCATED();

 public:
  explicit FrameClientHintsPreferencesContext(LocalFrame*);

  void CountClientHints(mojom::WebClientHintsType) override;
  void CountPersistentClientHintHeaders() override;

 private:
  Member<LocalFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PRIVATE_FRAME_CLIENT_HINTS_PREFERENCES_CONTEXT_H_
