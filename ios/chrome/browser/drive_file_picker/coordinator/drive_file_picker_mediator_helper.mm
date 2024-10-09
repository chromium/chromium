// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_helper.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Number of items to fetch for the first page.
constexpr NSInteger kFirstPageSize = 20;
// Number of items to fetch for the next pages.
constexpr NSInteger kNextPageSize = 50;
// The size of the drive file picker item icon.
constexpr CGFloat kDriveFilePickerItemIconSize = 18;

// extra_term parameter for the Starred view.
NSString* kStarredExtraTerm = @"starred=true";
// extra_term parameter for the Recent view.
NSString* kRecentExtraTerm = @"mimeType!='application/vnd.google-apps.folder'";
// order_by parameter for the Recent view.
NSString* kRecentOrderBy = @"recency desc";
// extra_term parameter for the Shared with me view.
NSString* kSharedWithMeExtraTerm = @"sharedWithMe=true";
// order_by parameter for the Shared with me view.
NSString* kSharedWithMeOrderBy = @"sharedWithMeTime desc";
// The key word to sort items in an ascending order.
NSString* kAscendingQueryOrder = @"asc";
// The key word to sort items in an descending order.
NSString* kDescendingQueryOrder = @"desc";
// The key word to sort items by name.
NSString* kQueryOrderNameType = @"name";
// The key word to sort items by opening time.
NSString* kQueryOrderOpeningType = @"viewedByMeTime";
// The key word to sort items by modification time.
NSString* kQueryOrderModifiedType = @"modifiedTime";
// String representing any audio MIME type.
const char kAnyAudioFileMimeType[] = "audio/*";
// String representing any video MIME type.
const char kAnyVideoFileMimeType[] = "video/*";
// String representing any image MIME type.
const char kAnyImageFileMimeType[] = "image/*";
// extra_term parameter for the "Archives" filter.
NSString* kOnlyShowArchivesExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType='application/zip' or"
     " mimeType='application/x-7z-compressed' or"
     " mimeType='application/x-rar-compressed' or"
     " mimeType='application/vnd.rar' or"
     " mimeType='application/x-tar' or"
     " mimeType='application/x-bzip' or"
     " mimeType='application/x-bzip2' or"
     " mimeType='application/x-freearc' or"
     " mimeType='application/java-archive' or"
     " mimeType='application/gzip')";
// extra_term parameter for the "Audio" filter.
NSString* kOnlyShowAudioExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'audio/')";
// extra_term parameter for the "Video" filter.
NSString* kOnlyShowVideosExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'video/')";
// extra_term parameter for the "Photos & Images" filter.
NSString* kOnlyShowImagesExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'image/')";
// extra_term parameter for the "PDFs" filter.
NSString* kOnlyShowPDFsExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType='application/pdf')";
// Prefix of MIME types associated with Google apps.
NSString* kGoogleAppsMIMETypePrefix = @"application/vnd.google-apps.";
// Prefix of links to icons in the Drive third-party icon repository.
NSString* kDriveIconRepositoryPrefix =
    @"https://drive-thirdparty.googleusercontent.com/";
// Prefix of links to shared drive background images gallery.
NSString* kSharedDriveBackgroundImageGalleryPrefix =
    @"https://ssl.gstatic.com/team_drive_themes/";

}  // namespace

NSArray<UTType*>* UTTypesAcceptedForEvent(const ChooseFileEvent& event) {
  NSMutableArray<UTType*>* types = [NSMutableArray array];
  // Add accepted file extensions.
  for (const std::string& file_extension : event.accept_file_extensions) {
    std::string_view truncated_file_extension{std::next(file_extension.begin()),
                                              file_extension.end()};
    UTType* file_extension_type =
        [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(
                                              truncated_file_extension)];
    [types addObject:file_extension_type];
  }
  // Add accepted MIME types.
  for (const std::string& mime_type : event.accept_mime_types) {
    UTType* mime_type_type = nil;
    // Handle "audio/*", "video/*" and "image/*" separately since they are not
    // recognized by UTType.
    if (mime_type == kAnyAudioFileMimeType) {
      mime_type_type = UTTypeAudio;
    } else if (mime_type == kAnyVideoFileMimeType) {
      mime_type_type = UTTypeVideo;
    } else if (mime_type == kAnyImageFileMimeType) {
      mime_type_type = UTTypeImage;
    } else {
      mime_type_type =
          [UTType typeWithMIMEType:base::SysUTF8ToNSString(mime_type)];
    }
    // Test `mime_type_type` before adding since `typeWithMIMEType` can return
    // nil.
    if (mime_type_type) {
      [types addObject:mime_type_type];
    }
  }
  return types;
}

