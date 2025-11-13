// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/shared/ui/util/file_size_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {
NSString* const kBulletSeparator = @" · ";
NSString* const kSlashSeparator = @"/";
NSString* const kHostEmptyString = @"";
NSString* const kEstimatedTimeEmptyString = @"";
NSString* const kStatusTextEmptyString = @"";
}  // namespace

@implementation DownloadListItem {
  DownloadRecord _downloadRecord;
  NSString* _downloadID;
  UIImage* _fileTypeIcon;
}

#pragma mark - Initialization

- (instancetype)initWithDownloadRecord:(const DownloadRecord&)downloadRecord {
  self = [super init];
  if (self) {
    _downloadRecord = downloadRecord;
  }
  return self;
}

#pragma mark - Public

- (base::Time)createdTime {
  return _downloadRecord.created_time.is_null() ? base::Time::Now()
                                                : _downloadRecord.created_time;
}

- (NSString*)detailText {
  switch (_downloadRecord.state) {
    case web::DownloadTask::State::kFailed:
      // Failed: show "Status" or "Size • Status".
      return [self formattedStringWithOptionalSizeAndStatus];

    case web::DownloadTask::State::kComplete: {
      // Completed: Show "Size", "Host", or "Size • Host".
      NSMutableArray* components = [NSMutableArray array];
      if (_downloadRecord.total_bytes > 0) {
        [components addObject:[self formattedTotalSize]];
      } else {
        [components addObject:[self statusText]];
      }

      NSString* hostFromURL = [self hostFromURL];
      if (hostFromURL.length > 0) {
        [components addObject:hostFromURL];
      }
      return [components componentsJoinedByString:kBulletSeparator];
    }

    case web::DownloadTask::State::kInProgress: {
      // In Progress: Show "Downloaded / Total • Time" or just "Status".
      if (_downloadRecord.total_bytes <= 0) {
        return [self statusText];
      }

      NSMutableArray* components = [NSMutableArray array];
      [components
          addObject:[NSString stringWithFormat:@"%@%@%@",
                                               [self formattedDownloadedSize],
                                               kSlashSeparator,
                                               [self formattedTotalSize]]];

      NSString* timeRemaining = [self estimatedTimeRemaining];
      if (timeRemaining.length > 0) {
        [components addObject:timeRemaining];
      }
      return [components componentsJoinedByString:kBulletSeparator];
    }

    default:
      // For all other states, just show the localized status text.
      return [self statusText];
  }
}

- (NSString*)downloadID {
  if (!_downloadID) {
    _downloadID = base::SysUTF8ToNSString(_downloadRecord.download_id);
  }
  return _downloadID;
}

- (CGFloat)downloadProgress {
  return _downloadRecord.progress_percent / 100.0f;
}

- (web::DownloadTask::State)downloadState {
  return _downloadRecord.state;
}

- (BOOL)shouldShowProgressView {
  return _downloadRecord.state == web::DownloadTask::State::kInProgress;
}

- (NSString*)fileName {
  if (_downloadRecord.file_name.empty()) {
    return self.defaultFileName;
  }
  NSString* filename = base::SysUTF8ToNSString(_downloadRecord.file_name);
  return (filename.length > 0) ? filename : self.defaultFileName;
}

- (base::FilePath)filePath {
  if (_downloadRecord.file_path.empty()) {
    return base::FilePath();
  }
  return ConvertToAbsoluteDownloadPath(_downloadRecord.file_path);
}

- (NSString*)mimeType {
  return base::SysUTF8ToNSString(_downloadRecord.mime_type);
}

- (UIImage*)fileTypeIcon {
  if (_fileTypeIcon) {
    return _fileTypeIcon;
  }

  NSString* pathString;
  if (!_downloadRecord.file_path.empty()) {
    pathString = base::SysUTF8ToNSString(_downloadRecord.file_path.value());
  } else {
    // Use a temporary path if the actual file path is not available.
    pathString =
        [NSTemporaryDirectory() stringByAppendingPathComponent:self.fileName];
  }
  NSURL* fileURL = [NSURL fileURLWithPath:pathString];

  // Use UIDocumentInteractionController to get the file icon.
  // The file at fileURL does not need to actually exist.
  // The system can return the corresponding result as long as the fileURL has
  // the correct file extension.
  UIDocumentInteractionController* docController =
      [UIDocumentInteractionController interactionControllerWithURL:fileURL];
  _fileTypeIcon = docController.icons.lastObject;

  return _fileTypeIcon;
}

- (DownloadListItemAction)availableActions {
  DownloadListItemAction actions = DownloadListItemActionNone;

  // Downloads in progress have no available actions.
  if (_downloadRecord.state == web::DownloadTask::State::kInProgress) {
    return actions;
  }

  // Completed downloads can be opened in Files app.
  if (_downloadRecord.state == web::DownloadTask::State::kComplete) {
    actions |= DownloadListItemActionOpenInFiles;
  }

  // All non-in-progress downloads can be deleted.
  actions |= DownloadListItemActionDelete;

  return actions;
}

