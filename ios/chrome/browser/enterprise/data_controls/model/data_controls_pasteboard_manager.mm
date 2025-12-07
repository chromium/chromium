// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/not_fatal_until.h"
#import "components/open_from_clipboard/clipboard_async_wrapper_ios.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/data_controls/model/pasteboard_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

void WriteItemsToPasteboard(NSArray<NSDictionary<NSString*, id>*>* items,
                            UIPasteboard* pasteboard) {
  CHECK(items);
  CHECK(pasteboard);

  // Make the items expire after some time. Fallback for the case where the web
  // page tries to paste the items but never do it.
  NSDate* now = [NSDate date];
  NSTimeInterval expiration_in_seconds = 10.0;
  NSDate* expiration_date =
      [now dateByAddingTimeInterval:expiration_in_seconds];
  [pasteboard setItems:items
               options:@{
                 UIPasteboardOptionLocalOnly : @YES,
                 UIPasteboardOptionExpirationDate : expiration_date
               }];
}

void ReplacePasteboardItemsWithPlaceholder(UIPasteboard* pasteboard) {
  pasteboard.string = l10n_util::GetNSString(
      IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE);
}

}  // namespace

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
    ProfileIOS* source_profile,
    bool os_clipboard_allowed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(source_profile);

  stage_ = Stage::kPendingSource;

  pasteboard_state_ = {std::move(source_url),
                       source_profile->GetOriginalProfile()->GetProfileName(),
                       source_profile->IsOffTheRecord(), os_clipboard_allowed};
}

PasteboardSource
DataControlsPasteboardManager::GetCurrentPasteboardItemsSource() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (stage_ != Stage::kKnownSource) {
    return PasteboardSource();
  }

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  if (ProfileIOS* profile = profile_manager->GetProfileWithName(
          pasteboard_state_.source_profile_name)) {
    return PasteboardSource{pasteboard_state_.source_url,
                            pasteboard_state_.source_profile_incognito
                                ? profile->GetOffTheRecordProfile()
                                : profile};
  }

  // Invalidate source if the profile is gone.
  pasteboard_state_ = {};
  stage_ = Stage::kUnknownSource;

  return PasteboardSource();
}

void DataControlsPasteboardManager::RestoreItemsToGeneralPasteboardIfNeeded(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stage_ != Stage::kKnownSource) {
    std::move(callback).Run();
    return;
  }

  // Restore protected items to the pasteboard so they can be copied to
  // destinations allowed by Data Control rules.
  if (!pasteboard_state_.os_clipboard_allowed && pasteboard_state_.items) {
    stage_ = Stage::kReplacingItems;
    GetGeneralPasteboard(
        /* asynchronous= */ true,
        base::BindOnce(&WriteItemsToPasteboard, pasteboard_state_.items)
            .Then(std::move(callback)));
  } else {
    std::move(callback).Run();
  }
}

void DataControlsPasteboardManager::OnPasteboardChanged(
    UIPasteboard* pasteboard) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (stage_) {
    case Stage::kUnknownSource:
      break;
    case Stage::kPendingSource: {
      if (pasteboard_state_.os_clipboard_allowed) {
        stage_ = Stage::kKnownSource;
      } else {
        // The copied items are not allowed to stay in the os clipboard because
        // they can be freely copied. Replace them with a placeholder text.
        // We'll put the original items back in the clipboard only for paste
        // operations approved by data control rules.
        stage_ = Stage::kReplacingItems;
        pasteboard_state_.items = pasteboard.items;
        ReplacePasteboardItemsWithPlaceholder(pasteboard);
      }
      break;
    }
    case Stage::kReplacingItems:
      stage_ = Stage::kKnownSource;
      break;
    case Stage::kKnownSource:
      pasteboard_state_ = {};
      stage_ = Stage::kUnknownSource;
      break;
  }
}

void DataControlsPasteboardManager::
    RestorePlaceholderToGeneralPasteboardIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stage_ != Stage::kKnownSource) {
    return;
  }

  if (!pasteboard_state_.os_clipboard_allowed) {
    stage_ = Stage::kReplacingItems;
    GetGeneralPasteboard(
        /* asynchronous= */ true,
        base::BindOnce(&ReplacePasteboardItemsWithPlaceholder));
  }
}

void DataControlsPasteboardManager::ResetForTesting() {
  Initialize();
}

}  // namespace data_controls
