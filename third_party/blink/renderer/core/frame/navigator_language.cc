// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_language.h"

#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Vector<String> ParseAndSanitize(const String& accept_languages) {
  Vector<String> languages;
  accept_languages.Split(',', languages);

  // Sanitizing tokens. We could do that more extensively but we should assume
  // that the accept languages are already sane and support BCP47. It is
  // likely a waste of time to make sure the tokens matches that spec here.
  for (size_t i = 0; i < languages.size(); ++i) {
    String& token = languages[i];
    token = token.StripWhiteSpace();
    if (token.length() >= 3 && token[2] == '_')
      token.replace(2, 1, "-");
  }

  if (languages.IsEmpty())
    languages.push_back(DefaultLanguage());

  return languages;
}

NavigatorLanguage::NavigatorLanguage(ExecutionContext* context)
    : context_(context) {}

AtomicString NavigatorLanguage::language() {
  if (RuntimeEnabledFeatures::NavigatorLanguageInInsecureContextEnabled() ||
      context_->IsSecureContext()) {
    return AtomicString(languages().front());
  }
  return AtomicString();
}

const Vector<String>& NavigatorLanguage::languages() {
  if (RuntimeEnabledFeatures::NavigatorLanguageInInsecureContextEnabled() ||
      context_->IsSecureContext()) {
    EnsureUpdatedLanguage();
    return languages_;
  }
  DEFINE_STATIC_LOCAL(const Vector<String>, empty_vector, {});
  return empty_vector;
}

AtomicString NavigatorLanguage::SerializeLanguagesForClientHintHeader() {
  EnsureUpdatedLanguage();

  StringBuilder builder;
  for (size_t i = 0; i < languages_.size(); i++) {
    if (i)
      builder.Append(", ");
    builder.Append('"');
    builder.Append(languages_[i]);
    builder.Append('"');
  }
  return builder.ToAtomicString();
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
    probe::ApplyAcceptLanguageOverride(context_, &accept_languages_override);

    if (!accept_languages_override.IsNull()) {
      languages_ = ParseAndSanitize(accept_languages_override);
    } else {
      languages_ = ParseAndSanitize(GetAcceptLanguages());
    }

    languages_dirty_ = false;
  }
}

void NavigatorLanguage::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
}

}  // namespace blink
