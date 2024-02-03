// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_file_uploader.h"

DriveFileUploader::DriveFileUploader() = default;

DriveFileUploader::~DriveFileUploader() = default;

// TODO(crbug.com/1495354): Remove this implementation once all subclasses
// provide their own.
void DriveFileUploader::FetchStorageQuota(
    DriveStorageQuotaCompletionCallback completion_callback) {}
