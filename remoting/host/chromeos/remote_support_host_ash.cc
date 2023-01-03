// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remote_support_host_ash.h"

#include <utility>

#include <stddef.h>

#include "base/notreached.h"
#include "base/strings/stringize_macros.h"
#include "remoting/host/chromeos/remoting_service.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/it2me/it2me_constants.h"
#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"
#include "remoting/host/policy_watcher.h"

namespace remoting {

RemoteSupportHostAsh::RemoteSupportHostAsh(base::OnceClosure cleanup_callback)
    : cleanup_callback_(std::move(cleanup_callback)) {}

RemoteSupportHostAsh::~RemoteSupportHostAsh() = default;

void RemoteSupportHostAsh::StartSession(
    mojom::SupportSessionParamsPtr params,
    const absl::optional<ChromeOsEnterpriseParams>& enterprise_params,
    StartSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Ensure there is at most one active remote support connection.
  // Since we are initiating the disconnect, don't run the cleanup callback.
  if (it2me_native_message_host_ash_) {
    auto temp = std::move(it2me_native_message_host_ash_);
    temp->Disconnect();
  }

  it2me_native_message_host_ash_ =
      std::make_unique<It2MeNativeMessageHostAsh>();

  mojo::PendingReceiver<mojom::SupportHostObserver> pending_receiver =
      it2me_native_message_host_ash_->Start(
          RemotingService::Get().CreateHostContext(),
          RemotingService::Get().CreatePolicyWatcher());

  mojom::StartSupportSessionResponsePtr response =
      mojom::StartSupportSessionResponse::NewObserver(
          std::move(pending_receiver));

  it2me_native_message_host_ash_->Connect(
      std::move(params), enterprise_params,
      base::BindOnce(std::move(callback), std::move(response)),
      base::BindOnce(&RemoteSupportHostAsh::OnSessionDisconnected,
                     base::Unretained(this)));
}

// static
mojom::SupportHostDetailsPtr RemoteSupportHostAsh::GetHostDetails() {
  return mojom::SupportHostDetails::New(
      STRINGIZE(VERSION), std::vector<std::string>({kFeatureAccessTokenAuth,
                                                    kFeatureAuthorizedHelper}));
}

void RemoteSupportHostAsh::OnSessionDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (it2me_native_message_host_ash_) {
    // Do not access any instance members after |cleanup_callback_| is run as
    // this instance will be destroyed by running this.
    std::move(cleanup_callback_).Run();
  }
}

}  // namespace remoting
