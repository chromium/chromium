// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/data_sharing/public/data_sharing_ui_delegate.h"

class ShareKitService;

namespace data_sharing {

// IOS implementation of DataSharingUIDelegate.
class DataSharingUIDelegateIOS : public DataSharingUIDelegate {
 public:
  explicit DataSharingUIDelegateIOS(ShareKitService* share_kit_service);
  ~DataSharingUIDelegateIOS() override;

  DataSharingUIDelegateIOS(const DataSharingUIDelegateIOS&) = delete;
  DataSharingUIDelegateIOS& operator=(const DataSharingUIDelegateIOS&) = delete;
  DataSharingUIDelegateIOS(DataSharingUIDelegateIOS&&) = delete;
  DataSharingUIDelegateIOS& operator=(DataSharingUIDelegateIOS&&) = delete;

  // DataSharingUIDelegate implementation.
  void HandleShareURLIntercepted(
      const GURL& url,
      std::unique_ptr<ShareURLInterceptionContext> context) override;

 private:
  raw_ptr<ShareKitService> share_kit_service_;
};

}  // namespace data_sharing

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_DATA_SHARING_UI_DELEGATE_IOS_H_
