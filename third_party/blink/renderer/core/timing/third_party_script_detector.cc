// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/third_party_script_detector.h"

#include <cmath>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {
// kThirdPartyTechnologiesSourceLocationRegexString has to strictly follow rules
// below so the regex matching work as intended.
//
// 1. Each technology(eg. WordPress) contains exactly one capturing group in
// order to identify technologies when a pattern is matched. Non-capturing
// groups are free to use. (Ref:
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_expressions/Groups_and_backreferences#types)
// 2. Different technologies are separated by "|".
// 3. If a technology has more than one regex pattern to be matched, use "|" to
// concatenate them together within the same technology group.
// 4. The order must be consistent with Technology enum value defined in
// third_party_script_detector.h. That means i-th (0 based) group in regex
// should have (1<<i) Technology.
// 5. For better readability, please put each regex pattern on a single line
// beginning with a "|".
constexpr char kThirdPartyTechnologiesSourceLocationRegexString[] =
    // WordPress
    "(/wp-(?:content|includes)/"
    "|wp-embed\\.min\\.js)"
    // GoogleAnalytics
    "|(google-analytics\\.com/(?:ga|urchin|analytics)\\.js"
    "|googletagmanager\\.com/gtag/js)"
    // GoogleFontApi
    "|(googleapis\\.com/.+webfont)";
constexpr int kTechnologyCount = std::bit_width(
    static_cast<uint8_t>(ThirdPartyScriptDetector::Technology::kLast));
}  // namespace

// static
const char ThirdPartyScriptDetector::kSupplementName[] =
    "ThirdPartyScriptDetector";

// static
ThirdPartyScriptDetector& ThirdPartyScriptDetector::From(
    LocalDOMWindow& window) {
  ThirdPartyScriptDetector* supplement =
      Supplement<LocalDOMWindow>::From<ThirdPartyScriptDetector>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<ThirdPartyScriptDetector>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

ThirdPartyScriptDetector::ThirdPartyScriptDetector(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      precompiled_detection_regex__(
          kThirdPartyTechnologiesSourceLocationRegexString) {}

void ThirdPartyScriptDetector::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

ThirdPartyScriptDetector::Technology ThirdPartyScriptDetector::Detect(
    const WTF::String url) {
  if (!base::FeatureList::IsEnabled(features::kThirdPartyScriptDetection)) {
    return Technology::kNone;
  }

  if (!url) {
    // Early exit if the script is first party.
    return Technology::kNone;
  }

  if (url_to_technology_cache_.Contains(url)) {
    return url_to_technology_cache_.at(url);
  }

  // Create result vectors to get the matches for the capturing groups.
  std::vector<std::string> results(kTechnologyCount);
  std::vector<RE2::Arg> match_results(kTechnologyCount);
  std::vector<RE2::Arg*> match_results_ptr(kTechnologyCount);

  for (size_t i = 0; i < kTechnologyCount; ++i) {
    match_results[i] = &results[i];
    match_results_ptr[i] = &match_results[i];
  }

  Technology technology = Technology::kNone;
  if (RE2::PartialMatchN(url.Utf8(), precompiled_detection_regex__,
                         match_results_ptr.data(), kTechnologyCount)) {
    for (int i = 0; i < kTechnologyCount; ++i) {
      if (results[i] != "") {
        // results[i] stores capturing subgroup match result. If not empty
        // string, it means the subgroup has been matched, and the technology
        // relates to that capturing group should be returned.
        technology = static_cast<Technology>(1 << i);
        break;
      }
    }
  }

  url_to_technology_cache_.Set(url, technology);
  return technology;
}

}  // namespace blink
