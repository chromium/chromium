// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_

#include "net/device_bound_sessions/deletion_reason.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_key.h"
#include "services/network/public/mojom/device_bound_sessions.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::DeviceBoundSessionKeyDataView,
                    net::device_bound_sessions::SessionKey> {
  static const net::SchemefulSite& site(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.site;
  }

  static const std::string& id(
      const net::device_bound_sessions::SessionKey& obj) {
    return obj.id.value();
  }

  static bool Read(network::mojom::DeviceBoundSessionKeyDataView data,
                   net::device_bound_sessions::SessionKey* out);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionAccessType,
                  net::device_bound_sessions::SessionAccess::AccessType> {
  static network::mojom::DeviceBoundSessionAccessType ToMojom(
      net::device_bound_sessions::SessionAccess::AccessType access_type) {
    using enum net::device_bound_sessions::SessionAccess::AccessType;
    switch (access_type) {
      case kCreation:
        return network::mojom::DeviceBoundSessionAccessType::kCreation;
      case kUpdate:
        return network::mojom::DeviceBoundSessionAccessType::kUpdate;
      case kTermination:
        return network::mojom::DeviceBoundSessionAccessType::kTermination;
    }
  }

  static bool FromMojom(
      network::mojom::DeviceBoundSessionAccessType input,
      net::device_bound_sessions::SessionAccess::AccessType* output) {
    using enum net::device_bound_sessions::SessionAccess::AccessType;
    switch (input) {
      case network::mojom::DeviceBoundSessionAccessType::kCreation:
        *output = kCreation;
        return true;
      case network::mojom::DeviceBoundSessionAccessType::kUpdate:
        *output = kUpdate;
        return true;
      case network::mojom::DeviceBoundSessionAccessType::kTermination:
        *output = kTermination;
        return true;
    }
  }
};

template <>
struct StructTraits<network::mojom::DeviceBoundSessionAccessDataView,
                    net::device_bound_sessions::SessionAccess> {
  static net::device_bound_sessions::SessionAccess::AccessType access_type(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.access_type;
  }

  static const net::device_bound_sessions::SessionKey& session_key(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.session_key;
  }

  static const std::vector<std::string>& cookies(
      const net::device_bound_sessions::SessionAccess& access) {
    return access.cookies;
  }

  static bool Read(network::mojom::DeviceBoundSessionAccessDataView data,
                   net::device_bound_sessions::SessionAccess* out);
};

template <>
struct EnumTraits<network::mojom::DeviceBoundSessionDeletionReason,
                  net::device_bound_sessions::DeletionReason> {
  static network::mojom::DeviceBoundSessionDeletionReason ToMojom(
      net::device_bound_sessions::DeletionReason reason) {
    using enum net::device_bound_sessions::DeletionReason;
    switch (reason) {
      case kExpired:
        return network::mojom::DeviceBoundSessionDeletionReason::kExpired;
      case kFailedToRestoreKey:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kFailedToRestoreKey;
      case kFailedToUnwrapKey:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kFailedToUnwrapKey;
      case kStoragePartitionCleared:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kStoragePartitionCleared;
      case kClearBrowsingData:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kClearBrowsingData;
      case kServerRequested:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kServerRequested;
      case kInvalidSessionParams:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kInvalidSessionParams;
      case kRefreshFatalError:
        return network::mojom::DeviceBoundSessionDeletionReason::
            kRefreshFatalError;
    }
  }

  static bool FromMojom(network::mojom::DeviceBoundSessionDeletionReason input,
                        net::device_bound_sessions::DeletionReason* output) {
    using enum net::device_bound_sessions::DeletionReason;
    switch (input) {
      case network::mojom::DeviceBoundSessionDeletionReason::kExpired:
        *output = kExpired;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kFailedToRestoreKey:
        *output = kFailedToRestoreKey;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kFailedToUnwrapKey:
        *output = kFailedToUnwrapKey;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kStoragePartitionCleared:
        *output = kStoragePartitionCleared;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kClearBrowsingData:
        *output = kClearBrowsingData;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kServerRequested:
        *output = kServerRequested;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::
          kInvalidSessionParams:
        *output = kInvalidSessionParams;
        return true;
      case network::mojom::DeviceBoundSessionDeletionReason::kRefreshFatalError:
        *output = kRefreshFatalError;
        return true;
    }
  }
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DEVICE_BOUND_SESSIONS_MOJOM_TRAITS_H_
