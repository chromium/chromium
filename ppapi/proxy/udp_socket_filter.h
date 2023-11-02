// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_UDP_SOCKET_FILTER_H_
#define PPAPI_PROXY_UDP_SOCKET_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/ppb_udp_socket.h"
#include "ppapi/c/private/ppb_net_address_private.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_filter.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

class ResourceMessageReplyParams;

// Handles receiving UDP packets on the IO thread so that when the recipient is
// not on the main thread, we can post directly to the appropriate thread.
class PPAPI_PROXY_EXPORT UDPSocketFilter : public ResourceMessageFilter {
 public:
  UDPSocketFilter();

  // All these are called on whatever thread the plugin wants, while already
  // holding the ppapi::ProxyLock. The "slot_available_callback" will be invoked
  // whenever we detect that a slot is now available, so that the client can
  // take appropriate action (like informing the host we can receive another
  // buffer). It will always be run with the ProxyLock.
  void AddUDPResource(PP_Instance instance,
                      PP_Resource resource,
                      bool private_api,
                      base::RepeatingClosure slot_available_callback);
  void RemoveUDPResource(PP_Resource resource);
  // Note, the slot_available_callback that was provided to AddUDPResource may
  // be invoked during the RequestData call; this gives the client the
  // opportunity to post a message to the host immediately.
  int32_t RequestData(PP_Resource resource,
                      int32_t num_bytes,
                      char* buffer,
                      PP_Resource* addr,
                      const scoped_refptr<TrackedCallback>& callback);

  // ResourceMessageFilter implementation.
  bool OnResourceReplyReceived(const ResourceMessageReplyParams& reply_params,
                               const IPC::Message& nested_msg) override;

  PP_NetAddress_Private GetLastAddrPrivate(PP_Resource resource) const;

  // The maximum number of bytes that each
  // PpapiPluginMsg_PPBUDPSocket_PushRecvResult message is allowed to carry.
  static const int32_t kMaxReadSize;
  // The maximum number that we allow for setting
  // PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  static const int32_t kMaxReceiveBufferSize;
  // The maximum number of received packets that we allow instances of this
  // class to buffer.
  static const size_t kPluginReceiveBufferSlots;

 private:
  // The queue of received data intended for 1 UDPSocketResourceBase. All usage
  // must be protected by UDPSocketFilter::lock_.
  class RecvQueue {
   public:
    explicit RecvQueue(PP_Instance instance,
                       bool private_api,
                       base::RepeatingClosure slot_available_callback);
    ~RecvQueue();

    // Called on the IO thread when data is received. It will post |callback_|
    // if it's valid, otherwise push the data on buffers_.
    // The ppapi::ProxyLock should *not* be held, and won't be acquired.
    void DataReceivedOnIOThread(int32_t result,
                                const std::string& d,
                                const PP_NetAddress_Private& addr);
    // Called on whatever thread the plugin chooses. Must already hold the
    // PpapiProxyLock. Returns a code from pp_errors.h, or a positive number.
    //
    // Note, the out-params are owned by the plugin, and if the request can't be
    // handled immediately, they will be written later just before the callback
    // is invoked.
    int32_t RequestData(int32_t num_bytes,
                        char* buffer_out,
                        PP_Resource* addr_out,
                        const scoped_refptr<TrackedCallback>& callback);
    PP_NetAddress_Private GetLastAddrPrivate() const;

   private:
    struct RecvBuffer {
      int32_t result;
      std::string data;
      PP_NetAddress_Private addr;
    };
    base::queue<RecvBuffer> recv_buffers_;

    PP_Instance pp_instance_;
    scoped_refptr<ppapi::TrackedCallback> recvfrom_callback_;
    char* read_buffer_;
    int32_t bytes_to_read_;
    PP_Resource* recvfrom_addr_resource_;
    PP_NetAddress_Private last_recvfrom_addr_;
    bool private_api_;
    // Callback to invoke when a UDP receive slot is available.
    base::RepeatingClosure slot_available_callback_;
  };

 private:
  // This is deleted via RefCountedThreadSafe (see ResourceMessageFilter).
  ~UDPSocketFilter();
  void OnPluginMsgPushRecvResult(const ResourceMessageReplyParams& params,
                                 int32_t result,
                                 const std::string& data,
                                 const PP_NetAddress_Private& addr);

  // lock_ protects queues_.
  //
  // Lock order (if >1 acquired):
  // 1 ppapi::ProxyLock
  // \-->2 Filter lock_
  mutable base::Lock lock_;
  std::unordered_map<PP_Resource, std::unique_ptr<RecvQueue>> queues_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_UDP_SOCKET_FILTER_H_
