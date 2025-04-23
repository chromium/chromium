// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace network {

// Stubbed out implementation of network::mojom::CookieManager for
// tests.
class TestCookieManager : public network::mojom::CookieManager {
 public:
  TestCookieManager();
  ~TestCookieManager() override;

  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& source_url,
                          const net::CookieOptions& cookie_options,
                          SetCanonicalCookieCallback callback) override;
  void GetAllCookies(GetAllCookiesCallback callback) override {}
  void GetAllCookiesWithAccessSemantics(
      GetAllCookiesWithAccessSemanticsCallback callback) override {}
  void GetCookieList(
      const GURL& url,
      const net::CookieOptions& cookie_options,
      const net::CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override {}
  void DeleteCanonicalCookie(const net::CanonicalCookie& cookie,
                             DeleteCanonicalCookieCallback callback) override {}
  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override {}
  void DeleteSessionOnlyCookies(
      DeleteSessionOnlyCookiesCallback callback) override {}
  void DeleteStaleSessionOnlyCookies(
      DeleteStaleSessionOnlyCookiesCallback callback) override {}
  void AddCookieChangeListener(
      const GURL& url,
      const std::optional<std::string>& name,
      mojo::PendingRemote<network::mojom::CookieChangeListener> listener)
      override;
  void AddGlobalChangeListener(
      mojo::PendingRemote<network::mojom::CookieChangeListener>
          notification_pointer) override {}
  void CloneInterface(mojo::PendingReceiver<network::mojom::CookieManager>
                          new_interface) override {}
  void FlushCookieStore(FlushCookieStoreCallback callback) override {}
  void AllowFileSchemeCookies(
      bool allow,
      AllowFileSchemeCookiesCallback callback) override {}
  void SetContentSettings(
      ContentSettingsType type,
      const std::vector<::ContentSettingPatternSource>& settings,
      SetContentSettingsCallback callback) override {}
  void SetForceKeepSessionState() override {}
  void BlockThirdPartyCookies(bool block) override {}
  void SetMitigationsEnabledFor3pcd(bool enable) override {}
  void SetTrackingProtectionEnabledFor3pcd(bool enable) override {}
  void SetPreCommitCallbackDelayForTesting(base::TimeDelta delay) override {}

  virtual void DispatchCookieChange(const net::CookieChangeInfo& change);

 private:
  // List of observers receiving cookie change notifications.
  std::vector<mojo::Remote<network::mojom::CookieChangeListener>>
      cookie_change_listeners_;
};

}  // namespace network
#endif  // SERVICES_NETWORK_TEST_TEST_COOKIE_MANAGER_H_
