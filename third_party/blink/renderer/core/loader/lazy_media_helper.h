// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LAZY_MEDIA_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LAZY_MEDIA_HELPER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class HTMLMediaElement;
class LocalFrame;

// Contains helper functions to deal with the lazy loading logic of media
// elements (video and audio).
class CORE_EXPORT LazyMediaHelper final {
  STATIC_ONLY(LazyMediaHelper);

 public:
  // Start monitoring an element for viewport intersection.
  static void StartMonitoring(Element* element);

  // Stop monitoring an element.
  static void StopMonitoring(Element* element);

  // Determine if media loading should be deferred based on the loading
  // attribute and other conditions.
  static bool ShouldDeferMediaLoad(LocalFrame& frame,
                                   HTMLMediaElement* media_element);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LAZY_MEDIA_HELPER_H_
