// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/third_party_script_detector.h"

#include <cmath>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {
// kThirdPartyTechnologiesSourceLocationRegexString has to strictly follow the
// rules below in order for the regex matching to be working as intended.
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
// 5. For better readability, please put each regex pattern on a new line
// beginning with a "|".
// 6. If adding a new technology which leverages an existing technology (eg.
// Elementor plugins always leverage WordPress), make sure the the smaller set
// goes first (ie. Elementor prior to WordPress) so it won't be masked. Feel
// free to swap their locations if needed and make sure their locations in
// GetTechnologyFromGroupIndex are also swapped.
constexpr char kThirdPartyTechnologiesSourceLocationRegexString[] =
    // Elementor
    "(/wp-content/plugins/elementor)"
    // Google Analytics
    "|(google-analytics\\.com/(?:ga|urchin|analytics)\\.js"
    "|googletagmanager\\.com/gtag/js)"
    // Google Font Api
    "|(googleapis\\.com/.+webfont)"
    // Google Tag Manager
    "|(googletagmanager\\.com/gtm\\.js)"
    // Google Maps
    "|((?:maps\\.google\\.com/"
    "maps\\?file=api(?:&v=(?:[\\d.]+))?|maps\\.google\\.com/maps/api/"
    "staticmap)\\;version:API v1"
    "|//maps\\.google(?:apis)?\\.com/maps/api/js)"
    // Meta Pixel
    "|(connect\\.facebook.\\w+/signals/config/"
    "\\d+\\?v=(?:[\\d\\.]+)\\;version:1"
    "|connect\\.facebook\\.\\w+/.+/fbevents\\.js)"
    // YouTube
    "|(youtube\\.com)"
    // Adobe Analytics
    "|(adoberesources\\.net/alloy/.+/alloy(?:\\.min)?\\.js"
    "|adobedtm\\.com/extensions/.+/AppMeasurement(?:\\.min)?\\.js)"
    // Tiktok Pixel
    "|(analytics\\.tiktok\\.com)"
    // Hotjar
    "|(static\\.hotjar\\.com)"
    // Google AdSense
    "|(googlesyndication\\.com/[^\"]+/"
    "(?:adsbygoogle|show_ads_impl|interstitial_ad_frame))"
    // Google Publisher Tag
    "|(doubleclick\\.net/[^\"]+/pubads_impl(?:_page_level_ads)?.js"
    "|googlesyndication\\.com/tag/js/gpt\\.js)"
    // Google Ads Libraries
    "|(googlesyndication\\.com/[^\"]+/(?:ufs_web_display|reactive_library_fy))"
    // Funding Choices
    "|(fundingchoicesmessages\\.google\\.com)"
    // Slider Revolution
    "|(/wp-content/plugins/revslider)"
    // WordPress
    "|(/wp-(?:content|includes)/"
    "|wp-embed\\.min\\.js)";

constexpr int kTechnologyCount = std::bit_width(
    static_cast<uint64_t>(ThirdPartyScriptDetector::Technology::kLast));

// The order of technologies in the vector should follow their order in the
// regex patterns in kThirdPartyTechnologiesSourceLocationRegexString.
ThirdPartyScriptDetector::Technology GetTechnologyFromGroupIndex(int index) {
  using Technology = ThirdPartyScriptDetector::Technology;
  DEFINE_STATIC_LOCAL(const Vector<Technology>,
                      technologies_in_regex_capturing_group_order, ([] {
                        Vector<Technology> vector{
                            Technology::kElementor,
                            Technology::kGoogleAnalytics,
                            Technology::kGoogleFontApi,
                            Technology::kGoogleTagManager,
                            Technology::kGoogleMaps,
                            Technology::kMetaPixel,
                            Technology::kYouTube,
                            Technology::kAdobeAnalytics,
                            Technology::kTiktokPixel,
                            Technology::kHotjar,
                            Technology::kGoogleAdSense,
                            Technology::kGooglePublisherTag,
                            Technology::kGoogleAdsLibraries,
                            Technology::kFundingChoices,
                            Technology::kSliderRevolution,
                            Technology::kWordPress};
                        return vector;
                      }()));
  return technologies_in_regex_capturing_group_order[index];
}
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
        technology = GetTechnologyFromGroupIndex(i);
        break;
      }
    }
  }

  url_to_technology_cache_.Set(url, technology);
  return technology;
}

}  // namespace blink