void ApplySortToDriveListQuery(DriveItemsSortingType sorting_criteria,
                               DriveItemsSortingOrder sorting_direction,
                               bool folders_first,
                               DriveListQuery& query) {
  NSString* sorting_criteria_str;
  switch (sorting_criteria) {
    case DriveItemsSortingType::kName:
      sorting_criteria_str = kQueryOrderNameType;
      break;
    case DriveItemsSortingType::kOpeningTime:
      sorting_criteria_str = kQueryOrderOpeningType;
      break;
    case DriveItemsSortingType::kModificationTime:
      sorting_criteria_str = kQueryOrderModifiedType;
      break;
  }
  NSString* sorting_direction_str;
  switch (sorting_direction) {
    case DriveItemsSortingOrder::kAscending:
      sorting_direction_str = kAscendingQueryOrder;
      break;
    case DriveItemsSortingOrder::kDescending:
      sorting_direction_str = kDescendingQueryOrder;
      break;
  }
  if (folders_first) {
    query.order_by =
        [NSString stringWithFormat:@"folder,%@ %@", sorting_criteria_str,
                                   sorting_direction_str];
  } else {
    query.order_by = [NSString
        stringWithFormat:@"%@ %@", sorting_criteria_str, sorting_direction_str];
  }
}

void ApplyFilterToDriveListQuery(DriveFilePickerFilter filter,
                                 DriveListQuery& query) {
  NSString* filter_extra_term = nil;
  switch (filter) {
    case DriveFilePickerFilter::kOnlyShowArchives:
      filter_extra_term = kOnlyShowArchivesExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowAudio:
      filter_extra_term = kOnlyShowAudioExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowVideos:
      filter_extra_term = kOnlyShowVideosExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowImages:
      filter_extra_term = kOnlyShowImagesExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowPDFs:
      filter_extra_term = kOnlyShowPDFsExtraTerm;
      break;
    case DriveFilePickerFilter::kShowAllFiles:
      filter_extra_term = nil;
      break;
  }
  if (!filter_extra_term) {
    return;
  }
  if (query.extra_term) {
    query.extra_term = [NSString
        stringWithFormat:@"(%@) and (%@)", query.extra_term, filter_extra_term];
  } else {
    query.extra_term = filter_extra_term;
  }
}

DriveListQuery CreateDriveListQuery(
    DriveFilePickerCollectionType collection_type,
    NSString* folder_identifier,
    DriveFilePickerFilter filter,
    DriveItemsSortingType sorting_criteria,
    DriveItemsSortingOrder sorting_direction,
    BOOL should_show_search_items,
    NSString* search_text,
    NSString* page_token) {
  DriveListQuery query;

  // Set page size and page token.
  query.page_size = page_token ? kNextPageSize : kFirstPageSize;
  query.page_token = page_token;

  // Handling search independently of the collection type.
  if (should_show_search_items) {
    if (search_text.length == 0) {
      // Zero-state of search is showing items sorted by recency.
      query.extra_term = kRecentExtraTerm;
      query.order_by = kRecentOrderBy;
    } else {
      query.filename_prefix = search_text;
      ApplySortToDriveListQuery(sorting_criteria, sorting_direction,
                                /* folders_first= */ false, query);
    }
    ApplyFilterToDriveListQuery(filter, query);
    return query;
  }

  switch (collection_type) {
    case DriveFilePickerCollectionType::kRoot:
      // The root collection cannot be obtained using a query.
      NOTREACHED_NORETURN();
    case DriveFilePickerCollectionType::kSharedDrives:
      // For "Shared Drives", there are no parameters to set.
      break;
    case DriveFilePickerCollectionType::kFolder:
      query.folder_identifier = folder_identifier;
      ApplySortToDriveListQuery(sorting_criteria, sorting_direction,
                                /* folders_first= */ true, query);
      ApplyFilterToDriveListQuery(filter, query);
      break;
    case DriveFilePickerCollectionType::kStarred:
      query.extra_term = kStarredExtraTerm;
      ApplySortToDriveListQuery(sorting_criteria, sorting_direction,
                                /* folders_first= */ false, query);
      ApplyFilterToDriveListQuery(filter, query);
      break;
    case DriveFilePickerCollectionType::kRecent:
      query.extra_term = kRecentExtraTerm;
      query.order_by = kRecentOrderBy;
      ApplyFilterToDriveListQuery(filter, query);
      break;
    case DriveFilePickerCollectionType::kSharedWithMe:
      query.extra_term = kSharedWithMeExtraTerm;
      query.order_by = kSharedWithMeOrderBy;
      ApplyFilterToDriveListQuery(filter, query);
      break;
  }

  return query;
}

