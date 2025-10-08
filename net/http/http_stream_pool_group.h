// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_GROUP_H_
#define NET_HTTP_HTTP_STREAM_POOL_GROUP_H_

#include <list>
#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_request.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/stream_socket_handle.h"
#include "net/spdy/spdy_session_key.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"

namespace net {

class HttpNetworkSession;
class HttpStream;
class HttpStreamPoolHandle;
class StreamSocket;

// Maintains active/idle text-based HTTP streams. If new streams are needed,
// creates an HttpStreamPool::AttemptManager and starts connection attempts for
// streams.
//
// When an active AttemptManager starts shutting down (e.g. the AttemptManager
// fails), creates a new AttemptManager for subsequent stream requests (Jobs).
// AttemptManagers need to outlive all associated Jobs. Keeps shutting down
// AttemptManager(s) until these are ready to destroy.
//
// Owned by an HttpStreamPool, keyed by HttpStreamKey. Destroyed when all
// streams associated with this group are completed.
class HttpStreamPool::Group {
 public:
  // The same timeout as ClientSocketPool::used_idle_socket_timeout().
  static constexpr base::TimeDelta kUsedIdleStreamSocketTimeout =
      base::Seconds(300);

  // The same timeout as
  // ClientSocketPoolManager::unused_idle_socket_timeout().
  static constexpr base::TimeDelta kUnusedIdleStreamSocketTimeout =
      base::Seconds(60);

  Group(HttpStreamPool* pool,
        HttpStreamKey stream_key,
        std::optional<QuicSessionAliasKey> quic_session_alias_key);

  Group(const Group&) = delete;
  Group& operator=(const Group&) = delete;

  ~Group();

  const HttpStreamKey& stream_key() const { return stream_key_; }

  const SpdySessionKey& spdy_session_key() const { return spdy_session_key_; }

  const QuicSessionAliasKey& quic_session_alias_key() const {
    return quic_session_alias_key_;
  }

  HttpStreamPool* pool() { return pool_; }
  const HttpStreamPool* pool() const { return pool_; }

  HttpNetworkSession* http_network_session() const {
    return pool_->http_network_session();
  }

  // TODO(crbug.com/416088643): Rename to `active_attempt_manager()`.
  AttemptManager* attempt_manager() const { return attempt_manager_.get(); }

  size_t ShuttingDownAttemptManagerCount() const {
    return shutting_down_attempt_managers_.size();
  }

  const NetLogWithSource& net_log() { return net_log_; }

  bool force_quic() const { return force_quic_; }

  const perfetto::Track& track() const { return track_; }
  const perfetto::Flow& flow() const { return flow_; }

  // Creates a Job to attempt connection(s). We have separate methods for
  // creating and starting a Job to ensure that the owner of the Job can
  // properly manage the lifetime of the Job, even when StartJob() synchronously
  // calls one of the delegate's methods.
  std::unique_ptr<Job> CreateJob(Job::Delegate* delegate,
                                 quic::ParsedQuicVersion quic_version,
                                 NextProto expected_protocol,
                                 const NetLogWithSource& request_net_log);

  // Called when `job` is going to be destroyed.
  void OnJobComplete(Job* job);

  // Creates an HttpStreamPoolHandle from `socket`. Call sites must ensure that
  // the number of active streams do not exceed the global/per-group limits.
  std::unique_ptr<HttpStreamPoolHandle> CreateHandle(
      std::unique_ptr<StreamSocket> socket,
      StreamSocketHandle::SocketReuseType reuse_type,
      LoadTimingInfo::ConnectTiming connect_timing);

  // Creates a text-based HttpStream from `socket`. Call sites must ensure that
  // the number of active streams do not exceed the global/per-group limits.
  // `socket` must not be negotiated to use HTTP/2.
  std::unique_ptr<HttpStream> CreateTextBasedStream(
      std::unique_ptr<StreamSocket> socket,
      StreamSocketHandle::SocketReuseType reuse_type,
      LoadTimingInfo::ConnectTiming connect_timing);

  // Releases a StreamSocket that was used to create a text-based HttpStream.
  void ReleaseStreamSocket(std::unique_ptr<StreamSocket> socket,
                           int64_t generation);

  // Adds `socket` as an idle StreamSocket for text-based HttpStream. Call sites
  // must ensure that the number of idle streams do not exceed the global/per-
  // group limits.
  // `socket` must not be negotiated to use HTTP/2.
  void AddIdleStreamSocket(std::unique_ptr<StreamSocket> socket);

