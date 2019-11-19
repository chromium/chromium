// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_USE_COUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_USE_COUNTER_H_

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// TODO(yhirano): Remove this.
using WebFeature = mojom::WebFeature;

// Definition for UseCounter features can be found in:
// third_party/blink/public/mojom/web_feature/web_feature.mojom
//
// UseCounter is used for counting the number of times features of
// Blink are used on real web pages and help us know commonly
// features are used and thus when it's safe to remove or change them. It's
// counting whether a feature is used in a context (e.g., a page), so calling
// a counting function multiple times for the same UseCounter with the same
// feature will be ignored.
//
// The Chromium Content layer controls what is done with this data.
//
// For instance, in Google Chrome, these counts are submitted anonymously
// through the UMA histogram recording system in Chrome for users who have the
// "Automatically send usage statistics and crash reports to Google" setting
// enabled:
// http://www.google.com/chrome/intl/en/privacy.html
//
// This is a pure virtual interface class with some utility static functions.
class UseCounter : public GarbageCollectedMixin {
 public:
  static void Count(UseCounter* use_counter, mojom::WebFeature feature) {
    if (use_counter) {
      use_counter->CountUse(feature);
    }
  }
  static void Count(UseCounter& use_counter, mojom::WebFeature feature) {
    use_counter.CountUse(feature);
  }

 public:
  UseCounter() = default;
  virtual ~UseCounter() = default;

  // Counts a use of the given feature. Repeated calls are ignored.
  virtual void CountUse(mojom::WebFeature feature) = 0;

  // Counts and reports a use of the given (deprecated) feature. Repeated
  // calls are ignored.
  virtual void CountDeprecation(mojom::WebFeature feature) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(UseCounter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_USE_COUNTER_H_
