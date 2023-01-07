// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_FACTORY_H_
#define GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "google_apis/gcm/engine/connection_factory.h"

namespace gcm {

class FakeConnectionHandler;

// A connection factory that mocks out real connections, using a fake connection
// handler instead.
class FakeConnectionFactory : public ConnectionFactory {
 public:
  FakeConnectionFactory();

  FakeConnectionFactory(const FakeConnectionFactory&) = delete;
  FakeConnectionFactory& operator=(const FakeConnectionFactory&) = delete;

  ~FakeConnectionFactory() override;

  // ConnectionFactory implementation.
  void Initialize(
      const BuildLoginRequestCallback& request_builder,
      const ConnectionHandler::ProtoReceivedCallback& read_callback,
      const ConnectionHandler::ProtoSentCallback& write_callback) override;
  ConnectionHandler* GetConnectionHandler() const override;
  void Connect() override;
  bool IsEndpointReachable() const override;
  std::string GetConnectionStateString() const override;
  base::TimeTicks NextRetryAttempt() const override;
  void SignalConnectionReset(ConnectionResetReason reason) override;
  void SetConnectionListener(ConnectionListener* listener) override;

  // Whether a connection reset has been triggered and is yet to run.
  bool reconnect_pending() const { return reconnect_pending_; }

  // Whether connection resets should be handled immediately or delayed until
  // release.
  void set_delay_reconnect(bool should_delay) {
    delay_reconnect_ = should_delay;
  }

 private:
  std::unique_ptr<FakeConnectionHandler> connection_handler_;

  BuildLoginRequestCallback request_builder_;

  // Logic for handling connection resets.
  bool reconnect_pending_;
  bool delay_reconnect_;

  raw_ptr<ConnectionListener> connection_listener_;
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_FAKE_CONNECTION_FACTORY_H_
