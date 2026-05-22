// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_

#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

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

 private:
  PreloadActivationReportManager();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOAD_ACTIVATION_REPORT_MANAGER_H_
