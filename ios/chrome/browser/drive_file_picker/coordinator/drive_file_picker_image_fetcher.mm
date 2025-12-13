// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_image_fetcher.h"

#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_helper.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace {

// Dimension to resize background images for shared drives.
constexpr int kBackgroundImageResizeDimension = 64;
// Dimension to resize thumbnails.
constexpr int kThumbnailResizeDimension = 64;

}  // namespace

DriveFilePickerImageFetcher::DriveFilePickerImageFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : image_data_fetcher_(std::make_unique<image_fetcher::ImageDataFetcher>(
          url_loader_factory)),
      images_pending_([NSMutableSet set]),
      image_cache_([[NSCache alloc] init]),
      image_transcoder_(std::make_unique<web::JavaScriptImageTranscoder>()) {}

DriveFilePickerImageFetcher::~DriveFilePickerImageFetcher() = default;

void DriveFilePickerImageFetcher::FetchImage(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback) {
  NSString* image_link = item.GetImageLink();
  if (!image_link) {
    // If there is no link available, invoke callback with no image. This is
    // asynchronous as it should behave the same regardless of the input.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DriveFilePickerImageFetcher::InvokeCallbackWithFetchedImage,
            weak_ptr_factory_.GetWeakPtr(), std::move(item),
            std::move(callback), nil));
    return;
  }
  [images_pending_ addObject:image_link];
  // If there is a cached image, use it.
  UIImage* cached_image = [image_cache_ objectForKey:image_link];
  if (cached_image) {
    // This is asynchronous as it should behave the same as fetching from the
    // network.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DriveFilePickerImageFetcher::InvokeCallbackWithFetchedImage,
            weak_ptr_factory_.GetWeakPtr(), std::move(item),
            std::move(callback), cached_image));
    return;
  }
  // Otherwise fetch the image from the network.
  GURL image_url = GURL(base::SysNSStringToUTF16(image_link));
  image_data_fetcher_->FetchImageData(
      image_url,
      base::BindOnce(&DriveFilePickerImageFetcher::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(item),
                     std::move(callback)),
      NO_TRAFFIC_ANNOTATION_YET);
}

BOOL DriveFilePickerImageFetcher::IsFetchInProgress(const DriveItem& item) {
  return [images_pending_ containsObject:item.GetImageLink()];
}

void DriveFilePickerImageFetcher::OnImageFetched(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback,
    const std::string& image_data,
    const image_fetcher::RequestMetadata& metadata) {
  NSData* image_ns_data = [NSData dataWithBytes:image_data.data()
                                         length:image_data.length()];
  ProcessUnsafeImageData(std::move(item), std::move(callback), image_ns_data);
}

void DriveFilePickerImageFetcher::OnImageTranscoded(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback,
    NSData* safe_data,
    NSError* error) {
  if (safe_data) {
    ProcessSafeImageData(std::move(item), std::move(callback), safe_data);
  } else {
    // If there is no data, invoke callback with no image.
    InvokeCallbackWithFetchedImage(std::move(item), std::move(callback), nil);
  }
}

void DriveFilePickerImageFetcher::ProcessUnsafeImageData(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback,
    NSData* unsafe_image_data) {
  NSNumber* resized_width = nil;
  NSNumber* resized_height = nil;
  if (item.GetImageType() == DriveItem::ImageType::kBackground) {
    resized_width = resized_height = @(kBackgroundImageResizeDimension);
  }
  image_transcoder_->TranscodeImage(
      unsafe_image_data, @"image/png", resized_width, resized_height, nil,
      base::BindOnce(&DriveFilePickerImageFetcher::OnImageTranscoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(item),
                     std::move(callback)));
}

void DriveFilePickerImageFetcher::ProcessSafeImageData(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback,
    NSData* image_data) {
  UIImage* image = [UIImage imageWithData:image_data];
  if (item.GetImageType() == DriveItem::ImageType::kThumbnail) {
    image = ResizeImage(
        image, CGSizeMake(kThumbnailResizeDimension, kThumbnailResizeDimension),
        ProjectionMode::kAspectFill);
  } else {
    // Only non-thumbnail images are cached.
    [image_cache_ setObject:image forKey:item.GetImageLink()];
  }
  InvokeCallbackWithFetchedImage(std::move(item), std::move(callback), image);
}

void DriveFilePickerImageFetcher::InvokeCallbackWithFetchedImage(
    DriveItem item,
    DriveFilePickerImageFetcherCallback callback,
    UIImage* image) {
  [images_pending_ removeObject:item.GetImageLink()];
  std::move(callback).Run(std::move(item), image);
}
