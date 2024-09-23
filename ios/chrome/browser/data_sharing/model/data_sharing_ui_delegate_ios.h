// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_

#include "components/data_sharing/public/data_sharing_ui_delegate.h"

namespace data_sharing {

// IOS implementation of DataSharingUIDelegate.
class DataSharingUIDelegateIOS : public DataSharingUIDelegate {
 public:
  explicit DataSharingUIDelegateIOS();
  ~DataSharingUIDelegateIOS() override;

  DataSharingUIDelegateIOS(const DataSharingUIDelegateIOS&) = delete;
  DataSharingUIDelegateIOS& operator=(const DataSharingUIDelegateIOS&) = delete;
  DataSharingUIDelegateIOS(DataSharingUIDelegateIOS&&) = delete;
  DataSharingUIDelegateIOS& operator=(DataSharingUIDelegateIOS&&) = delete;

  // DataSharingUIDelegate implementation.
  void HandleShareURLIntercepted(const GURL& url) override;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_
