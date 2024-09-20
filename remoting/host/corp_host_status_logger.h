// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_HOST_STATUS_LOGGER_H_
#define REMOTING_HOST_CORP_HOST_STATUS_LOGGER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_observer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {
namespace protocol {
class SessionManager;
}  // namespace protocol

class LoggingServiceClient;

// A class that reports host status changes to the corp logging service. This is
// not used for external users. For internal details, see go/crd-corp-logging.
class CorpHostStatusLogger final : public protocol::SessionObserver {
 public:
  CorpHostStatusLogger(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const LocalSessionPoliciesProvider* local_session_policies_provider,
      const std::string& service_account_email,
      const std::string& refresh_token);
  CorpHostStatusLogger(
      std::unique_ptr<LoggingServiceClient> service_client,
      const LocalSessionPoliciesProvider* local_session_policies_provider);
  ~CorpHostStatusLogger() override;
  CorpHostStatusLogger(const CorpHostStatusLogger&) = delete;
  CorpHostStatusLogger& operator=(const CorpHostStatusLogger&) = delete;

  void StartObserving(protocol::SessionManager& session_manager);

 private:
  // protocol::SessionObserver
  void OnSessionStateChange(const protocol::Session& session,
                            protocol::Session::State state) override;

  std::unique_ptr<LoggingServiceClient> service_client_;
  raw_ptr<const LocalSessionPoliciesProvider> local_session_policies_provider_;
  Subscription observer_subscription_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_HOST_STATUS_LOGGER_H_
