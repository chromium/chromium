// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HAPTICS_HAPTICS_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HAPTICS_HAPTICS_CONTROLLER_H_

#include "third_party/blink/public/mojom/haptics/haptics.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_haptic_effect.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;

// Renderer-side implementation of navigator.playHaptics(). Applies the API's
// gating rules as a fast path and forwards to the content::HapticsService
// security boundary.
class MODULES_EXPORT HapticsController final
    : public GarbageCollected<HapticsController>,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];
  static HapticsController& From(Navigator&);

  // Web-exposed navigator.playHaptics(effect, intensity).
  static void playHaptics(Navigator&, V8HapticEffect effect, double intensity);

  explicit HapticsController(Navigator&);

  HapticsController(const HapticsController&) = delete;
  HapticsController& operator=(const HapticsController&) = delete;

  void Trace(Visitor*) const override;

 private:
  void PlayHaptics(V8HapticEffect effect, double intensity);

  // Bound lazily on first use; auto-reset when the context is destroyed.
  HeapMojoRemote<mojom::blink::HapticsService> haptics_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HAPTICS_HAPTICS_CONTROLLER_H_
