// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_language.h"

#include "services/network/public/cpp/features.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Vector<String> ParseAndSanitize(const String& accept_languages) {
  Vector<String> languages;
  accept_languages.Split(',', languages);

  // Sanitizing tokens. We could do that more extensively but we should assume
  // that the accept languages are already sane and support BCP47. It is
  // likely a waste of time to make sure the tokens matches that spec here.
  for (wtf_size_t i = 0; i < languages.size(); ++i) {
    String& token = languages[i];
    token = token.StripWhiteSpace();
    if (token.length() >= 3 && token[2] == '_')
      token.replace(2, 1, "-");
  }

  if (languages.empty())
    languages.push_back(DefaultLanguage());

  return languages;
}

NavigatorLanguage::NavigatorLanguage(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

AtomicString NavigatorLanguage::language() {
  return AtomicString(languages().front());
}

const Vector<String>& NavigatorLanguage::languages() {
  EnsureUpdatedLanguage();
  return languages_;
}

bool NavigatorLanguage::IsLanguagesDirty() const {
  return languages_dirty_;
}

void NavigatorLanguage::SetLanguagesDirty() {
  languages_dirty_ = true;
  languages_.clear();
}

void NavigatorLanguage::SetLanguagesForTesting(const String& languages) {
  languages_ = ParseAndSanitize(languages);
}

void NavigatorLanguage::EnsureUpdatedLanguage() {
  if (languages_dirty_) {
    String accept_languages_override;
    probe::ApplyAcceptLanguageOverride(execution_context_,
                                       &accept_languages_override);

    if (!accept_languages_override.IsNull()) {
      languages_ = ParseAndSanitize(accept_languages_override);
    } else {
      languages_ = ParseAndSanitize(GetAcceptLanguages());
      // Reduce the Accept-Language if the ReduceAcceptLanguage deprecation
      // trial is not enabled and feature flag ReduceAcceptLanguage is enabled.
      if (RuntimeEnabledFeatures::DisableReduceAcceptLanguageEnabled(
              execution_context_)) {
        UseCounter::Count(execution_context_,
                          WebFeature::kDisableReduceAcceptLanguage);
      } else if (base::FeatureList::IsEnabled(
                     network::features::kReduceAcceptLanguage)) {
        languages_ = Vector<String>({languages_.front()});
      }
    }

    languages_dirty_ = false;
  }
}

void NavigatorLanguage::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

}  // namespace blink
