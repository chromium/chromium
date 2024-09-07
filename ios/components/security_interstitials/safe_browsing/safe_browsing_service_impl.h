// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_IMPL_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_IMPL_H_

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class PrefChangeRegistrar;

namespace net {
class URLRequestContext;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}  // namespace safe_browsing

// This class must be created on the UI thread.
class SafeBrowsingServiceImpl : public SafeBrowsingService {
 public:
  SafeBrowsingServiceImpl();

  SafeBrowsingServiceImpl(const SafeBrowsingServiceImpl&) = delete;
  SafeBrowsingServiceImpl& operator=(const SafeBrowsingServiceImpl&) = delete;

  // SafeBrowsingService:
  void Initialize(PrefService* prefs,
                  const base::FilePath& user_data_path,
                  safe_browsing::SafeBrowsingMetricsCollector*
                      safe_browsing_metrics_collector) override;
  void ShutDown() override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateUrlChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateAsyncChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> CreateSyncChecker(
      network::mojom::RequestDestination request_destination,
      web::WebState* web_state,
      SafeBrowsingClient* client) override;
  bool ShouldCreateAsyncChecker(web::WebState* web_state,
                                SafeBrowsingClient* client) override;
  bool CanCheckUrl(const GURL& url) const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> GetDatabaseManager()
      override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  void ClearCookies(const net::CookieDeletionInfo::TimeRange& creation_range,
                    base::OnceClosure callback) override;

 private:
  // A helper class for enabling/disabling Safe Browsing and maintaining state
  // on the IO thread. This class may be constructed and destroyed on the UI
  // thread, but all of its other methods should only be called on the IO
  // thread.
  class IOThreadEnabler : public base::RefCountedThreadSafe<IOThreadEnabler> {
   public:
    IOThreadEnabler();

    IOThreadEnabler(const IOThreadEnabler&) = delete;
    IOThreadEnabler& operator=(const IOThreadEnabler&) = delete;

    // Creates the network context and URL loader factory used by the
    // SafeBrowsingDatabaseManager.
    void Initialize(mojo::PendingReceiver<network::mojom::NetworkContext>
                        network_context_receiver,
                    const base::FilePath& safe_browsing_data_path);

    // Disables Safe Browsing, and destroys the network context and URL loader
    // factory used by the SafeBrowsingDatabaseManager.
    void ShutDown();

    // Clears all cookies. Calls the given `callback` when deletion is complete.
    void ClearAllCookies(base::OnceClosure callback);

   private:
    friend base::RefCountedThreadSafe<IOThreadEnabler>;
    ~IOThreadEnabler();

    // Constructs a URLRequestContext, using the given path as the location for
    // the cookie store.
    void SetUpURLRequestContext(const base::FilePath& safe_browsing_data_path);

    // This is wrapped by `network_context`.
    std::unique_ptr<net::URLRequestContext> url_request_context_;

    // The network context used for Safe Browsing related network requests.
    std::unique_ptr<network::NetworkContext> network_context_;
  };

  ~SafeBrowsingServiceImpl() override;

  // Called on the UI thread to construct a URLLoaderFactory that is used on
  // the IO thread.
  void SetUpURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver);

  // Enables or disables Safe Browsing, depending on the current state of
  // preferences.
  void UpdateSafeBrowsingEnabledState();

  // This is the UI thread remote for IOThreadState's network context.
  mojo::Remote<network::mojom::NetworkContext> network_context_client_;

  // The URLLoaderFactory used for Safe Browsing network requests.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  // A PendingReceiver for `url_loader_factory_`, used during service
  // initialization.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory>
      url_loader_factory_pending_receiver_;

  // A SharedURLLoaderFactory that wraps `url_loader_factory_`.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;

  // Constructed on the UI thread, but otherwise its methods are only called on
  // the IO thread.
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;

  // This tracks whether the service is running.
  bool enabled_ = false;

  // This watches for changes to the Safe Browsing opt-out preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Encapsulates methods and objects that are used on the IO thread.
  scoped_refptr<IOThreadEnabler> io_thread_enabler_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_SERVICE_IMPL_H_