  // Retrieves an existing idle StreamSocket. Returns nullptr when there is no
  // idle stream.
  std::unique_ptr<StreamSocket> GetIdleStreamSocket();

  // Tries to process a pending request.
  void ProcessPendingRequest();

  // Closes one idle stream socket. Returns true if it closed a stream. Called
  // when the pool reached the stream count limit.
  bool CloseOneIdleStreamSocket();

  // Returns the number of handed out streams.
  size_t HandedOutStreamSocketCount() const { return handed_out_stream_count_; }

  // Returns the number of idle streams.
  size_t IdleStreamSocketCount() const { return idle_stream_sockets_.size(); }

  // Returns the number of connecting streams.
  size_t ConnectingStreamSocketCount() const;

  // Returns the number of active streams.
  size_t ActiveStreamSocketCount() const;

  // True when the number of active streams reached the group limit.
  bool ReachedMaxStreamLimit() const;

  // Returns the highest pending request priority if the group is stalled due to
  // the per-pool limit, not the per-group limit.
  std::optional<RequestPriority> GetPriorityIfStalledByPoolLimit() const;

  // Closes all streams in this group and cancels all pending requests.
  void FlushWithError(int error,
                      StreamSocketCloseReason attempt_cancel_reason,
                      std::string_view net_log_close_reason_utf8);

  // Increments the generation of this group. Closes idle streams. Streams
  // handed out before this increment won't be reused. Cancels in-flight
  // connection attempts.
  void Refresh(std::string_view net_log_close_reason_utf8,
               StreamSocketCloseReason cancel_reason);

  void CloseIdleStreams(std::string_view net_log_close_reason_utf8);

  // Cancels all on-going jobs.
  void CancelJobs(int error, StreamSocketCloseReason cancel_reason);

  // Returns an active AttemptManager for `job`.
  AttemptManager* GetAttemptManagerForJob(Job* job);

  // Called when the active AttemptManager is shutting down.
  void OnAttemptManagerShuttingDown(AttemptManager* attempt_manager);

  // Called when an AttemptManager has completed.
  void OnAttemptManagerComplete(AttemptManager* attempt_manager);

  // Retrieves information on the current state of the group as a base::Value.
  base::Value::Dict GetInfoAsValue() const;

  // Returns true when `this` can be deleted.
  // TODO(crbug.com/346835898): This is public for consistency checks. Make this
  // private once we stabilize the implementation.
  bool CanComplete() const;

  void CleanupTimedoutIdleStreamSocketsForTesting();

 private:
  struct IdleStreamSocket {
    IdleStreamSocket(std::unique_ptr<StreamSocket> stream_socket,
                     base::TimeTicks start_time);
    ~IdleStreamSocket();

    IdleStreamSocket(const IdleStreamSocket&) = delete;
    IdleStreamSocket& operator=(const IdleStreamSocket&) = delete;

    std::unique_ptr<StreamSocket> stream_socket;
    base::TimeTicks time_became_idle;
  };

  enum class CleanupMode {
    // Clean up only timed out idle streams.
    kTimeoutOnly,
    // Clean up all idle streams.
    kForce,
  };

  static base::expected<void, std::string_view> IsIdleStreamSocketUsable(
      const IdleStreamSocket& idle);

  void CleanupIdleStreamSockets(CleanupMode mode,
                                std::string_view net_log_close_reason_utf8);

  // Returns an `AttemptManager` for an Alt-Svc QUIC preconnect job.
  AttemptManager* GetAttemptManagerForAltSvcQuicPreconnect();

  void MaybeComplete();

  // Posts a task to call MaybeComplete() later.
  void MaybeCompleteLater();

  const raw_ptr<HttpStreamPool> pool_;
  const HttpStreamKey stream_key_;
  const SpdySessionKey spdy_session_key_;
  const QuicSessionAliasKey quic_session_alias_key_;
  const NetLogWithSource net_log_;
  const bool force_quic_;
  const perfetto::NamedTrack track_;
  const perfetto::Flow flow_;

  size_t handed_out_stream_count_ = 0;
  int64_t generation_ = 0;
  std::list<IdleStreamSocket> idle_stream_sockets_;

  std::unique_ptr<AttemptManager> attempt_manager_;

  // An `AttemptManager` for Alt-Svc QUIC preconnects.
  std::unique_ptr<AttemptManager> alt_svc_quic_preconnect_attempt_manager_;

  // Keeps AttemptManagers that are shutting down.
  std::set<std::unique_ptr<AttemptManager>, base::UniquePtrComparator>
      shutting_down_attempt_managers_;

  base::WeakPtrFactory<Group> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_GROUP_H_
