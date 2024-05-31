// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_COOKIE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_deletion_info.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/tpcd/metadata/manager.h"

namespace net {
class CookieStore;
class URLRequestContext;
}  // namespace net

class GURL;

namespace network {
class FirstPartySetsAccessDelegate;
class SessionCleanupCookieStore;

namespace tpcd::metadata {
class Manager;
}

using SettingsChangeCallback = base::RepeatingClosure;

// Wrap a cookie store in an implementation of the mojo cookie interface.
class COMPONENT_EXPORT(NETWORK_SERVICE) CookieManager
    : public mojom::CookieManager {
 public:
  // Construct a CookieService that can serve mojo requests for the underlying
  // cookie store.  |url_request_context->cookie_store()| must outlive this
  // object. `*first_party_sets_access_delegate` must outlive
  // `url_request_context->cookie_store()`.
  CookieManager(
      net::URLRequestContext* url_request_context,
      FirstPartySetsAccessDelegate* const first_party_sets_access_delegate,
      scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store,
      mojom::CookieManagerParamsPtr params,
      tpcd::metadata::Manager* tpcd_metadata_manager);

  CookieManager(const CookieManager&) = delete;
  CookieManager& operator=(const CookieManager&) = delete;

  ~CookieManager() override;

  // Register a callback to be invoked just before settings change.
  void AddSettingsWillChangeCallback(SettingsChangeCallback callback);

  const CookieSettings& cookie_settings() const { return cookie_settings_; }

  // Bind a cookie receiver to this object.  Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddReceiver(mojo::PendingReceiver<mojom::CookieManager> receiver);

  // TODO(rdsmith): Add a verion of AddRequest that does renderer-appropriate
  // security checks on bindings coming through that interface.

  // mojom::CookieManager
  void GetAllCookies(GetAllCookiesCallback callback) override;
  void GetAllCookiesWithAccessSemantics(
      GetAllCookiesWithAccessSemanticsCallback callback) override;
  void GetCookieList(
      const GURL& url,
      const net::CookieOptions& cookie_options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override;
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override;
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override;
  void SetContentSettings(ContentSettingsType content_settings_type,
                          const ContentSettingsForOneType& settings,
                          SetContentSettingsCallback callback) override;
  void DeleteCookies(mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override;
  void DeleteSessionOnlyCookies(
      DeleteSessionOnlyCookiesCallback callback) override;
  void DeleteStaleSessionOnlyCookies(
      DeleteStaleSessionOnlyCookiesCallback callback) override;
  void AddCookieChangeListener(
      const GURL& url,
      const std::optional<std::string>& name,
      mojo::PendingRemote<mojom::CookieChangeListener> listener) override;
  void AddGlobalChangeListener(
      mojo::PendingRemote<mojom::CookieChangeListener> listener) override;
  void CloneInterface(
      mojo::PendingReceiver<mojom::CookieManager> new_interface) override;
  void SetPreCommitCallbackDelayForTesting(base::TimeDelta delay) override;

  size_t GetClientsBoundForTesting() const { return receivers_.size(); }
  size_t GetListenersRegisteredForTesting() const {
    return listener_registrations_.size();
  }

  void FlushCookieStore(FlushCookieStoreCallback callback) override;
  void AllowFileSchemeCookies(bool allow,
                              AllowFileSchemeCookiesCallback callback) override;
  void SetForceKeepSessionState() override;
  void BlockThirdPartyCookies(bool block) override;
  void SetMitigationsEnabledFor3pcd(bool enable) override;
  void SetTrackingProtectionEnabledFor3pcd(bool enable) override;

  // Configures |out| based on |params|. (This doesn't honor
  // allow_file_scheme_cookies, which affects the cookie store rather than the
  // settings).
  static void ConfigureCookieSettings(
      const network::mojom::CookieManagerParams& params,
      CookieSettings* out);

  // Causes the next call to GetCookieList to crash the process.
  static void CrashOnGetCookieList();

 private:
  // This is called right before settings are about to change. This is used to
  // give a chance to adjust expectations for observers that rely on the
  // previous known settings.
  void OnSettingsWillChange();

  // State associated with a CookieChangeListener.
  struct ListenerRegistration {
    ListenerRegistration();

    ListenerRegistration(const ListenerRegistration&) = delete;
    ListenerRegistration& operator=(const ListenerRegistration&) = delete;

    ~ListenerRegistration();

    // Translates a CookieStore change callback to a CookieChangeListener call.
    void DispatchCookieStoreChange(const net::CookieChangeInfo& change);

    // Owns the callback registration in the store.
    std::unique_ptr<net::CookieChangeSubscription> subscription;

    // The observer receiving change notifications.
    mojo::Remote<mojom::CookieChangeListener> listener;
  };

  // Handles connection errors on change listener pipes.
  void RemoveChangeListener(ListenerRegistration* registration);

  const raw_ptr<net::CookieStore> cookie_store_;
  scoped_refptr<SessionCleanupCookieStore> session_cleanup_cookie_store_;
  mojo::ReceiverSet<mojom::CookieManager> receivers_;
  std::vector<std::unique_ptr<ListenerRegistration>> listener_registrations_;
  // Note: RestrictedCookieManager and CookieAccessDelegate store pointers to
  // |cookie_settings_|.
  CookieSettings cookie_settings_;

  SettingsChangeCallback settings_will_change_callback_;

  base::WeakPtrFactory<CookieManager> weak_factory_{this};
};

COMPONENT_EXPORT(NETWORK_SERVICE)
net::CookieDeletionInfo DeletionFilterToInfo(
    mojom::CookieDeletionFilterPtr filter);

}  // namespace network

#endif  // SERVICES_NETWORK_COOKIE_MANAGER_H_
