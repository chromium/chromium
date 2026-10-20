// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESTRICTED_DEVICE_BOUND_SESSION_MANAGER_IMPL_H_
#define SERVICES_NETWORK_RESTRICTED_DEVICE_BOUND_SESSION_MANAGER_IMPL_H_

#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom.h"
#include "url/origin.h"

namespace net::device_bound_sessions {
struct SessionEvent;
struct SessionKey;
class SessionService;
}  // namespace net::device_bound_sessions

namespace network {

class CookieSettings;

// Observes session events through SessionService::AddEventObserver().
// One instance per Document, owned by a mojo::UniqueReceiverSet in
// NetworkContext.
class COMPONENT_EXPORT(NETWORK_SERVICE) RestrictedDeviceBoundSessionManagerImpl
    : public mojom::RestrictedDeviceBoundSessionManager {
 public:
  // `origin` is the Document's committed origin, used for scope and cookie
  // checks. `isolation_info` provides the cookie context (`site_for_cookies`,
  // `top_frame_origin`, `nonce`). The browser binds a manager only for a
  // Document whose `origin` is potentially trustworthy and not opaque, and
  // whose `isolation_info` has no nonce and is first-party for `origin`.
  // CHECKs these and `isolation_info.frame_origin() == origin`.
  explicit RestrictedDeviceBoundSessionManagerImpl(
      net::device_bound_sessions::SessionService& session_service,
      const CookieSettings& cookie_settings,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info);

  RestrictedDeviceBoundSessionManagerImpl(
      const RestrictedDeviceBoundSessionManagerImpl&) = delete;
  RestrictedDeviceBoundSessionManagerImpl& operator=(
      const RestrictedDeviceBoundSessionManagerImpl&) = delete;

  ~RestrictedDeviceBoundSessionManagerImpl() override;

  // mojom::RestrictedDeviceBoundSessionManager:
  void SubscribeToRegistrations(
      mojo::PendingAssociatedRemote<mojom::RestrictedDeviceBoundSessionObserver>
          observer,
      SubscribeToRegistrationsCallback callback) override;

 private:
  // Returns true if the site-level cookie setting gives the Document full
  // cookie access. Not cached, because the user can change the setting at any
  // time.
  bool HasSiteDataAccess() const;

  // Called for every session event in this profile. Pushes each newly created
  // session that this Document may see to `observers_`.
  void OnSessionEvent(const net::device_bound_sessions::SessionEvent& event);

  // Builds the SubscribeToRegistrations reply from the keys that
  // GetAllSessionsAsync() returns.
  void OnAllSessionKeys(
      SubscribeToRegistrationsCallback callback,
      const std::vector<net::device_bound_sessions::SessionKey>& session_keys);

  // Returns the renderer projection of the session for `session_key` if it
  // exists and is visible to origin_, otherwise null.
  mojom::RestrictedDeviceBoundSessionPtr ResolveVisibleSession(
      const net::device_bound_sessions::SessionKey& session_key) const;

  // Releases `subscription_` once the last observer disconnects.
  void OnObserverDisconnected(mojo::RemoteSetElementId id);

  // Both point to objects that outlive this one: NetworkContext declares their
  // owners before `restricted_device_bound_session_managers_`.
  const raw_ref<net::device_bound_sessions::SessionService> session_service_;
  const raw_ref<const CookieSettings> cookie_settings_;

  // Document context, bound once by the Browser Process broker. Only values
  // that cannot change for the Document's life are bound here.
  const url::Origin origin_;
  const net::IsolationInfo isolation_info_;
  const net::SchemefulSite site_;

  // Observers registered by this Document.
  mojo::AssociatedRemoteSet<mojom::RestrictedDeviceBoundSessionObserver>
      observers_;

  // One SessionService::AddEventObserver() subscription, held while
  // `observers_` is not empty.
  base::CallbackListSubscription subscription_;

  // GetAllSessionsAsync() can reply after this object is destroyed.
  base::WeakPtrFactory<RestrictedDeviceBoundSessionManagerImpl> weak_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESTRICTED_DEVICE_BOUND_SESSION_MANAGER_IMPL_H_
