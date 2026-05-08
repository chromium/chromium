// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_AUTOFILL_DATA_EXTRACTION_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_AUTOFILL_DATA_EXTRACTION_UTILS_H_

#import <optional>
#import <string>

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

// TODO(crbug.com/490114734): Share the logic in
// components/autofill/content/browser/integrators/actor/autofill_annotations_provider_impl.cc
// instead of reimplementing it here.

namespace web {
class WebState;
}  // namespace web

// Describes the reasons why a field should be redacted. Mirror of the
// private AutofillFieldRedactionReason blink::AutofillAnnotationsProviderImpl.
enum class AutofillFieldRedactionReason {
  kNoRedactionNeeded,
  kShouldRedactForPayments,
};

// Context passed to APC extraction functions containing information required to
// fetch Autofill data for the page.
struct AutofillExtractionContext {
 public:
  base::WeakPtr<web::WebState> web_state = nullptr;

  // The frame token of the current frame being extracted.
  std::optional<autofill::LocalFrameToken> frame_token;

  // Maps complex Autofill section identifiers (frame token + renderer ID +
  // section name) to sequential integer IDs (0, 1...) for the APC proto. This
  // mapping is shared across the entire page extraction to ensure consistent
  // section IDs for all fields belonging to the same autofill section.
  raw_ptr<base::flat_map<std::string, uint32_t>> section_numbers = nullptr;

  // True if credit card redactions should be applied when setting APC
  // redaction decisions based on Autofill metadata.
  bool extract_autofill_credit_card_redactions = false;

  AutofillExtractionContext();
  AutofillExtractionContext(
      base::WeakPtr<web::WebState> web_state,
      std::optional<autofill::LocalFrameToken> frame_token,
      bool extract_autofill_credit_card_redactions,
      raw_ptr<base::flat_map<std::string, uint32_t>> section_numbers);
  AutofillExtractionContext(const AutofillExtractionContext&) = delete;
  AutofillExtractionContext& operator=(const AutofillExtractionContext&) =
      delete;
  ~AutofillExtractionContext();
};

// Contains the structured metadata of an Autofill Field for APC.
struct AutofillFieldMetadata {
  // Sequential integer assigned to this field's Autofill section.
  uint32_t section_id;

  // The coarse field type (e.g. ADDRESS, CREDIT_CARD).
  optimization_guide::proto::CoarseAutofillFieldType coarse_field_type;

  // The reason why this field should be redacted based on Autofill type.
  AutofillFieldRedactionReason redaction_reason;
};

// Returns the AutofillFieldRedactionReason for a set of field types. If
// multiple field types require redacting, one reason will be chosen at random
// (based on set iteration order).
AutofillFieldRedactionReason GetRedactionReason(
    const autofill::FieldTypeSet& field_types);

// Converts the autofill redaction reason to the APC proto RedactionDecision.
optimization_guide::proto::RedactionDecision
ConvertAutofillFieldRedactionReason(
    const optimization_guide::proto::FormControlData& form_control_data,
    AutofillFieldRedactionReason redaction_reason);

// Evaluates whether the APC `redaction_decision` should result in the content
// being redacted.
bool ShouldRedactContent(
    optimization_guide::proto::RedactionDecision redaction_decision,
    const AutofillExtractionContext& autofill_context);

// Fetches the AutofillFieldMetadata given an extraction context and a node ID.
std::optional<AutofillFieldMetadata> GetAutofillFieldData(
    int32_t dom_node_id,
    AutofillExtractionContext& autofill_context);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_AUTOFILL_DATA_EXTRACTION_UTILS_H_
