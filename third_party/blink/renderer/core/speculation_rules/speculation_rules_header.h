// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_HEADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_HEADER_H_

#include <utility>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalDOMWindow;
class ResourceResponse;
class SecurityContext;
enum class SpeculationRulesLoadOutcome;

// Responsible for parsing the Speculation-Rules header.
//
// This includes additional logic to deal with its interaction with the origin
// trial system, while this feature is experimental.
class SpeculationRulesHeader {
 public:
  // This does all of the below -- it determines whether fetching from the
  // Speculation-Rules header is possible, tries to enable it if it's not on but
  // could be turned on, and then initiates any speculation rules fetches that
  // are required.
  CORE_EXPORT static void ProcessHeadersForDocumentResponse(
      const ResourceResponse&,
      LocalDOMWindow&);

 private:
  SpeculationRulesHeader();
  ~SpeculationRulesHeader();

  // Parse the respective headers. Speculation-Rules must be parsed first, since
  // it affects which origin trial tokens are considered potentially
  // significant.
  void ParseSpeculationRulesHeader(const String& header_value,
                                   const KURL& base_url);
  void ParseOriginTrialHeader(const String& header_value, SecurityContext&);

  // Possibly enables features given the found origin trial tokens.
  void MaybeEnableFeatureFromOriginTrial(ExecutionContext&);

  // If errors were encountered, report the unsuccessful outcome for metrics
  // purposes and also inform the developer.
  void ReportErrors(LocalDOMWindow&);

  // Start fetching the rule sets found in the Speculation-Rules header.
  void StartFetches(Document&);

  // Successfully parsed speculation rules fetches to make.
  Vector<KURL> urls_;

  // Error information to be reported if the feature is found to be enabled.
  Vector<std::pair<SpeculationRulesLoadOutcome, String>> errors_;

  // Potentially valid origin trial tokens.
  // These are the tokens which:
  // - enable a trial which can be enabled when paired with Speculation-Rules
  // - do not match the first-party origin
  // - allow third-party use
  // - have a third-party origin which matches an origin in `urls_`
  Vector<String> origin_trial_tokens_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_HEADER_H_
