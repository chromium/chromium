// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cros/credential_store.h"

#include "chromeos/dbus/u2f/u2f_client.h"
#include "chromeos/dbus/u2f/u2f_interface.pb.h"
#include "components/device_event_log/device_event_log.h"

namespace device {
namespace fido {
namespace cros {

PlatformAuthenticatorCredentialStore::PlatformAuthenticatorCredentialStore() {}
PlatformAuthenticatorCredentialStore::~PlatformAuthenticatorCredentialStore() {}

void PlatformAuthenticatorCredentialStore::DeleteCredentials(
    base::Time created_not_before,
    base::Time created_not_after,
    base::OnceClosure callback) {
  chromeos::U2FClient::IsU2FServiceAvailable(
      base::BindOnce(&PlatformAuthenticatorCredentialStore::DoDeleteCredentials,
                     weak_factory_.GetWeakPtr(), created_not_before,
                     created_not_after, std::move(callback)));
}

void PlatformAuthenticatorCredentialStore::CountCredentials(
    base::Time created_not_before,
    base::Time created_not_after,
    base::OnceCallback<void(size_t)> callback) {
  chromeos::U2FClient::IsU2FServiceAvailable(
      base::BindOnce(&PlatformAuthenticatorCredentialStore::DoCountCredentials,
                     weak_factory_.GetWeakPtr(), created_not_before,
                     created_not_after, std::move(callback)));
}

void PlatformAuthenticatorCredentialStore::DoDeleteCredentials(
    base::Time created_not_before,
    base::Time created_not_after,
    base::OnceClosure callback,
    bool is_u2f_service_available) {
  if (!is_u2f_service_available) {
    std::move(callback).Run();
    return;
  }
  u2f::DeleteCredentialsInTimeRangeRequest req;
  req.set_created_not_before_seconds(
      (created_not_before - base::Time::UnixEpoch()).InSeconds());
  req.set_created_not_after_seconds(
      (created_not_after - base::Time::UnixEpoch()).InSeconds());

  chromeos::U2FClient::Get()->DeleteCredentials(
      req,
      base::BindOnce(
          &PlatformAuthenticatorCredentialStore::OnDeleteCredentialsFinished,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PlatformAuthenticatorCredentialStore::DoCountCredentials(
    base::Time created_not_before,
    base::Time created_not_after,
    base::OnceCallback<void(size_t)> callback,
    bool is_u2f_service_available) {
  if (!is_u2f_service_available) {
    std::move(callback).Run(0);
    return;
  }
  u2f::CountCredentialsInTimeRangeRequest req;
  req.set_created_not_before_seconds(
      (created_not_before - base::Time::UnixEpoch()).InSeconds());
  req.set_created_not_after_seconds(
      (created_not_after - base::Time::UnixEpoch()).InSeconds());

  chromeos::U2FClient::Get()->CountCredentials(
      req,
      base::BindOnce(
          &PlatformAuthenticatorCredentialStore::OnCountCredentialsFinished,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PlatformAuthenticatorCredentialStore::OnDeleteCredentialsFinished(
    base::OnceClosure callback,
    std::optional<u2f::DeleteCredentialsInTimeRangeResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "DeleteCredentialsInTimeRange dbus call failed";
  } else if (
      response->status() !=
      u2f::
          DeleteCredentialsInTimeRangeResponse_DeleteCredentialsInTimeRangeStatus_SUCCESS) {
    FIDO_LOG(ERROR) << "DeleteCredentialsInTimeRange failed, status: "
                    << response->status();
  }
  std::move(callback).Run();
}

void PlatformAuthenticatorCredentialStore::OnCountCredentialsFinished(
    base::OnceCallback<void(size_t)> callback,
    std::optional<u2f::CountCredentialsInTimeRangeResponse> response) {
  if (!response) {
    FIDO_LOG(ERROR) << "CountCredentialsInTimeRange dbus call failed";
    std::move(callback).Run(0);
    return;
  }

  if (response->status() !=
      u2f::
          CountCredentialsInTimeRangeResponse_CountCredentialsInTimeRangeStatus_SUCCESS) {
    FIDO_LOG(ERROR) << "CountCredentialsInTimeRange failed, status: "
                    << response->status();
    std::move(callback).Run(0);
    return;
  }
  std::move(callback).Run(response->num_credentials());
}

}  // namespace cros
}  // namespace fido
}  // namespace device
