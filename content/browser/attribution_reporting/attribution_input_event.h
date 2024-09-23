// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#include <stdint.h>

#include <optional>

#include "base/android/scoped_java_ref.h"
#endif

namespace content {

// A struct that wraps the input event associated with a navigation.
struct CONTENT_EXPORT AttributionInputEvent {
  AttributionInputEvent();
  ~AttributionInputEvent();

  AttributionInputEvent(const AttributionInputEvent&);
  AttributionInputEvent& operator=(const AttributionInputEvent&);

  AttributionInputEvent(AttributionInputEvent&&);
  AttributionInputEvent& operator=(AttributionInputEvent&&);

  bool operator==(const AttributionInputEvent& other) const;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> input_event;
  std::optional<uint32_t> input_event_id;
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INPUT_EVENT_H_
