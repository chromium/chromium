// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DATAGRAM_SERVER_SOCKET_H_
#define NET_SOCKET_DATAGRAM_SERVER_SOCKET_H_

#include <stdint.h>

#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/diff_serv_code_point.h"

namespace net {

class IPAddress;
class IPEndPoint;
class IOBuffer;

// A UDP Socket.
class NET_EXPORT DatagramServerSocket : public DatagramSocket {
 public:
  ~DatagramServerSocket() override = default;

  // Initialize this socket as a server socket listening at |address|.
  // Returns a network error code.
  virtual int Listen(const IPEndPoint& address) = 0;

  // Read from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |callback| is the callback on completion of the RecvFrom.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  virtual int RecvFrom(IOBuffer* buf,
                       int buf_len,
                       IPEndPoint* address,
                       CompletionOnceCallback callback) = 0;

  // Send to a socket with a particular destination.
  // |buf| is the buffer to send.
  // |buf_len| is the number of bytes to send.
  // |address| is the recipient address.
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, the caller must keep |buf| and |address|
  // alive until the callback is called.
  virtual int SendTo(IOBuffer* buf,
                     int buf_len,
                     const IPEndPoint& address,
                     CompletionOnceCallback callback) = 0;

  // Set the receive buffer size (in bytes) for the socket.
  // Returns a net error code.
  virtual int SetReceiveBufferSize(int32_t size) = 0;

  // Set the send buffer size (in bytes) for the socket.
  // Returns a net error code.
  virtual int SetSendBufferSize(int32_t size) = 0;

  // Allow the socket to share the local address to which the socket will be
  // bound with other processes. If multiple processes are bound to the same
  // local address at the same time, behavior is undefined; e.g., it is not
  // guaranteed that incoming messages will be sent to all listening sockets.
  //
  // Should be called before Listen().
  virtual void AllowAddressReuse() = 0;

  // Allow sending and receiving packets to and from broadcast addresses.
  // Should be called before Listen().
  virtual void AllowBroadcast() = 0;

  // Allow the socket to share the local address to which the socket will be
  // bound with other processes and attempt to allow all such sockets to receive
  // the same multicast messages.
  //
  // For best cross-platform results in allowing the messages to be shared, all
  // sockets sharing the same address should join the same multicast group and
  // interface. Also, the socket should listen to the specific multicast group
  // address rather than a wildcard address (e.g. 0.0.0.0) on platforms where
  // doing so is allowed.
  //
  // Should be called before Listen().
  virtual void AllowAddressSharingForMulticast() = 0;

  // Join the multicast group with address |group_address|.
  // Returns a network error code.
  virtual int JoinGroup(const IPAddress& group_address) const = 0;

  // Leave the multicast group with address |group_address|.
  // If the socket hasn't joined the group, it will be ignored.
  // It's optional to leave the multicast group before destroying
  // the socket. It will be done by the OS.
  // Returns a network error code.
  virtual int LeaveGroup(const IPAddress& group_address) const = 0;

  // Set interface to use for multicast. If |interface_index| set to 0, default
  // interface is used.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastInterface(uint32_t interface_index) = 0;

  // Set the time-to-live option for UDP packets sent to the multicast
  // group address. The default value of this option is 1.
  // Cannot be negative or more than 255.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastTimeToLive(int time_to_live) = 0;

  // Set the loopback flag for UDP socket. If this flag is true, the host
  // will receive packets sent to the joined group from itself.
  // The default value of this option is true.
  // Should be called before Bind().
  // Returns a network error code.
  virtual int SetMulticastLoopbackMode(bool loopback) = 0;

  // Set the Differentiated Services Code Point. May do nothing on
  // some platforms. Returns a network error code.
  virtual int SetDiffServCodePoint(DiffServCodePoint dscp) = 0;

  // Resets the thread to be used for thread-safety checks.
  virtual void DetachFromThread() = 0;
};

}  // namespace net

#endif  // NET_SOCKET_DATAGRAM_SERVER_SOCKET_H_
