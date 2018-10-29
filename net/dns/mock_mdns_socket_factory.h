// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
#define NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/dns/mdns_client_impl.h"
#include "net/log/net_log_with_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

class IPAddress;

class MockMDnsDatagramServerSocket : public DatagramServerSocket {
 public:
  explicit MockMDnsDatagramServerSocket(AddressFamily address_family);
  ~MockMDnsDatagramServerSocket() override;

  // DatagramServerSocket implementation:
  MOCK_METHOD1(Listen, int(const IPEndPoint& address));

  // GMock cannot handle move-only types like CompletionOnceCallback, so it
  // needs to be converted into the copyable type CompletionRepeatingCallback.
  int RecvFrom(IOBuffer* buffer,
               int size,
               IPEndPoint* address,
               CompletionOnceCallback callback) override;

  MOCK_METHOD4(RecvFromInternal,
               int(IOBuffer* buffer,
                   int size,
                   IPEndPoint* address,
                   CompletionRepeatingCallback callback));

  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback) override;

  MOCK_METHOD3(SendToInternal,
               int(const std::string& packet,
                   const std::string address,
                   CompletionRepeatingCallback callback));

  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t size));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t size));
  MOCK_METHOD0(SetDoNotFragment, int());
  MOCK_METHOD1(SetMsgConfirm, void(bool confirm));

  MOCK_METHOD0(Close, void());

  MOCK_CONST_METHOD1(GetPeerAddress, int(IPEndPoint* address));
  int GetLocalAddress(IPEndPoint* address) const override;
  MOCK_METHOD0(UseNonBlockingIO, void());
  MOCK_METHOD0(UseWriteBatching, void());
  MOCK_METHOD0(UseMultiCore, void());
  MOCK_METHOD0(UseSendmmsg, void());
  MOCK_CONST_METHOD0(NetLog, const NetLogWithSource&());

  MOCK_METHOD0(AllowAddressReuse, void());
  MOCK_METHOD0(AllowBroadcast, void());
  MOCK_METHOD0(AllowAddressSharingForMulticast, void());

  MOCK_CONST_METHOD1(JoinGroup, int(const IPAddress& group_address));
  MOCK_CONST_METHOD1(LeaveGroup, int(const IPAddress& address));

  MOCK_METHOD1(SetMulticastInterface, int(uint32_t interface_index));
  MOCK_METHOD1(SetMulticastTimeToLive, int(int ttl));
  MOCK_METHOD1(SetMulticastLoopbackMode, int(bool loopback));

  MOCK_METHOD1(SetDiffServCodePoint, int(DiffServCodePoint dscp));

  MOCK_METHOD0(DetachFromThread, void());

  void SetResponsePacket(const std::string& response_packet);

  int HandleRecvNow(IOBuffer* buffer,
                    int size,
                    IPEndPoint* address,
                    CompletionRepeatingCallback callback);

  int HandleRecvLater(IOBuffer* buffer,
                      int size,
                      IPEndPoint* address,
                      CompletionRepeatingCallback callback);

 private:
  std::string response_packet_;
  IPEndPoint local_address_;
};

class MockMDnsSocketFactory : public MDnsSocketFactory {
 public:
  MockMDnsSocketFactory();
  ~MockMDnsSocketFactory() override;

  void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) override;

  void SimulateReceive(const uint8_t* packet, int size);

  MOCK_METHOD1(OnSendTo, void(const std::string&));

 private:
  int SendToInternal(const std::string& packet,
                     const std::string& address,
                     CompletionOnceCallback callback);

  // The latest receive callback is always saved, since the MDnsConnection
  // does not care which socket a packet is received on.
  int RecvFromInternal(IOBuffer* buffer,
                       int size,
                       IPEndPoint* address,
                       CompletionRepeatingCallback callback);

  void CreateSocket(
      AddressFamily address_family,
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets);

  scoped_refptr<IOBuffer> recv_buffer_;
  int recv_buffer_size_;
  CompletionRepeatingCallback recv_callback_;
};

}  // namespace net

#endif  // NET_DNS_MOCK_MDNS_SOCKET_FACTORY_H_
