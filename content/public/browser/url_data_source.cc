// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/url_data_source.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

URLDataSource* GetSourceForURLHelper(ResourceContext* resource_context,
                                     const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  URLDataSourceImpl* source =
      GetURLDataManagerForResourceContext(resource_context)
          ->GetDataSourceFromURL(url);
  return source->source();
}

}  // namespace

// static
void URLDataSource::Add(BrowserContext* browser_context,
                        std::unique_ptr<URLDataSource> source) {
  URLDataManager::AddDataSource(browser_context, std::move(source));
}

// static
void URLDataSource::GetSourceForURL(
    BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(URLDataSource*)> callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&GetSourceForURLHelper,
                     browser_context->GetResourceContext(), url),
      std::move(callback));
}

// static
std::string URLDataSource::URLToRequestPath(const GURL& url) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    return spec.substr(offset);

  return std::string();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLDataSource::TaskRunnerForRequestPath(const std::string& path) {
  return base::CreateSingleThreadTaskRunner({BrowserThread::UI});
}

bool URLDataSource::ShouldReplaceExistingSource() {
  return true;
}

bool URLDataSource::AllowCaching() {
  return true;
}

bool URLDataSource::ShouldAddContentSecurityPolicy() {
  return true;
}

std::string URLDataSource::GetContentSecurityPolicyScriptSrc() {
  // Note: Do not add 'unsafe-eval' here. Instead override CSP for the
  // specific pages that need it, see context http://crbug.com/525224.
  return "script-src chrome://resources 'self';";
}

std::string URLDataSource::GetContentSecurityPolicyObjectSrc() {
  return "object-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyChildSrc() {
  return "child-src 'none';";
}

std::string URLDataSource::GetContentSecurityPolicyStyleSrc() {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyImgSrc() {
  return std::string();
}

std::string URLDataSource::GetContentSecurityPolicyWorkerSrc() {
  return std::string();
}

bool URLDataSource::ShouldDenyXFrameOptions() {
  return true;
}

bool URLDataSource::ShouldServiceRequest(const GURL& url,
                                         ResourceContext* resource_context,
                                         int render_process_id) {
  return url.SchemeIs(kChromeDevToolsScheme) || url.SchemeIs(kChromeUIScheme);
}

bool URLDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return false;
}

std::string URLDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) {
  return std::string();
}

void URLDataSource::DisablePolymer2ForHost(const std::string& host) {}

bool URLDataSource::ShouldReplaceI18nInJS() {
  return false;
}

}  // namespace content
