// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_
#define IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_

#include "ios/web/public/webui/url_data_source_ios.h"

namespace web {

// A DataSource for chrome://resources/ URLs.
class SharedResourcesDataSourceIOS : public URLDataSourceIOS {
 public:
  SharedResourcesDataSourceIOS();

  SharedResourcesDataSourceIOS(const SharedResourcesDataSourceIOS&) = delete;
  SharedResourcesDataSourceIOS& operator=(const SharedResourcesDataSourceIOS&) =
      delete;

  // web::URLDataSourceIOS implementation.
  std::string GetSource() const override;
  void StartDataRequest(const std::string& path,
                        URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) const override;

 private:
  ~SharedResourcesDataSourceIOS() override;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_SHARED_RESOURCES_DATA_SOURCE_IOS_H_
