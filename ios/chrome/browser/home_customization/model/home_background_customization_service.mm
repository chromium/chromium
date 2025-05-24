// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import "components/sync/protocol/theme_specifics.pb.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "url/gurl.h"

HomeBackgroundCustomizationService::HomeBackgroundCustomizationService() {}

HomeBackgroundCustomizationService::~HomeBackgroundCustomizationService() {}

void HomeBackgroundCustomizationService::Shutdown() {}

const sync_pb::ThemeSpecifics::NtpCustomBackground&
HomeBackgroundCustomizationService::GetCurrentBackground() {
  return current_background_;
}

void HomeBackgroundCustomizationService::SetCurrentBackground(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& attribution_action_url,
    const std::string& collection_id) {
  sync_pb::ThemeSpecifics::NtpCustomBackground new_background;
  new_background.set_url(background_url.spec());
  // TODO(crbug.com/411453550): Add thumbnail_url to NtpCustomBackground field.
  new_background.set_attribution_line_1(attribution_line_1);
  new_background.set_attribution_line_2(attribution_line_2);
  new_background.set_attribution_action_url(attribution_action_url.spec());
  new_background.set_collection_id(collection_id);
  current_background_ = new_background;

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::AddObserver(
    HomeBackgroundCustomizationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void HomeBackgroundCustomizationService::RemoveObserver(
    HomeBackgroundCustomizationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HomeBackgroundCustomizationService::NotifyObserversOfBackgroundChange() {
  for (HomeBackgroundCustomizationServiceObserver& observer : observers_) {
    observer.OnBackgroundChanged();
  }
}
