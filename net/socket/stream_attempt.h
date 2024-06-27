// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_ATTEMPT_H_
#define NET_SOCKET_STREAM_ATTEMPT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

class ClientSocketFactory;
class SocketPerformanceWatcherFactory;
class StreamSocket;
class NetworkQualityEstimator;
class NetLog;

// Common parameters for StreamAttempt classes.
struct NET_EXPORT_PRIVATE StreamAttemptParams {
  StreamAttemptParams(
      ClientSocketFactory* client_socket_factory,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log);

  raw_ptr<ClientSocketFactory> client_socket_factory;
  raw_ptr<SocketPerformanceWatcherFactory> socket_performance_watcher_factory;
  raw_ptr<NetworkQualityEstimator> network_quality_estimator;
  raw_ptr<NetLog> net_log;
};

// Represents a connection attempt to a single IP endpoint.
class NET_EXPORT_PRIVATE StreamAttempt {
 public:
  // `params` must outlive `this`.
  StreamAttempt(const StreamAttemptParams* params,
                IPEndPoint ip_endpoint,
                NetLogSourceType net_log_source_type,
                NetLogEventType net_log_attempt_event_type,
                NetLogWithSource* net_log = nullptr);

  StreamAttempt(const StreamAttempt&) = delete;
  StreamAttempt& operator=(const StreamAttempt&) = delete;

  virtual ~StreamAttempt();

  // Starts this connection attempt. When ERR_IO_PENDING is returned, the
  // attempt completed synchronously and `callback` is never invoked. Otherwise,
  // `callback` is invoked when the attempt completes.
  int Start(CompletionOnceCallback callback);

  std::unique_ptr<StreamSocket> ReleaseStreamSocket();

  const IPEndPoint& ip_endpoint() const { return ip_endpoint_; }

  const NetLogWithSource& net_log() const { return net_log_; }

  // Returns the start time of this attempt. Should only be accessed after the
  // attempt started.
  base::TimeTicks start_time() const { return start_time_; }

  // Returns the end time of this attempt. Should only be accessed after the
  // attempt completed.
  base::TimeTicks end_time() const { return end_time_; }

 protected:
  virtual int StartInternal() = 0;

  const StreamAttemptParams& params() { return *params_; }

  void SetStreamSocket(std::unique_ptr<StreamSocket> stream_socket);

  // Called by subclasses to notify the completion of this attempt. `this` may
  // be deleted after calling this method.
  void NotifyOfCompletion(int rv);

 private:
  void LogCompletion(int rv);

  const raw_ptr<const StreamAttemptParams> params_;
  const IPEndPoint ip_endpoint_;

  NetLogWithSource net_log_;
  NetLogEventType net_log_attempt_event_type_;

  // `callback_` is consumed when the attempt completes.
  CompletionOnceCallback callback_;

  std::unique_ptr<StreamSocket> stream_socket_;

  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_ATTEMPT_H_
