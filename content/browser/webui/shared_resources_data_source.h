// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
#define CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/url_data_source.h"

namespace content {

// A DataSource for chrome://resources/ URLs.
class SharedResourcesDataSource : public URLDataSource {
 public:
  SharedResourcesDataSource();

  // URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const WebContents::Getter& wc_getter,
      const URLDataSource::GotDataCallback& callback) override;
  bool AllowCaching() override;
  std::string GetMimeType(const std::string& path) override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerForRequestPath(
      const std::string& path) override;
  std::string GetAccessControlAllowOriginForOrigin(
      const std::string& origin) override;
#if defined(OS_CHROMEOS)
  void DisablePolymer2ForHost(const std::string& host) override;
  std::string GetContentSecurityPolicyWorkerSrc() override;
#endif  // defined (OS_CHROMEOS)

 private:
#if defined(OS_CHROMEOS)
  std::string disabled_polymer2_host_;

  bool IsPolymer2DisabledForPage(const WebContents::Getter& wc_getter);
#endif  // defined (OS_CHROMEOS)

  ~SharedResourcesDataSource() override;

  DISALLOW_COPY_AND_ASSIGN(SharedResourcesDataSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