- (BOOL)cancelable {
  return _downloadRecord.state == web::DownloadTask::State::kInProgress;
}

- (BOOL)isEqualToItem:(DownloadListItem*)item {
  if (self == item) {
    return YES;
  }
  const DownloadRecord& other = item->_downloadRecord;
  return _downloadRecord == other;
}

#pragma mark - Private

/// Returns the default file name when a download file's name is not available.
- (NSString*)defaultFileName {
  // TODO(crbug.com/440222083): For all translatable strings, a separate commit
  // will handle them later. This requires contributors with @google.com
  // accounts to upload screenshots to Google Cloud Storage and provide the
  // corresponding .sha1 files. (https://g.co/chrome/translation)
  /*
  <message name="IDS_IOS_DOWNLOAD_DEFAULT_FILE_NAME"
    desc="Default file name when a download file's name is not available. [iOS
  only]"> Untitled
  </message>
  */
  // return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_DEFAULT_FILE_NAME);
  return @"";
}

/// Returns a human-readable formatted string for the total file size.
- (NSString*)formattedTotalSize {
  return GetSizeString(_downloadRecord.total_bytes);
}

/// Returns a human-readable formatted string for the downloaded bytes.
- (NSString*)formattedDownloadedSize {
  return GetSizeString(_downloadRecord.received_bytes);
}

/// Extracts and returns the host name from the download's source URL.
- (NSString*)hostFromURL {
  if (_downloadRecord.original_url.empty()) {
    return kHostEmptyString;
  }

  GURL downloadURL = GURL(_downloadRecord.original_url);
  if (!downloadURL.is_valid()) {
    return kHostEmptyString;
  }

  std::string_view host = downloadURL.host();
  if (host.empty()) {
    return kHostEmptyString;
  }

  NSString* hostString = base::SysUTF8ToNSString(host);
  return hostString ? hostString : kHostEmptyString;
}

/// Calculates and returns formatted time string (e.g., "2 min left") or empty
/// string if not applicable.
- (NSString*)estimatedTimeRemaining {
  // Only calculate for downloads in progress with valid data.
  if (_downloadRecord.state != web::DownloadTask::State::kInProgress ||
      _downloadRecord.received_bytes <= 0 || _downloadRecord.total_bytes <= 0) {
    return kEstimatedTimeEmptyString;
  }

  int64_t remainingBytes =
      _downloadRecord.total_bytes - _downloadRecord.received_bytes;
  if (remainingBytes <= 0) {
    return kEstimatedTimeEmptyString;
  }

  // Calculate elapsed time since download started.
  base::TimeDelta elapsedTime =
      base::Time::Now() - _downloadRecord.created_time;

  if (elapsedTime.is_zero() || elapsedTime.is_negative()) {
    return kEstimatedTimeEmptyString;
  }

  // Calculate download speed and estimate remaining time.
  double downloadSpeed = static_cast<double>(_downloadRecord.received_bytes) /
                         elapsedTime.InSecondsF();
  if (downloadSpeed <= 0) {
    return kEstimatedTimeEmptyString;
  }

  base::TimeDelta remaining = base::Seconds(remainingBytes / downloadSpeed);
  std::u16string time_remaining_text =
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                             ui::TimeFormat::LENGTH_SHORT, remaining);

  return base::SysUTF16ToNSString(time_remaining_text);
}

/// Helper method to handle common logic for displaying size and status.
- (NSString*)formattedStringWithOptionalSizeAndStatus {
  NSMutableArray* components = [NSMutableArray array];
  if (_downloadRecord.total_bytes > 0) {
    [components addObject:[self formattedTotalSize]];
  }
  [components addObject:[self statusText]];
  return [components componentsJoinedByString:kBulletSeparator];
}

/// Returns a localized status text string for the given download state.
- (NSString*)statusText {
  switch (_downloadRecord.state) {
    case web::DownloadTask::State::kInProgress:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_STATE_IN_PROGRESS);
    case web::DownloadTask::State::kComplete:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_STATE_COMPLETED);
    case web::DownloadTask::State::kFailed:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_STATE_FAILED);
    case web::DownloadTask::State::kCancelled:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_STATE_CANCELLED);
    case web::DownloadTask::State::kNotStarted:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_STATE_PAUSED);
    default:
      return kStatusTextEmptyString;
  }
}

#pragma mark - NSObject

/// Required for UITableViewDiffableDataSource to properly track items.
/// Compares all DownloadRecord properties that affect the UI display.
- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[DownloadListItem class]]) {
    return NO;
  }
  DownloadListItem* otherItem =
      base::apple::ObjCCastStrict<DownloadListItem>(object);
  const DownloadRecord& other = otherItem->_downloadRecord;
  return _downloadRecord.download_id == other.download_id;
}

/// Required for UITableViewDiffableDataSource to properly track items.
- (NSUInteger)hash {
  return [self.downloadID hash];
}

@end
