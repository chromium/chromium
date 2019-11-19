// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/api/display_source.h"
#include "net/base/ip_address.h"

namespace extensions {

using DisplaySourceSinkInfo = api::display_source::SinkInfo;
using DisplaySourceSinkInfoList = std::vector<DisplaySourceSinkInfo>;
using DisplaySourceAuthInfo = api::display_source::AuthenticationInfo;
using DisplaySourceErrorType = api::display_source::ErrorType;
// The DisplaySourceConnectionDelegate interface should be implemented
// to provide sinks search and connection functionality for
// 'chrome.displaySource' API.
class DisplaySourceConnectionDelegate : public KeyedService {
 public:
  using AuthInfoCallback = base::Callback<void(const DisplaySourceAuthInfo&)>;
  using StringCallback = base::Callback<void(const std::string&)>;
  using SinkInfoListCallback =
      base::Callback<void(const DisplaySourceSinkInfoList&)>;

  const static int kInvalidSinkId = -1;

  class Connection {
   public:
    // Returns the connected sink object.
    virtual const DisplaySourceSinkInfo& GetConnectedSink() const = 0;

    // Returns the local address of the source.
    virtual net::IPAddress GetLocalAddress() const = 0;

    // Returns the address of the connected sink.
    virtual net::IPAddress GetSinkAddress() const = 0;

    // Sends a control message to the connected sink.
    // If an error occurs 'Observer::OnConnectionError' is invoked.
    virtual void SendMessage(const std::string& message) = 0;

    // Sets a callback to receive control messages from the connected sink.
    // This method should only be called once in the lifetime of each
    // Connection object.
    // If an error occurs 'Observer::OnConnectionError' is invoked.
    virtual void SetMessageReceivedCallback(
        const StringCallback& callback) = 0;

   protected:
    Connection();
    virtual ~Connection();
  };

  class Observer {
   public:
    // This method is called each time the list of available
    // sinks is updated whether after 'GetAvailableSinks' call
    // or while the implementation is constantly watching the
    // available sinks (after 'StartWatchingAvailableSinks' was called).
    // Also this method is called to reflect current connection updates.
    virtual void OnSinksUpdated(const DisplaySourceSinkInfoList& sinks) = 0;

    // This method is called during the established connection to report
    // a transport layer fatal error (which implies that the connection
    // becomes broken/disconnected).
    virtual void OnConnectionError(int sink_id,
                                   DisplaySourceErrorType type,
                                   const std::string& description) = 0;

   protected:
    virtual ~Observer() {}
  };

  DisplaySourceConnectionDelegate();
  ~DisplaySourceConnectionDelegate() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Returns the list of last found available sinks
  // this list may contain outdated data if the delegate
  // is not watching the sinks (via 'StartWatchingSinks'
  // method). The list is refreshed after 'GetAvailableSinks'
  // call.
  virtual const DisplaySourceSinkInfoList& last_found_sinks() const = 0;

  // Returns the Connection object representing the current
  // connection to the sink or NULL if there is no current connection.
  virtual Connection* connection() = 0;

  // Queries the list of currently available sinks.
  virtual void GetAvailableSinks(const SinkInfoListCallback& sinks_callback,
                                 const StringCallback& failure_callback) = 0;

  // Queries the authentication method required by the sink for connection.
  // If the used authentication method requires authentication data to be
  // visible on the sink's display (e.g. PIN) the implementation should
  // request the sink to show it.
  virtual void RequestAuthentication(
      int sink_id,
      const AuthInfoCallback& auth_info_callback,
      const StringCallback& failure_callback) = 0;

  // Connects to a sink by given id and auth info.
  virtual void Connect(int sink_id,
                       const DisplaySourceAuthInfo& auth_info,
                       const StringCallback& failure_callback) = 0;

  // Disconnects the current connection to sink, the 'failure_callback'
  // is called if an error has occurred or if there is no established
  // connection.
  virtual void Disconnect(const StringCallback& failure_callback) = 0;

  // Implementation should start watching the available sinks updates.
  virtual void StartWatchingAvailableSinks() = 0;

  // Implementation should stop watching the available sinks updates.
  virtual void StopWatchingAvailableSinks() = 0;

 protected:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_SOURCE_DISPLAY_SOURCE_CONNECTION_DELEGATE_H_
