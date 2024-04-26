// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_deletion_info.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

class PrefService;
class SafeBrowsingClient;

namespace base {
class FilePath;
}

namespace network {
class SharedURLLoaderFactory;

namespace mojom {
class NetworkContext;
}
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
class SafeBrowsingUrlCheckerImpl;
class SafeBrowsingMetricsCollector;
}  // namespace safe_browsing

namespace web {
class WebState;
}  // namespace web

// Manages Safe Browsing related functionality. This class owns and provides
// support for constructing and initializing the Safe Browsing database.
// This class is RefCounted so that PostTask'd calls into this class can retain
// a reference to an instance.
class SafeBrowsingService
    : public base::RefCountedThreadSafe<SafeBrowsingService> {
 public:
  SafeBrowsingService(const SafeBrowsingService&) = delete;
  SafeBrowsingService& operator=(const SafeBrowsingService&) = delete;

  // Called on the UI thread to initialize the service.
  virtual void Initialize(PrefService* prefs,
                          const base::FilePath& user_data_path,
                          safe_browsing::SafeBrowsingMetricsCollector*
                              safe_browsing_metrics_collector) = 0;

  // Called on the UI thread to terminate the service. This must be called
  // before the IO thread is torn down.
  virtual void ShutDown() = 0;

  // Creates a SafeBrowsingUrlCheckerImpl that can be used to query the
  // SafeBrowsingDatabaseManager owned by this service.
  virtual std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
  CreateUrlChecker(network::mojom::RequestDestination request_destination,
                   web::WebState* web_state,
                   SafeBrowsingClient* client) = 0;

  // Creates a SafeBrowsingUrlCheckerImpl that can be used for async checks.
  virtual std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
  CreateAsyncChecker(network::mojom::RequestDestination request_destination,
                     web::WebState* web_state,
                     SafeBrowsingClient* client) = 0;

  // Creates a SafeBrowsingUrlCheckerImpl that can be used for sync checks which
  // handles checks not related to real time.
  virtual std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
  CreateSyncChecker(network::mojom::RequestDestination request_destination,
                    web::WebState* web_state,
                    SafeBrowsingClient* client) = 0;

  // Checks if async check should be created.
  virtual bool ShouldCreateAsyncChecker(web::WebState* web_state,
                                        SafeBrowsingClient* client) = 0;

  // Returns true if `url` has a scheme that is handled by Safe Browsing.
  virtual bool CanCheckUrl(const GURL& url) const = 0;

  // Returns the SharedURLLoaderFactory used for Safe Browsing network requests.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns the SafeBrowsingDatabaseManager owned by this service.
  virtual scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
  GetDatabaseManager() = 0;

  // Returns the network context owned by this service.
  virtual network::mojom::NetworkContext* GetNetworkContext() = 0;

  // Clears cookies if the given deletion time range is for "all time". Calls
  // the given `callback` once deletion is complete.
  virtual void ClearCookies(
      const net::CookieDeletionInfo::TimeRange& creation_range,
      base::OnceClosure callback) = 0;

 protected:
  SafeBrowsingService() = default;
  virtual ~SafeBrowsingService() = default;

 private:
  friend class base::RefCountedThreadSafe<SafeBrowsingService>;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_H_
