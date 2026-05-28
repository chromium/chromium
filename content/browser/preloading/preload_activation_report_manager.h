// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_

#include <list>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {

class BrowserContext;
class WebContents;

// Manages sending activation beacons for prefetched or prerendered pages
// that have been consumed.
// This is bound to `BrowserContext` so that beacon requests can outlive
// their initiator documents.
class CONTENT_EXPORT PreloadActivationReportManager
    : public base::SupportsUserData::Data {
 public:
  ~PreloadActivationReportManager() override;

  PreloadActivationReportManager(const PreloadActivationReportManager&) =
      delete;
  PreloadActivationReportManager& operator=(
      const PreloadActivationReportManager&) = delete;

  static PreloadActivationReportManager* GetOrCreateForBrowserContext(
      BrowserContext* browser_context);

  // Sends a credentialless HEAD request to the specified endpoint.
  void ReportActivation(const GURL& endpoint, WebContents* web_contents);

  size_t GetLoaderCountForTesting() const { return loaders_.size(); }

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> factory) {
    url_loader_factory_for_testing_ = std::move(factory);
  }

 private:
  using UrlLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  PreloadActivationReportManager();

  void OnRedirect(UrlLoaderList::iterator it,
                  const url::Origin& original_origin,
                  const net::RedirectInfo& redirect_info);
  void OnComplete(UrlLoaderList::iterator it);
  void RemoveLoader(UrlLoaderList::iterator it);

  UrlLoaderList loaders_;
  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
  base::WeakPtrFactory<PreloadActivationReportManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_
