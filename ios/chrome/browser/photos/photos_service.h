// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_H_
#define IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "components/keyed_service/core/keyed_service.h"

@protocol SystemIdentity;

// Service responsible for providing access to the Photos Library API.
class PhotosService : public KeyedService {
 public:
  // Progress of an upload operation.
  struct UploadProgress {
    // Number of additionnal bytes sent since last report of progress.
    int64_t bytes_sent = 0;
    // Total number of bytes sent since the upload operation started.
    int64_t total_bytes_sent = 0;
    // Total number of bytes expected to be sent for the upload operation.
    int64_t total_bytes_expected_to_send = 0;
  };

  // Result of an upload operation.
  struct UploadResult {
    // Whether the upload operation is successful.
    bool successful = false;
  };

  // Callback reporting progress of upload operation.
  using UploadProgressCallback =
      base::RepeatingCallback<void(const UploadProgress&)>;
  // Callback reporting completion of an upload operation.
  using UploadCompletionCallback = base::OnceCallback<void(UploadResult)>;

  PhotosService();
  ~PhotosService() override;

  // Whether the service is available e.g. can be used to upload an image.
  virtual bool IsAvailable() const = 0;

  // Upload an image to a Photos user's personal library.
  // - `image_name`: filename of the image.
  // - `image_data`: data representation of the image.
  // - `identity`: identity whose Photos library should be used as destination.
  // - `progress_callback`: called to report progress of the upload.
  // - `completion_callback`: called when the upload operation completes. It
  // will not be called if the operation is canceled using `CancelUpload()`.
  virtual void UploadImage(NSString* image_name,
                           NSData* image_data,
                           id<SystemIdentity> identity,
                           UploadProgressCallback progress_callback,
                           UploadCompletionCallback completion_callback) = 0;

  // Cancel any ongoing upload operation.
  virtual void CancelUpload() = 0;
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_PHOTOS_SERVICE_H_
