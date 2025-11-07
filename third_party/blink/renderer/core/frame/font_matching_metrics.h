// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// Traces font lookups in perfetto.
class FontMatchingMetrics {
 public:
  // Create a FontMatchingMetrics objects for a document or a worker. The
  // corresponding ExecutionContext `execution_context` must outlive this.
  explicit FontMatchingMetrics(ExecutionContext* execution_context);

  // Called when a page attempts to match a font family, and the font family is
  // available.
  void ReportSuccessfulFontFamilyMatch(const AtomicString& font_family_name);

  // Called when a page attempts to match a font family, and the font family is
  // not available.
  void ReportFailedFontFamilyMatch(const AtomicString& font_family_name);

  // Reports a font listed in a @font-face src:local rule that successfully
  // matched.
  void ReportSuccessfulLocalFontMatch(const AtomicString& font_name);

  // Reports a font listed in a @font-face src:local rule that didn't
  // successfully match.
  void ReportFailedLocalFontMatch(const AtomicString& font_name);

 private:
  // Reports a local font's existence was looked up by a name, but its actual
  // font data may or may not have been loaded. This includes lookups where the
  // name is allowed to match full font names or family names.
  void ReportLocalFontExistenceByUniqueOrFamilyName(
      const AtomicString& font_name,
      bool font_exists);

  WeakPersistent<ExecutionContext> execution_context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FONT_MATCHING_METRICS_H_
