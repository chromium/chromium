// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_IMAGE_FETCHER_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_IMAGE_FETCHER_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/image_fetcher/core/request_metadata.h"

namespace image_fetcher {
class ImageDataFetcher;
}
namespace network {
class SharedURLLoaderFactory;
}
namespace web {
class JavaScriptImageTranscoder;
}

struct DriveItem;

// Callback with a fetched image, or nil if the image could not be fetched.
using DriveFilePickerImageFetcherCallback =
    base::OnceCallback<void(DriveItem item, UIImage*)>;

// Class to fetch images for the Drive file picker. It either fetches images
// from a cache or otherwise from the network.
class DriveFilePickerImageFetcher {
 public:
  explicit DriveFilePickerImageFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~DriveFilePickerImageFetcher();

  // Fetches an image for a given `item`. The `callback` will be called
  // asynchronously with the fetched image, or nil if the image could not be
  // fetched.
  void FetchImage(DriveItem item, DriveFilePickerImageFetcherCallback callback);

  // Returns whether a fetch for `item` is in progress.
  BOOL IsFetchInProgress(const DriveItem& item);

 private:
  // Called when the image data has been fetched.
  void OnImageFetched(DriveItem item,
                      DriveFilePickerImageFetcherCallback callback,
                      const std::string& image_data,
                      const image_fetcher::RequestMetadata& metadata);

  // Called when the image has been transcoded.
  void OnImageTranscoded(DriveItem item,
                         DriveFilePickerImageFetcherCallback callback,
                         NSData* safe_data,
                         NSError* error);

 private:
  // Transcodes `unsafe_image_data` into safe image data and forwards that safe
  // image data to `ProcessSafeImageData`.
  void ProcessUnsafeImageData(DriveItem item,
                              DriveFilePickerImageFetcherCallback callback,
                              NSData* unsafe_image_data);

  // Decodes and optionally caches `image_data` using the image link `item` as
  // key and forwards the image to `InvokeCallbackWithFetchedImage`.
  void ProcessSafeImageData(DriveItem item,
                            DriveFilePickerImageFetcherCallback callback,
                            NSData* image_data);

  // Removes `image_link` from the set of images pending fetching from
  // cache/network and invokes `callback` with the provided `image`.
  void InvokeCallbackWithFetchedImage(
      DriveItem item,
      DriveFilePickerImageFetcherCallback callback,
      UIImage* image);

  std::unique_ptr<image_fetcher::ImageDataFetcher> image_data_fetcher_;
  // The set of images being fetched, soon to be added to `imageCache_`.
  NSMutableSet<NSString*>* images_pending_;
  // Cache of fetched images for the Drive file picker.
  NSCache<NSString*, UIImage*>* image_cache_;
  // JavaScript image transcoder to locally re-encode icons, thumbnails, etc.
  std::unique_ptr<web::JavaScriptImageTranscoder> image_transcoder_;

  base::WeakPtrFactory<DriveFilePickerImageFetcher> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_IMAGE_FETCHER_H_
