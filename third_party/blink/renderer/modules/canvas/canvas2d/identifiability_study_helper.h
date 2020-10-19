// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_

#include <stdint.h>

#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

// Text operations supported on different canvas types; the intent is to use
// these values (and any input supplied to these operations) to build a running
// hash that reprensents the sequence of text operations performed on the
// canvas. A hash of all other canvas operations is maintained by hashing the
// serialized PaintOps produced by the canvas in CanvasResourceProvider.
//
// If a canvas method to exfiltrate the canvas buffer is called by a script
// (getData(), etc.), this hash will be uploaded to UKM along with a hash of the
// canvas buffer data.
//
// **Don't renumber after the privacy budget study has started to ensure
// consistency.**
enum class CanvasOps {
  // BaseRenderingContext2D methods.
  kSetStrokeStyle,
  kSetFillStyle,
  kCreateLinearGradient,
  kCreateRadialGradient,
  kCreatePattern,
  kSetTextAlign,
  kSetTextBaseline,
  // CanvasRenderingContext2D / OffscreenCanvasRenderingContext2D methods.
  kSetFont,
  kFillText,
  kStrokeText,
  // CanvasGradient methods.
  kAddColorStop,
};

// A helper class to simplify maintaining the current text digest for the canvas
// context. An operation count is also maintained to limit the performance
// impact of the study.
class IdentifiabilityStudyHelper {
 public:
  template <typename... Ts>
  void MaybeUpdateBuilder(Ts... tokens) {
    if (!IdentifiabilityStudySettings::Get()->IsTypeAllowed(
            blink::IdentifiableSurface::Type::kCanvasReadback)) {
      return;
    }
    if (operation_count_ >= max_operations_) {
      encountered_skipped_ops_ = true;
      return;
    }
    AddTokens(tokens...);
    operation_count_++;
  }

  IdentifiableToken GetToken() const { return builder_.GetToken(); }

  bool encountered_skipped_ops() const { return encountered_skipped_ops_; }

  bool encountered_sensitive_ops() const { return encountered_sensitive_ops_; }

  void set_encountered_sensitive_ops() { encountered_sensitive_ops_ = true; }

  // For testing, allows scoped changing the max number of operations for all
  // IdentifiabilityStudyHelper instances.
  class ScopedMaxOperationsSetter {
   public:
    explicit ScopedMaxOperationsSetter(int new_value)
        : old_max_operations_(IdentifiabilityStudyHelper::max_operations_) {
      IdentifiabilityStudyHelper::max_operations_ = new_value;
    }
    ~ScopedMaxOperationsSetter() {
      IdentifiabilityStudyHelper::max_operations_ = old_max_operations_;
    }

   private:
    const int old_max_operations_;
  };

 private:
  // Note that primitives are implicitly converted to IdentifiableTokens
  template <typename... Ts>
  void AddTokens(IdentifiableToken token, Ts... args) {
    builder_.AddToken(token);
    AddTokens(args...);
  }
  void AddTokens() {}

  static MODULES_EXPORT int max_operations_;

  IdentifiableTokenBuilder builder_;
  int operation_count_ = 0;

  // If true, at least one op was skipped completely, for performance reasons.
  bool encountered_skipped_ops_ = false;

  // If true, encountered at least one "sensitive" operation -- for instance,
  // strings may contain PII, so we only use a 16-bit digest for such strings.
  //
  // This must be set manually by calling set_encountered_sensitive_ops().
  bool encountered_sensitive_ops_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_IDENTIFIABILITY_STUDY_HELPER_H_
