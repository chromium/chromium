// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/open_from_clipboard/clipboard_async_wrapper_ios.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager_observer.h"
#import "ios/chrome/browser/enterprise/data_controls/model/pasteboard_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/web/public/thread/web_thread.h"
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

// Converts an `image_item` into a base64 encoded string and returns it if
// possible, otherwise returns an empty string. Returns std::nullopt if the size
// exceeds `kMaxPasteboardContentSizeToProcess`.
std::optional<std::string> HandleImageItem(id image_item) {
  DCHECK(!web::WebThread::CurrentlyOn(web::WebThread::UI));
  NSData* image_data = nil;
  // The item might be UIImage or NSData, depending on the source of the copy.
  if (UIImage* image = base::apple::ObjCCast<UIImage>(image_item)) {
    image_data = UIImagePNGRepresentation(image);
  } else if (NSData* data = base::apple::ObjCCast<NSData>(image_item)) {
    image_data = data;
  }

  if (!image_data) {
    return std::string();
  }

  if (image_data.length > data_controls::kMaxPasteboardContentSizeToProcess) {
    return std::nullopt;
  }

  NSString* image_string =
      [image_data base64EncodedStringWithOptions:/*No Formatting*/ 0];
  return base::SysNSStringToUTF8(image_string);
}

// Process all the pasteboard items in a for loop and return with a
// `PasteboardContentDLP` containing a string of concatenated text and a base64
// encoded image string. Returns std::nullopt if the size of the text or image
// exceeds `kMaxPasteboardContentSizeToProcess`.
std::optional<data_controls::PasteboardContentDLP> ProcessPasteboardItems(
    NSArray<NSDictionary<NSString*, id>*>* items) {
  DCHECK(!web::WebThread::CurrentlyOn(web::WebThread::UI));
  data_controls::PasteboardContentDLP content = {};
  NSUInteger text_size = 0;

  for (NSDictionary<NSString*, id>* item in items) {
    for (NSString* key in item) {
      if ([key isEqualToString:UTTypePNG.identifier] ||
          [key isEqualToString:UTTypeJPEG.identifier]) {
        // If the image is already read, this is not the first image in the
        // pasteboard and we don't return it.
        if (content.image.empty()) {
          std::optional<std::string> image = HandleImageItem(item[key]);
          if (!image.has_value()) {
            return std::nullopt;
          }
          content.image = *std::move(image);
        }
        // Skip to the next representation key to avoid falling to the check for
        // NSData class below and add the image representation to the text
        // string.
        continue;
      } else if ([key isEqualToString:UTTypeUTF8PlainText.identifier] ||
                 [key isEqualToString:UTTypeHTML.identifier] ||
                 [key isEqualToString:UTTypeSVG.identifier] ||
                 [key isEqualToString:UTTypeRTF.identifier]) {
        if (NSString* str = base::apple::ObjCCast<NSString>(item[key])) {
          // Early return to avoid doing costly string conversion.
          if (text_size + str.length >
              data_controls::kMaxPasteboardContentSizeToProcess) {
            return std::nullopt;
          }

          std::string str_utf8 = base::SysNSStringToUTF8(str);
          if (text_size + str_utf8.size() >
              data_controls::kMaxPasteboardContentSizeToProcess) {
            return std::nullopt;
          }
          text_size += str_utf8.size();
          content.text.append(str_utf8);
        }
      }
    }
  }
  return content;
}

// Get the pasteboard text items and image item as strings for Pasted Content
// DLP scanning.
void GetPasteboardItems(
    base::OnceCallback<void(std::optional<data_controls::PasteboardContentDLP>)>
        content_callback,
    NSArray<NSDictionary<NSString*, id>*>* items,
    UIPasteboard* pasteboard) {
  // Pasteboard is empty.
  if (pasteboard.numberOfItems == 0 && items.count == 0) {
    std::move(content_callback).Run(data_controls::PasteboardContentDLP{});
    return;
  }

  // UIPasteboard only has placeholder if `items.count != 0`, the actual items
  // are stored within the passed in parameter `items`.
  NSArray<NSDictionary<NSString*, id>*>* items_to_process =
      (items.count != 0) ? items : pasteboard.items;

  // Posting the processing of the pasteboard items as a background task because
  // converting `UIImage` to `std::string` is CPU intensive.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ProcessPasteboardItems, items_to_process),
      std::move(content_callback));
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
    GetGeneralPasteboard(base::BindOnce(
                             [](DataControlsPasteboardManager* manager,
                                NSArray<NSDictionary<NSString*, id>*>* items,
                                UIPasteboard* pasteboard) {
                               if (manager->stage_ == Stage::kKnownSource) {
                                 manager->stage_ = Stage::kReplacingItems;
                                 WriteItemsToPasteboard(items, pasteboard);
                               }
                             },
                             base::Unretained(this), pasteboard_state_.items)
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

  for (auto& observer : observers_) {
    observer.OnPasteboardContentChanged();
  }
}

void DataControlsPasteboardManager::
    RestorePlaceholderToGeneralPasteboardIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stage_ != Stage::kKnownSource) {
    return;
  }

  if (!pasteboard_state_.os_clipboard_allowed) {
    GetGeneralPasteboard(base::BindOnce(
        [](DataControlsPasteboardManager* manager, UIPasteboard* pasteboard) {
          if (manager->stage_ == Stage::kKnownSource) {
            manager->stage_ = Stage::kReplacingItems;
            ReplacePasteboardItemsWithPlaceholder(pasteboard);
          }
        },
        base::Unretained(this)));
  }
}

void DataControlsPasteboardManager::GetPasteboardTextAndImage(
    base::OnceCallback<void(std::optional<PasteboardContentDLP>)>
        content_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetGeneralPasteboard(base::BindOnce(&GetPasteboardItems,
                                      std::move(content_callback),
                                      pasteboard_state_.items));
}

void DataControlsPasteboardManager::AddObserver(
    DataControlsPasteboardManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void DataControlsPasteboardManager::RemoveObserver(
    DataControlsPasteboardManagerObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void DataControlsPasteboardManager::ResetForTesting() {
  Initialize();
}

}  // namespace data_controls
