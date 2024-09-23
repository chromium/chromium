// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_HOST_STARTER_BASE_H_
#define REMOTING_HOST_SETUP_HOST_STARTER_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_oauth_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

class HostStarterOAuthHelper;
class RsaKeyPair;

// Base class used to provide common functionality needed when registering a
// new remote access host instance in the CRD backend. Subclasses should
// override the methods which handle the steps which differ however the flow
// is generally the same for all workflows.
//
// Overview of the steps in the registration flow once StartHost() is called:
// 1.) Check for an existing host instance
// 2.) <optional> Exchange authorization_code for an access token
// 3.) Register the new host instance in the CRD backend
// 4.) Exchange the service account authorization_code for a refresh token
// 5.) Remove the existing host from the CRD backend
// 6.) Stop the existing host (if one is running)
// 7.) Write the host configuration file to disk
// 8.) Start the host service using the new configuration file
//
class HostStarterBase : public HostStarter {
 public:
  explicit HostStarterBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  HostStarterBase(const HostStarterBase&) = delete;
  HostStarterBase& operator=(const HostStarterBase&) = delete;
  ~HostStarterBase() override;

  // HostStarter implementation.
  void StartHost(Params params, CompletionCallback on_done) override;

 protected:
  Params& params() { return start_host_params_; }
  std::optional<std::string>& existing_host_id() { return existing_host_id_; }

  // Methods used to implement the registration process described in the class
  // comment. They are listed in the order in which they are called.
  void OnExistingConfigLoaded(std::optional<base::Value::Dict> config);
  void OnUserTokensRetrieved(const std::string& user_email,
                             const std::string& access_token,
                             const std::string& refresh_token,
                             const std::string& scopes);
  virtual void RegisterNewHost(const std::string& public_key,
                               std::optional<std::string> access_token) = 0;
  void OnNewHostRegistered(const std::string& directory_id,
                           const std::string& owner_account_email,
                           const std::string& service_account_email,
                           const std::string& authorization_code);
  void OnServiceAccountTokensRetrieved(const std::string& service_account_email,
                                       const std::string& access_token,
                                       const std::string& refresh_token,
                                       const std::string& scopes);
  virtual void RemoveOldHostFromDirectory(
      base::OnceClosure on_host_removed) = 0;
  void StopOldHost();
  void OnOldHostStopped(DaemonController::AsyncResult result);
  void GenerateConfigFile();
  virtual void ApplyConfigValues(base::Value::Dict& config) = 0;
  void OnNewHostStarted(DaemonController::AsyncResult result);

  // |HandleError| will cause |on_done_| to be executed.
  void HandleError(const std::string& error_message, Result error_result);
  // Converts |status| into a HostStarter error and logs error information.
  void HandleHttpStatusError(const ProtobufHttpStatus& status);
  // Overiddable to allow for reporting errors to a service backend.
  // |on_error_reported| will be run whether the error is reported successfully
  // or not.
  virtual void ReportError(const std::string& error_message,
                           base::OnceClosure on_error_reported);

  void SetDaemonControllerForTest(
      scoped_refptr<DaemonController> daemon_controller);

 private:
  Params start_host_params_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::optional<std::string> existing_host_id_;

  std::string service_account_email_;
  std::string service_account_refresh_token_;

  std::optional<HostStarterOAuthHelper> oauth_helper_;

  scoped_refptr<DaemonController> daemon_controller_ =
      DaemonController::Create();

  CompletionCallback on_done_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<HostStarterBase> weak_ptr_;
  base::WeakPtrFactory<HostStarterBase> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_HOST_STARTER_BASE_H_
