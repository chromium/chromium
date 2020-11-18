// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
#define CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/types/pass_key.h"
#include "content/public/browser/url_data_source.h"

namespace content {

// A DataSource for chrome://resources/ and chrome-untrusted://resources/ URLs.
// TODO(https://crbug.com/866236): chrome-untrusted://resources/ is not
// currently fully functional, as some resources have absolute
// chrome://resources URLs. If you need access to chrome-untrusted://resources/
// resources that are not currently functional, it is up to you to get them
// working.
class SharedResourcesDataSource : public URLDataSource {
 public:
  using PassKey = base::PassKey<SharedResourcesDataSource>;

  // Creates a SharedResourcesDataSource instance for chrome://resources.
  static std::unique_ptr<SharedResourcesDataSource> CreateForChromeScheme();

  // Creates a SharedResourcesDataSource instance for
  // chrome-untrusted://resources.
  static std::unique_ptr<SharedResourcesDataSource>
  CreateForChromeUntrustedScheme();

  SharedResourcesDataSource(PassKey, const std::string& scheme);
  SharedResourcesDataSource(const SharedResourcesDataSource&) = delete;
  SharedResourcesDataSource& operator=(const SharedResourcesDataSource&) =
      delete;
  ~SharedResourcesDataSource() override;

  // URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(const GURL& url,
                        const WebContents::Getter& wc_getter,
                        URLDataSource::GotDataCallback callback) override;
  bool AllowCaching() override;
  std::string GetMimeType(const std::string& path) override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

 private:
  // The URL scheme this data source is accessed from, e.g. "chrome" or
  // "chrome-untrusted".
  const std::string scheme_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
