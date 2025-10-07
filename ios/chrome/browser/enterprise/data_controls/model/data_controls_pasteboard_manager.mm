// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/not_fatal_until.h"
#import "ios/chrome/browser/enterprise/data_controls/model/pasteboard_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace data_controls {

// static
DataControlsPasteboardManager* DataControlsPasteboardManager::GetInstance() {
  static base::NoDestructor<DataControlsPasteboardManager> instance;
  return instance.get();
}

DataControlsPasteboardManager::DataControlsPasteboardManager() {
  Initialize();
}

DataControlsPasteboardManager::~DataControlsPasteboardManager() = default;

void DataControlsPasteboardManager::Initialize() {
  pasteboard_state_ = {};
  stage_ = Stage::kUnknownSource;
  pasteboard_observer_ = [[PasteboardObserver alloc]
      initWithCallback:base::BindRepeating(
                           &DataControlsPasteboardManager::OnPasteboardChanged,
                           base::Unretained(this))];
}

void DataControlsPasteboardManager::SetNextPasteboardItemsSource(
    GURL source_url,
    ProfileIOS* source_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(source_profile);

  stage_ = Stage::kPendingSource;

  pasteboard_state_.source_url = std::move(source_url);
  pasteboard_state_.source_profile_name =
      source_profile->GetOriginalProfile()->GetProfileName();
  pasteboard_state_.source_profile_incognito = source_profile->IsOffTheRecord();
}

PasteboardSource
DataControlsPasteboardManager::GetCurrentPasteboardItemsSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stage_ != Stage::kKnownSource) {
    return PasteboardSource();
  }

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  ProfileIOS* profile = profile_manager->GetProfileWithName(
      pasteboard_state_.source_profile_name);

  if (profile && pasteboard_state_.source_profile_incognito) {
    profile = profile->GetOffTheRecordProfile();
  }
  return PasteboardSource{pasteboard_state_.source_url, profile};
}

void DataControlsPasteboardManager::OnPasteboardChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (stage_) {
    case Stage::kUnknownSource:
      break;
    case Stage::kPendingSource:
      stage_ = Stage::kKnownSource;
      break;
    case Stage::kKnownSource:
      pasteboard_state_ = {};
      stage_ = Stage::kUnknownSource;
  }
}

void DataControlsPasteboardManager::ResetForTesting() {
  Initialize();
}

}  // namespace data_controls
