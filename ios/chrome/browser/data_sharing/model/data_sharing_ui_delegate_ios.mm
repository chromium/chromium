// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"

#import "base/notimplemented.h"

namespace data_sharing {

DataSharingUIDelegateIOS::DataSharingUIDelegateIOS() = default;
DataSharingUIDelegateIOS::~DataSharingUIDelegateIOS() = default;

void DataSharingUIDelegateIOS::HandleShareURLIntercepted(const GURL& url) {
  NOTIMPLEMENTED();
}

}  // namespace data_sharing
