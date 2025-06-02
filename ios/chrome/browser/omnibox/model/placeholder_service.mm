// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/placeholder_service.h"

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

PlaceholderService::PlaceholderService(FaviconLoader* favicon_loader,
                                       TemplateURLService* template_url_service)
    : favicon_loader_(favicon_loader),
      template_url_service_(template_url_service) {
  template_url_service->AddObserver(this);
}

PlaceholderService::~PlaceholderService() {
  for (auto& observer : model_observers_) {
    observer.OnPlaceholderServiceShuttingDown();
  }

  template_url_service_->RemoveObserver(this);
}

UIImage* PlaceholderService::GetCurrentDefaultSearchEngineFavicon() {
  return nil;
}

NSString* PlaceholderService::GetCurrentPlaceholderText() {
  CHECK(template_url_service_);

  std::u16string provider_name = u"";
  if (const TemplateURL* search_provider =
          template_url_service_->GetDefaultSearchProvider()) {
    provider_name = search_provider->short_name();
  }

  return l10n_util::GetNSStringF(IDS_OMNIBOX_EMPTY_HINT_WITH_DSE_NAME,
                                 provider_name);
}

NSString* PlaceholderService::GetCurrentSearchOnlyPlaceholderText() {
  CHECK(template_url_service_);

  std::u16string provider_name = u"";
  if (const TemplateURL* search_provider =
          template_url_service_->GetDefaultSearchProvider()) {
    provider_name = search_provider->short_name();
  }

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate)) {
    return l10n_util::GetNSStringF(IDS_IOS_OMNIBOX_PLACEHOLDER_SEARCH_ONLY,
                                   provider_name);
  } else {
    return l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  }
}

base::WeakPtr<PlaceholderService> PlaceholderService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlaceholderService::AddObserver(PlaceholderServiceObserver* observer) {
  model_observers_.AddObserver(observer);
}

void PlaceholderService::RemoveObserver(PlaceholderServiceObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

#pragma mark - TemplateURLServiceObserver

void PlaceholderService::OnTemplateURLServiceChanged() {
  for (auto& observer : model_observers_) {
    observer.OnPlaceholderTextChanged();
    observer.OnPlaceholderImageChanged();
  }
}
