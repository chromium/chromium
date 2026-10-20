// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_device_bound_session_manager_impl.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "net/base/is_potentially_trustworthy.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_event.h"
#include "net/device_bound_sessions/session_key.h"
#include "net/device_bound_sessions/session_service.h"
#include "services/network/cookie_settings.h"
#include "url/gurl.h"

namespace network {

RestrictedDeviceBoundSessionManagerImpl::
    RestrictedDeviceBoundSessionManagerImpl(
        net::device_bound_sessions::SessionService& session_service,
        const CookieSettings& cookie_settings,
        const url::Origin& origin,
        const net::IsolationInfo& isolation_info)
    : session_service_(session_service),
      cookie_settings_(cookie_settings),
      origin_(origin),
      isolation_info_(isolation_info),
      site_(origin_) {
  // TODO(crbug.com/494489935): Update this condition before a worker can bind
  // a manager. Only frames bind one today.
  CHECK(isolation_info_.frame_origin() == origin_);
  CHECK(!origin_.opaque());
  CHECK(net::IsOriginPotentiallyTrustworthy(origin_));
  CHECK(!isolation_info_.nonce().has_value());
  // TODO(crbug.com/353772143): When DBSC supports partitioned cookies, let
  // third-party contexts through, so that each sees the sessions in its own
  // cookie partition.
  CHECK(isolation_info_.site_for_cookies().IsFirstParty(origin_.GetURL()));
  // base::Unretained is safe: `observers_` is a member, so destroying this
  // object drops the handler.
  observers_.set_disconnect_handler(base::BindRepeating(
      &RestrictedDeviceBoundSessionManagerImpl::OnObserverDisconnected,
      base::Unretained(this)));
}

RestrictedDeviceBoundSessionManagerImpl::
    ~RestrictedDeviceBoundSessionManagerImpl() = default;

void RestrictedDeviceBoundSessionManagerImpl::SubscribeToRegistrations(
    mojo::PendingAssociatedRemote<mojom::RestrictedDeviceBoundSessionObserver>
        observer,
    SubscribeToRegistrationsCallback callback) {
  if (!subscription_) {
    // base::Unretained is safe: `subscription_` is a member, so destroying
    // this object cancels the callback.
    subscription_ = session_service_->AddEventObserver(base::BindRepeating(
        &RestrictedDeviceBoundSessionManagerImpl::OnSessionEvent,
        base::Unretained(this)));
  }

  // Added before GetAllSessionsAsync() copies the session keys, so a session
  // stored after the copy is pushed to `observer`.
  observers_.Add(std::move(observer));

  session_service_->GetAllSessionsAsync(
      base::BindOnce(&RestrictedDeviceBoundSessionManagerImpl::OnAllSessionKeys,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

bool RestrictedDeviceBoundSessionManagerImpl::HasSiteDataAccess() const {
  // TODO(crbug.com/353772143): Pass the Document's cookie partition key, built
  // from `isolation_info_`, once DBSC supports partitioned cookies.
  return cookie_settings_->IsFullCookieAccessAllowed(
      origin_.GetURL(), isolation_info_.site_for_cookies(),
      isolation_info_.top_frame_origin(), /*overrides=*/{},
      /*cookie_partition_key=*/std::nullopt);
}

void RestrictedDeviceBoundSessionManagerImpl::OnSessionEvent(
    const net::device_bound_sessions::SessionEvent& event) {
  if (!event.succeeded || event.site != site_ ||
      !std::holds_alternative<net::device_bound_sessions::CreationEventDetails>(
          event.event_type_details) ||
      !event.session_id.has_value()) {
    return;
  }

  if (!HasSiteDataAccess()) {
    return;
  }

  mojom::RestrictedDeviceBoundSessionPtr session =
      ResolveVisibleSession(net::device_bound_sessions::SessionKey{
          event.site,
          net::device_bound_sessions::SessionKey::Id(*event.session_id)});
  if (!session) {
    return;
  }

  for (auto& observer : observers_) {
    observer->OnDeviceBoundSessionRegistered(session.Clone());
  }
}

void RestrictedDeviceBoundSessionManagerImpl::OnAllSessionKeys(
    SubscribeToRegistrationsCallback callback,
    const std::vector<net::device_bound_sessions::SessionKey>& session_keys) {
  if (!HasSiteDataAccess()) {
    std::move(callback).Run({});
    return;
  }

  std::vector<mojom::RestrictedDeviceBoundSessionPtr> sessions;
  for (const net::device_bound_sessions::SessionKey& session_key :
       session_keys) {
    if (session_key.site != site_) {
      continue;
    }
    mojom::RestrictedDeviceBoundSessionPtr session =
        ResolveVisibleSession(session_key);
    if (session) {
      sessions.push_back(std::move(session));
    }
  }
  std::move(callback).Run(std::move(sessions));
}

mojom::RestrictedDeviceBoundSessionPtr
RestrictedDeviceBoundSessionManagerImpl::ResolveVisibleSession(
    const net::device_bound_sessions::SessionKey& session_key) const {
  const net::device_bound_sessions::Session* session =
      session_service_->GetSession(session_key);
  if (!session || !session->IsVisibleToOrigin(origin_)) {
    return nullptr;
  }
  return mojom::RestrictedDeviceBoundSession::New(
      session->id().value(), session->refresh_url(), session->creation_date());
}

void RestrictedDeviceBoundSessionManagerImpl::OnObserverDisconnected(
    mojo::RemoteSetElementId id) {
  if (observers_.empty()) {
    subscription_ = {};
  }
}

}  // namespace network
