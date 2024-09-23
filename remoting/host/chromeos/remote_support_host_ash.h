// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_
#define REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/chromeos/session_id.h"
#include "remoting/host/it2me/it2me_native_messaging_host_ash.h"
#include "remoting/host/mojom/remote_support.mojom-forward.h"

namespace remoting {

class BrowserInterop;
class It2MeHostFactory;
class It2MeNativeMessageHostAsh;
class SessionStorage;
struct ChromeOsEnterpriseParams;

// The identifier used for the enterprise reconnectable session.
// Its value is randomly chosen but fixed for now.
constexpr SessionId kEnterpriseSessionId = SessionId{125687};

// This class represents a remote support host instance which can be connected
// to and controlled over an IPC channel. It wraps a remote support host
// implementation and enforces the single connection requirement. Method calls
// and destruction must occur on the same sequence as creation. This object's
// lifetime is tied to that of the wrapped host, meaning this instance will be
// destroyed when the IPC channel is disconnected.
class RemoteSupportHostAsh {
 public:
  RemoteSupportHostAsh(base::OnceClosure cleanup_callback,
                       SessionStorage& session_storage);
  RemoteSupportHostAsh(std::unique_ptr<It2MeHostFactory> host_factory,
                       scoped_refptr<BrowserInterop> browser_interop,
                       SessionStorage& session_storage,
                       base::OnceClosure cleanup_callback);
  RemoteSupportHostAsh(const RemoteSupportHostAsh&) = delete;
  RemoteSupportHostAsh& operator=(const RemoteSupportHostAsh&) = delete;
  ~RemoteSupportHostAsh();

  using StartSessionCallback =
      base::OnceCallback<void(mojom::StartSupportSessionResponsePtr response)>;

  // Returns a structure which includes the host version and supported features.
  static mojom::SupportHostDetailsPtr GetHostDetails();

  // Allows the caller to start a new remote support session. `callback` is
  // called with the result.
  void StartSession(
      const mojom::SupportSessionParams& params,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      StartSessionCallback callback);

  // Allows the caller to resume the given remote support session.
  // `callback` is called with the result.
  void ReconnectToSession(SessionId session_id,
                          const std::string& access_token,
                          StartSessionCallback callback);

 private:
  void StartSession(
      const mojom::SupportSessionParams& params,
      const std::optional<ChromeOsEnterpriseParams>& enterprise_params,
      const std::optional<ReconnectParams>& reconnect_params,
      StartSessionCallback callback);

  void OnHostStateConnected(mojom::SupportSessionParams,
                            std::optional<ChromeOsEnterpriseParams>,
                            std::optional<ReconnectParams>);
  void OnHostStateDisconnected();
  void OnSessionDisconnected();

  void OnSessionRetrieved(SessionId session_id,
                          const std::string& access_token,
                          StartSessionCallback callback,
                          std::optional<base::Value::Dict> session);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<It2MeHostFactory> host_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<It2MeNativeMessageHostAsh> it2me_native_message_host_ash_
      GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<BrowserInterop> browser_interop_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ref<SessionStorage> session_storage_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure cleanup_callback_;

  base::WeakPtrFactory<RemoteSupportHostAsh> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_REMOTE_SUPPORT_HOST_ASH_H_
