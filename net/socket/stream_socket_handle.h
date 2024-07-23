// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_STREAM_SOCKET_HANDLE_H_
#define NET_SOCKET_STREAM_SOCKET_HANDLE_H_

#include <stdint.h>

#include <memory>

#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"

namespace net {

class StreamSocket;
class HigherLayeredPool;

// A base class for handles that contain a StreamSocket. A subclass may have a
// concept of initialization, where a handle needs to be initialized before it
// can be used. A handle can be deinitialized by calling Reset().
class NET_EXPORT_PRIVATE StreamSocketHandle {
 public:
  enum class SocketReuseType {
    kUnused = 0,  // unused socket that just finished connecting
    kUnusedIdle,  // unused socket that has been idle for awhile
    kReusedIdle,  // previously used socket
    kNumTypes,
  };

  StreamSocketHandle();

  StreamSocketHandle(const StreamSocketHandle&) = delete;
  StreamSocketHandle& operator=(const StreamSocketHandle&) = delete;

  virtual ~StreamSocketHandle();

  // Returns true when `this` is initialized successfully.
  bool is_initialized() const { return is_initialized_; }

  StreamSocket* socket() { return socket_.get(); }
  StreamSocket* socket() const { return socket_.get(); }

  // Sets `socket` to `this`.
  void SetSocket(std::unique_ptr<StreamSocket> socket);

  // Releases the ownership of `socket`.
  std::unique_ptr<StreamSocket> PassSocket();

  // Sets the portion of LoadTimingInfo related to connection establishment, and
  // the socket id.  `is_reused` is needed because the handle may not have full
  // reuse information.  `load_timing_info` must have all default values when
  // called. Returns false and makes no changes to `load_timing_info` when
  // `socket_` is nullptr.
  bool GetLoadTimingInfo(bool is_reused,
                         LoadTimingInfo* load_timing_info) const;

  SocketReuseType reuse_type() const { return reuse_type_; }
  void set_reuse_type(SocketReuseType reuse_type) { reuse_type_ = reuse_type; }

  const LoadTimingInfo::ConnectTiming& connect_timing() const {
    return connect_timing_;
  }
  void set_connect_timing(const LoadTimingInfo::ConnectTiming& connect_timing) {
    connect_timing_ = connect_timing;
  }

  // If this handle is associated with a pool that has the concept of higher
  // layered pools, adds/removes a higher layered pool to the pool. Otherwise,
  // does nothing.
  virtual void AddHigherLayeredPool(HigherLayeredPool* higher_pool);
  virtual void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool);

  // Releases the underlying socket and uninitializes `this`.
  virtual void Reset() = 0;

  // Returns true if the pool that is associated with this handle is stalled.
  virtual bool IsPoolStalled() const = 0;

 protected:
  void set_is_initialized(bool is_initialized) {
    is_initialized_ = is_initialized;
  }

 private:
  bool is_initialized_ = false;
  std::unique_ptr<StreamSocket> socket_;
  SocketReuseType reuse_type_ = SocketReuseType::kUnused;

  // Timing information is set when a connection is successfully established.
  LoadTimingInfo::ConnectTiming connect_timing_;
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_HANDLE_H_