bool DriveFilePickerItemShouldBeEnabled(const DriveItem& item,
                                        NSArray<UTType*>* accepted_types,
                                        BOOL ignore_accepted_types) {
  // Folders and shared drives can be opened so their contents can be inspected.
  if (item.is_folder || item.is_shared_drive) {
    return true;
  }
  // Non-downloadable files cannot be selected.
  if (!item.can_download ||
      [item.mime_type hasPrefix:kGoogleAppsMIMETypePrefix]) {
    return false;
  }
  // If the list of accepted types is empty, or the user opted to ignore it,
  // then any downloadable file can be selected.
  if (ignore_accepted_types || accepted_types.count == 0) {
    return true;
  }
  // If there is a non-empty list of accepted types, then any downloadable file
  // conforming to one of these types can be selected.
  UTType* item_type = [UTType typeWithMIMEType:item.mime_type];
  for (UTType* accepted_type in accepted_types) {
    if ([item_type conformsToType:accepted_type]) {
      return true;
    }
  }
  return false;
}

NSString* DriveFilePickerItemSubtitleModified(const DriveItem& item) {
  if (!item.modified_time) {
    return nil;
  }
  NSString* modified_time_str =
      [NSDateFormatter localizedStringFromDate:item.modified_time
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];
  return l10n_util::GetNSStringF(IDS_IOS_DRIVE_FILE_PICKER_SUBTITLE_MODIFIED,
                                 base::SysNSStringToUTF16(modified_time_str));
}

NSString* DriveFilePickerItemSubtitleOpened(const DriveItem& item) {
  if (!item.viewed_by_me_time) {
    return nil;
  }
  NSString* viewed_by_me_time_str =
      [NSDateFormatter localizedStringFromDate:item.viewed_by_me_time
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];
  return l10n_util::GetNSStringF(
      IDS_IOS_DRIVE_FILE_PICKER_SUBTITLE_OPENED,
      base::SysNSStringToUTF16(viewed_by_me_time_str));
}

NSString* DriveFilePickerItemSubtitleShareWithMe(const DriveItem& item) {
  if (!item.shared_with_me_time) {
    return nil;
  }
  NSString* shared_with_me_time_str =
      [NSDateFormatter localizedStringFromDate:item.shared_with_me_time
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];
  return l10n_util::GetNSStringF(
      IDS_IOS_DRIVE_FILE_PICKER_SUBTITLE_SHARED_WITH_ME,
      base::SysNSStringToUTF16(shared_with_me_time_str));
}

NSString* DriveFilePickerItemSubtitleRecent(const DriveItem& item) {
  if (!item.viewed_by_me_time || !item.modified_time) {
    return nil;
  }
  return [item.viewed_by_me_time compare:item.modified_time]
             ? DriveFilePickerItemSubtitleOpened(item)
             : DriveFilePickerItemSubtitleModified(item);
}

NSString* DriveFilePickerItemSubtitle(
    const DriveItem& item,
    DriveFilePickerCollectionType collection_type,
    DriveItemsSortingType sorting_criteria,
    BOOL should_show_search_items,
    NSString* search_text) {
  // Handling search separately.
  if (should_show_search_items) {
    if (search_text.length == 0) {
      // Zero-state search items are sorted by recency.
      return DriveFilePickerItemSubtitleRecent(item);
    }
    switch (sorting_criteria) {
      case DriveItemsSortingType::kName:
      case DriveItemsSortingType::kModificationTime:
        // If the sorting criteria is by name or modification time, then the
        // subtitle presented for each item is the last modification time.
        return DriveFilePickerItemSubtitleModified(item);
      case DriveItemsSortingType::kOpeningTime:
        return DriveFilePickerItemSubtitleOpened(item);
    }
  }

  // Handling non-search items.
  switch (collection_type) {
    case DriveFilePickerCollectionType::kRoot:
      NOTREACHED_NORETURN();
    case DriveFilePickerCollectionType::kSharedDrives:
      // Shared drives do not have subtitles.
      return nil;
    case DriveFilePickerCollectionType::kRecent:
      return DriveFilePickerItemSubtitleRecent(item);
    case DriveFilePickerCollectionType::kSharedWithMe:
      return DriveFilePickerItemSubtitleShareWithMe(item);
    case DriveFilePickerCollectionType::kFolder:
    case DriveFilePickerCollectionType::kStarred:
      switch (sorting_criteria) {
        case DriveItemsSortingType::kName:
        case DriveItemsSortingType::kModificationTime:
          // If the sorting criteria is by name or modification time, then the
          // subtitle presented for each item is the last modification time.
          return DriveFilePickerItemSubtitleModified(item);
        case DriveItemsSortingType::kOpeningTime:
          return DriveFilePickerItemSubtitleOpened(item);
      }
  }
}

