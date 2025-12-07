// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_RUST_UNZIPPER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_RUST_UNZIPPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"

extern const NSErrorDomain kRustUnzipperErrorDomain;

// A struct to hold the result of the unzip operation.
struct UnzipResultData {
  /// On success, this will be an array of `NSData` objects, with each object
  /// representing the contents of an unzipped file.
  /// On failure, this will be `nil`.
  NSArray<NSData*>* unzipped_files = nil;
  /// On failure, this will contain an `NSError` object with details about why
  /// the operation failed.
  /// On success, this will be `nil`.
  NSError* error = nil;
};

/// Unzip `data` asynchronously. `callback` will be called with the results.
/// If the unzipping was successful, `callback` will be called with an array of
/// the unzipped data and a nil `NSError`. If the unzipping failed, `callback`
/// will be called with a nil array and with a `NSError` describing the failure.
/// Calling this again will cancel ongoing requests.
void UnzipData(NSData* data,
               base::OnceCallback<void(UnzipResultData)> callback);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_RUST_UNZIPPER_H_
