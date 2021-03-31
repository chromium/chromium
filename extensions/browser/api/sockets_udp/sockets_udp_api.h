// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_

#include <stddef.h>

#include "base/gtest_prod_util.h"
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

class UDPSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  ~UDPSocketAsyncApiFunction() override;

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

class SocketsUdpCreateFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.create", SOCKETS_UDP_CREATE)

  SocketsUdpCreateFunction();

 protected:
  ~SocketsUdpCreateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsUdpUnitTest, Create);

  mojo::PendingRemote<network::mojom::UDPSocket> socket_;
  mojo::PendingReceiver<network::mojom::UDPSocketListener>
      socket_listener_receiver_;
  std::unique_ptr<sockets_udp::Create::Params> params_;
};

class SocketsUdpUpdateFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.update", SOCKETS_UDP_UPDATE)

  SocketsUdpUpdateFunction();

 protected:
  ~SocketsUdpUpdateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::Update::Params> params_;
};

class SocketsUdpSetPausedFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setPaused", SOCKETS_UDP_SETPAUSED)

  SocketsUdpSetPausedFunction();

 protected:
  ~SocketsUdpSetPausedFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::SetPaused::Params> params_;
  UDPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsUdpBindFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.bind", SOCKETS_UDP_BIND)

  SocketsUdpBindFunction();

 protected:
  ~SocketsUdpBindFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int net_result);

 private:
  std::unique_ptr<sockets_udp::Bind::Params> params_;
  UDPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsUdpSendFunction : public UDPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.send", SOCKETS_UDP_SEND)

  SocketsUdpSendFunction();

 protected:
  ~SocketsUdpSendFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartSendTo();

  std::unique_ptr<sockets_udp::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketsUdpCloseFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.close", SOCKETS_UDP_CLOSE)

  SocketsUdpCloseFunction();

 protected:
  ~SocketsUdpCloseFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::Close::Params> params_;
};

class SocketsUdpGetInfoFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getInfo", SOCKETS_UDP_GETINFO)

  SocketsUdpGetInfoFunction();

 protected:
  ~SocketsUdpGetInfoFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::GetInfo::Params> params_;
};

class SocketsUdpGetSocketsFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getSockets", SOCKETS_UDP_GETSOCKETS)

  SocketsUdpGetSocketsFunction();

 protected:
  ~SocketsUdpGetSocketsFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;
};

class SocketsUdpJoinGroupFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.joinGroup", SOCKETS_UDP_JOINGROUP)

  SocketsUdpJoinGroupFunction();

 protected:
  ~SocketsUdpJoinGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result);

  std::unique_ptr<sockets_udp::JoinGroup::Params> params_;
};

class SocketsUdpLeaveGroupFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.leaveGroup", SOCKETS_UDP_LEAVEGROUP)

  SocketsUdpLeaveGroupFunction();

 protected:
  ~SocketsUdpLeaveGroupFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result);

  std::unique_ptr<sockets_udp::LeaveGroup::Params> params_;
};

class SocketsUdpSetMulticastTimeToLiveFunction
    : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastTimeToLive",
                             SOCKETS_UDP_SETMULTICASTTIMETOLIVE)

  SocketsUdpSetMulticastTimeToLiveFunction();

 protected:
  ~SocketsUdpSetMulticastTimeToLiveFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::SetMulticastTimeToLive::Params> params_;
};

class SocketsUdpSetMulticastLoopbackModeFunction
    : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setMulticastLoopbackMode",
                             SOCKETS_UDP_SETMULTICASTLOOPBACKMODE)

  SocketsUdpSetMulticastLoopbackModeFunction();

 protected:
  ~SocketsUdpSetMulticastLoopbackModeFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::SetMulticastLoopbackMode::Params> params_;
};

class SocketsUdpGetJoinedGroupsFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.getJoinedGroups",
                             SOCKETS_UDP_GETJOINEDGROUPS)

  SocketsUdpGetJoinedGroupsFunction();

 protected:
  ~SocketsUdpGetJoinedGroupsFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_udp::GetJoinedGroups::Params> params_;
};

class SocketsUdpSetBroadcastFunction : public UDPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.udp.setBroadcast",
                             SOCKETS_UDP_SETBROADCAST)

  SocketsUdpSetBroadcastFunction();

 protected:
  ~SocketsUdpSetBroadcastFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int net_result);

  std::unique_ptr<sockets_udp::SetBroadcast::Params> params_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_UDP_SOCKETS_UDP_API_H_
