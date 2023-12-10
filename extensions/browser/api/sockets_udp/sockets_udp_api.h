// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_udp.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace extensions {
class ResumableUDPSocket;
}

namespace extensions {
namespace api {

class UDPSocketEventDispatcher;

class UDPSocketApiFunction : public SocketApiFunction {
 protected:
  ~UDPSocketApiFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableUDPSocket* GetUdpSocket(int socket_id);
};

class UDPSocketExtensionWithDnsLookupFunction
    : public SocketExtensionWithDnsLookupFunction {
 protected:
  ~UDPSocketExtensionWithDnsLookupFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableUDPSocket* GetUdpSocket(int socket_id);
};

class SocketsUdpCreateFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.create", SOCKETS_UDP_CREATE)

  SocketsUdpCreateFunction();

 protected:
  ~SocketsUdpCreateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsUdpUnitTest, Create);
};

class SocketsUdpUpdateFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.update", SOCKETS_UDP_UPDATE)

  SocketsUdpUpdateFunction();

 protected:
  ~SocketsUdpUpdateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsUdpSetPausedFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setPaused", SOCKETS_UDP_SETPAUSED)

  SocketsUdpSetPausedFunction();

 protected:
  ~SocketsUdpSetPausedFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsUdpBindFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.bind", SOCKETS_UDP_BIND)

  SocketsUdpBindFunction();

 protected:
  ~SocketsUdpBindFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
  void OnCompleted(int net_result);

 private:
  std::optional<sockets_udp::Bind::Params> params_;
  raw_ptr<UDPSocketEventDispatcher> socket_event_dispatcher_ = nullptr;
};

class SocketsUdpSendFunction : public UDPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.send", SOCKETS_UDP_SEND)

  SocketsUdpSendFunction();

 protected:
  ~SocketsUdpSendFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartSendTo();

  std::optional<sockets_udp::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_ = 0;
};

class SocketsUdpCloseFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.close", SOCKETS_UDP_CLOSE)

  SocketsUdpCloseFunction();

 protected:
  ~SocketsUdpCloseFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsUdpGetInfoFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getInfo", SOCKETS_UDP_GETINFO)

  SocketsUdpGetInfoFunction();

 protected:
  ~SocketsUdpGetInfoFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsUdpGetSocketsFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getSockets", SOCKETS_UDP_GETSOCKETS)

  SocketsUdpGetSocketsFunction();

 protected:
  ~SocketsUdpGetSocketsFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsUdpJoinGroupFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.joinGroup", SOCKETS_UDP_JOINGROUP)

  SocketsUdpJoinGroupFunction();

 protected:
  ~SocketsUdpJoinGroupFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;

 private:
  void OnCompleted(int result);
};

class SocketsUdpLeaveGroupFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.leaveGroup", SOCKETS_UDP_LEAVEGROUP)

  SocketsUdpLeaveGroupFunction();

 protected:
  ~SocketsUdpLeaveGroupFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;

 private:
  void OnCompleted(int result);
};

class SocketsUdpSetMulticastTimeToLiveFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastTimeToLive",
                             SOCKETS_UDP_SETMULTICASTTIMETOLIVE)

  SocketsUdpSetMulticastTimeToLiveFunction();

 protected:
  ~SocketsUdpSetMulticastTimeToLiveFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsUdpSetMulticastLoopbackModeFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastLoopbackMode",
                             SOCKETS_UDP_SETMULTICASTLOOPBACKMODE)

  SocketsUdpSetMulticastLoopbackModeFunction();

 protected:
  ~SocketsUdpSetMulticastLoopbackModeFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsUdpGetJoinedGroupsFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getJoinedGroups",
                             SOCKETS_UDP_GETJOINEDGROUPS)

  SocketsUdpGetJoinedGroupsFunction();

 protected:
  ~SocketsUdpGetJoinedGroupsFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsUdpSetBroadcastFunction : public UDPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setBroadcast",
                             SOCKETS_UDP_SETBROADCAST)

  SocketsUdpSetBroadcastFunction();

 protected:
  ~SocketsUdpSetBroadcastFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;

 private:
  void OnCompleted(int net_result);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