DriveFilePickerItem* DriveItemToDriveFilePickerItem(
    const DriveItem& item,
    DriveFilePickerCollectionType collection_type,
    DriveItemsSortingType sorting_criteria,
    BOOL should_show_search_items,
    NSString* search_text,
    UIImage* fetched_icon,
    NSString* fetched_icon_link) {
  DriveItemType type;
  if (item.is_folder) {
    type = DriveItemType::kFolder;
  } else if (item.is_shared_drive) {
    type = DriveItemType::kSharedDrive;
  } else {
    type = DriveItemType::kFile;
  }
  UIImage* icon =
      fetched_icon ? fetched_icon : GetPlaceholderIconForDriveItem(item);
  DriveFilePickerItem* drive_file_picker_item = [[DriveFilePickerItem alloc]
      initWithIdentifier:item.identifier
                   title:item.name
                subtitle:DriveFilePickerItemSubtitle(
                             item, collection_type, sorting_criteria,
                             should_show_search_items, search_text)
                    icon:icon
                    type:type];
  drive_file_picker_item.shouldFetchIcon =
      (fetched_icon == nil && fetched_icon_link != nil);
  return drive_file_picker_item;
}

std::optional<DriveItem> FindDriveItemFromIdentifier(
    const std::vector<DriveItem>& driveItems,
    NSString* identifier) {
  auto it =
      std::find_if(driveItems.begin(), driveItems.end(),
                   [identifier](const DriveItem& driveItem) {
                     return [driveItem.identifier isEqualToString:identifier];
                   });
  if (it != driveItems.end()) {
    return *it;
  }
  return std::nullopt;
}

NSURL* DriveFilePickerGenerateDownloadFileURL(NSString* download_file_name) {
  base::FilePath download_dir;
  if (!GetTempDir(&download_dir)) {
    return nil;
  }
  download_dir =
      download_dir.Append(base::SysNSStringToUTF8([[NSUUID UUID] UUIDString]));
  base::FilePath download_file_path =
      download_dir.Append(base::SysNSStringToUTF8(download_file_name));
  return base::apple::FilePathToNSURL(download_file_path);
}

UIImage* GetPlaceholderIconForDriveItem(const DriveItem& item) {
  if (item.is_shared_drive) {
    return [DriveFilePickerItem sharedDrivesItem].icon;
  } else if (item.is_folder) {
    return DefaultSymbolWithPointSize(kFolderSymbol,
                                      kDriveFilePickerItemIconSize);
  } else {
    return DefaultSymbolWithPointSize(kDocSymbol, kDriveFilePickerItemIconSize);
  }
}

NSString* GetImageLinkForDriveItem(const DriveItem& item) {
  NSString* imageLink = nil;
  if (item.is_shared_drive) {
    // If this is a shared drive, the background image link should be fetched.
    imageLink = item.background_image_link;
  } else {
    // Otherwise the icon link should be fetched.
    // By default drive api provides a 16 resolution icons, replacing 16 by 64
    // in the icon URLs provide better sized icons e.g. the URL
    // https://drive-thirdparty.googleusercontent.com/16/type/video/mp4 becomes
    // https://drive-thirdparty.googleusercontent.com/64/type/video/mp4
    imageLink = item.icon_link;
    NSString* target =
        [kDriveIconRepositoryPrefix stringByAppendingString:@"16"];
    NSString* replacement =
        [kDriveIconRepositoryPrefix stringByAppendingString:@"64"];
    imageLink = [imageLink stringByReplacingOccurrencesOfString:target
                                                     withString:replacement];
  }
  if (![imageLink hasPrefix:kDriveIconRepositoryPrefix] &&
      ![imageLink hasPrefix:kSharedDriveBackgroundImageGalleryPrefix]) {
    // If the image link is not a known source, return nil.
    return nil;
  }
  return imageLink;
}
