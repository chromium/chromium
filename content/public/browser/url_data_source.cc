// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"

namespace content {

// static
void URLDataSource::Add(BrowserContext* browser_context,
                        std::unique_ptr<URLDataSource> source) {
  URLDataManager::AddDataSource(browser_context, std::move(source));
}

scoped_refptr<base::SingleThreadTaskRunner>
URLDataSource::TaskRunnerForRequestPath(const std::string& path) const {
  return base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
}

bool URLDataSource::ShouldReplaceExistingSource() const {
  return true;
}

bool URLDataSource::AllowCaching() const {
  return true;
}

bool URLDataSource::ShouldAddContentSecurityPolicy() const {
  return true;
}

std::string URLDataSource::GetContentSecurityPolicyScriptSrc() const {
  // Note: Do not add 'unsafe-eval' here. Instead override CSP for the
  // specific pages that need it, see context http://crbug.com/525224.
  return "script-src chrome://resources 'self';";
}

std::string URLDataSource::GetContentSecurityPolicyObjectSrc() const {
  return "object-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyChildSrc() const {
  return "child-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyStyleSrc() const {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyImgSrc() const {
  return std::string();
}

bool URLDataSource::ShouldDenyXFrameOptions() const {
  return true;
}

bool URLDataSource::ShouldServiceRequest(const GURL& url,
                                         ResourceContext* resource_context,
                                         int render_process_id) const {
  return url.SchemeIs(kChromeDevToolsScheme) || url.SchemeIs(kChromeUIScheme);
}

bool URLDataSource::ShouldServeMimeTypeAsContentTypeHeader() const {
  return false;
}

std::string URLDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
  return std::string();
}

bool URLDataSource::IsGzipped(const std::string& path) const {
  return false;
}

}  // namespace content
