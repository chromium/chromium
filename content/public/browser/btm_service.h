// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BTM_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_BTM_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/btm_redirect_info.h"

class GURL;

namespace content {

class BrowserContext;
class WebContents;

// The primary purpose of BtmService is to allow clients to observe redirect
// chains as soon as the bounce tracking mitigation system detects them. It also
// has a few methods for recording user behavior outside of navigations,
// querying the DIPS database, and triggering tracker storage deletion.
class CONTENT_EXPORT BtmService : public base::SupportsUserData {
 public:
  using DeletedSitesCallback =
      base::OnceCallback<void(const std::vector<std::string>& sites)>;
  using CheckUserActivationCallback = base::OnceCallback<void(bool)>;

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a site bounces the user while accessing storage (e.g.
    // cookies or local storage) in any primary main frame.
    virtual void OnStatefulBounce(WebContents* web_contents) {}
    // Called whenever the BtmService finishes handling a redirect chain (so
    // metadata for its redirects have been written to the DIPS database).
    virtual void OnChainHandled(
        const std::vector<BtmRedirectInfoPtr>& redirects,
        const BtmRedirectChainInfoPtr& chain) {}
  };

  static BtmService* Get(BrowserContext* context);

  // Some embedders support the user signing into the browser. In order to
  // protect the cookies (or other storage) that keeps the user logged in,
  // embedders can call this with the eTLD+1 of the site that stores the data.
  virtual void RecordBrowserSignIn(std::string_view domain) = 0;

  // Embedders can call this to immediately delete the storage of all sites
  // suspected to be bounce trackers by DIPS. (Such deletion is automatically
  // performed periodically even if this method is not called.)
  virtual void DeleteEligibleSitesImmediately(
      DeletedSitesCallback callback) = 0;

  // Unit tests of embedders can use this to simulate user interaction on `url`.
  // (Browser tests should perform actual interactions to ensure realistic test
  // behavior.)
  virtual void RecordUserActivationForTesting(const GURL& url) = 0;

  // Asynchronously checks whether the user ever interacted on the site of `url`
  // since `bound`, and calls `callback` with the result.
  virtual void DidSiteHaveUserActivationSince(
      const GURL& url,
      base::Time bound,
      CheckUserActivationCallback callback) const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(const Observer* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BTM_SERVICE_H_
