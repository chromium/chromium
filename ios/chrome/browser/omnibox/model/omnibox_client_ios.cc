// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/omnibox/model/omnibox_client_ios.h"

#include <memory>

#include "base/strings/string_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/image/image.h"

bool OmniboxClientIOS::CurrentPageExists() const {
  return true;
}

const GURL& OmniboxClientIOS::GetURL() const {
  return GURL::EmptyGURL();
}

const std::u16string& OmniboxClientIOS::GetTitle() const {
  return base::EmptyString16();
}

gfx::Image OmniboxClientIOS::GetFavicon() const {
  return gfx::Image();
}

ukm::SourceId OmniboxClientIOS::GetUKMSourceId() const {
  return ukm::kInvalidSourceId;
}

bool OmniboxClientIOS::IsLoading() const {
  return false;
}

bool OmniboxClientIOS::IsPasteAndGoEnabled() const {
  return false;
}

bool OmniboxClientIOS::IsDefaultSearchProviderEnabled() const {
  return true;
}

bookmarks::BookmarkModel* OmniboxClientIOS::GetBookmarkModel() {
  return nullptr;
}

TemplateURLService* OmniboxClientIOS::GetTemplateURLService() {
  return nullptr;
}

AutocompleteClassifier* OmniboxClientIOS::GetAutocompleteClassifier() {
  return nullptr;
}

bool OmniboxClientIOS::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int OmniboxClientIOS::GetHttpsPortForTesting() const {
  return 0;
}

metrics::OmniboxEventProto::PageClassification
OmniboxClientIOS::GetOmniboxComposeboxPageClassification() const {
  return metrics::OmniboxEventProto::INVALID_SPEC;
}

bool OmniboxClientIOS::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image OmniboxClientIOS::GetExtensionIcon(
    const TemplateURL* template_url) const {
  return gfx::Image();
}

gfx::Image OmniboxClientIOS::GetSizedIcon(const SkBitmap* bitmap) const {
  return gfx::Image();
}

gfx::Image OmniboxClientIOS::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  return gfx::Image();
}

gfx::Image OmniboxClientIOS::GetSizedIcon(const gfx::Image& icon) const {
  return gfx::Image();
}

std::optional<lens::proto::LensOverlaySuggestInputs>
OmniboxClientIOS::GetLensOverlaySuggestInputs() const {
  return std::nullopt;
}

std::optional<lens::ContextualInputData>
OmniboxClientIOS::GetContextualInputData() const {
  return std::nullopt;
}

void OmniboxClientIOS::ProcessExtensionMatch(
    const std::u16string& text,
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition) {}

void OmniboxClientIOS::OnUserPastedInOmniboxResultingInValidURL() {}

gfx::Image OmniboxClientIOS::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image OmniboxClientIOS::GetFaviconForDefaultSearchProvider(
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

gfx::Image OmniboxClientIOS::GetFaviconForKeywordSearchProvider(
    const TemplateURL* template_url,
    FaviconFetchedCallback on_favicon_fetched) {
  return gfx::Image();
}

bool OmniboxClientIOS::IsHistoryEmbeddingsEnabled() const {
  return false;
}

bool OmniboxClientIOS::IsAimPopupEnabled() const {
  return false;
}

omnibox::InputState OmniboxClientIOS::GetInputState() const {
  return omnibox::InputState();
}

bool OmniboxClientIOS::ShouldSkipZeroSuggestRequest() const {
  return false;
}

GURL OmniboxClientIOS::GetContextualTasksInnerFrameURL() const {
  return GURL();
}
