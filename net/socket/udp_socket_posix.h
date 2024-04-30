// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SOCKET_POSIX_H_
#define NET_SOCKET_UDP_SOCKET_POSIX_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_tag.h"
#include "net/socket/udp_socket_global_limits.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class IPAddress;
class NetLog;
struct NetLogSource;
class SocketTag;

class NET_EXPORT UDPSocketPosix {
 public:
  UDPSocketPosix(DatagramSocket::BindType bind_type,
                 net::NetLog* net_log,
                 const net::NetLogSource& source);

  UDPSocketPosix(DatagramSocket::BindType bind_type,
                 NetLogWithSource source_net_log);

  UDPSocketPosix(const UDPSocketPosix&) = delete;
  UDPSocketPosix& operator=(const UDPSocketPosix&) = delete;

  virtual ~UDPSocketPosix();

  // Opens the socket.
  // Returns a net error code.
  int Open(AddressFamily address_family);

  // Binds this socket to |network|. All data traffic on the socket will be sent
  // and received via |network|. Must be called before Connect(). This call will
  // fail if |network| has disconnected. Communication using this socket will
  // fail if |network| disconnects.
  // Returns a net error code.
  int BindToNetwork(handles::NetworkHandle network);

  // Connects the socket to connect with a certain |address|.
  // Should be called after Open().
  // Returns a net error code.
  int Connect(const IPEndPoint& address);

  // Binds the address/port for this socket to |address|.  This is generally
  // only used on a server. Should be called after Open().
  // Returns a net error code.
  int Bind(const IPEndPoint& address);

  // Closes the socket.
  void Close();

  // Copies the remote udp address into |address| and returns a net error code.
  int GetPeerAddress(IPEndPoint* address) const;

  // Copies the local udp address into |address| and returns a net error code.
  // (similar to getsockname)
  int GetLocalAddress(IPEndPoint* address) const;

  // IO:
  // Multiple outstanding read requests are not supported.
  // Full duplex mode (reading and writing at the same time) is supported

  // Reads from the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Read(IOBuffer* buf, int buf_len, CompletionOnceCallback callback);

  // Writes to the socket.
  // Only usable from the client-side of a UDP socket, after the socket
  // has been connected.
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation);

  // Reads from a socket and receive sender address information.
  // |buf| is the buffer to read data into.
  // |buf_len| is the maximum amount of data to read.
  // |address| is a buffer provided by the caller for receiving the sender
  //   address information about the received data.  This buffer must be kept
  //   alive by the caller until the callback is placed.
  // |callback| is the callback on completion of the RecvFrom.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, this socket takes a ref to |buf| to keep
  // it alive until the data is received. However, the caller must keep
  // |address| alive until the callback is called.
  int RecvFrom(IOBuffer* buf,
               int buf_len,
               IPEndPoint* address,
               CompletionOnceCallback callback);

  // Sends to a socket with a particular destination.
  // |buf| is the buffer to send.
  // |buf_len| is the number of bytes to send.
  // |address| is the recipient address.
  // |callback| is the user callback function to call on complete.
  // Returns a net error code, or ERR_IO_PENDING if the IO is in progress.
  // If ERR_IO_PENDING is returned, this socket copies |address| for
  // asynchronous sending, and takes a ref to |buf| to keep it alive until the
  // data is sent.
  int SendTo(IOBuffer* buf,
             int buf_len,
             const IPEndPoint& address,
             CompletionOnceCallback callback);

  // Sets the receive buffer size (in bytes) for the socket.
  // Returns a net error code.
  int SetReceiveBufferSize(int32_t size);

  // Sets the send buffer size (in bytes) for the socket.
  // Returns a net error code.
  int SetSendBufferSize(int32_t size);

  // Requests that packets sent by this socket not be fragment, either locally
  // by the host, or by routers (via the DF bit in the IPv4 packet header).
  // May not be supported by all platforms. Returns a network error code if
  // there was a problem, but the socket will still be usable. Can not
  // return ERR_IO_PENDING.
  int SetDoNotFragment();

  // Requests that packets received by this socket have the ECN bit set. Returns
  // a network error code if there was a problem.
  int SetRecvTos();

  // If |confirm| is true, then the MSG_CONFIRM flag will be passed to
  // subsequent writes if it's supported by the platform.
  void SetMsgConfirm(bool confirm);

  // Returns true if the socket is already connected or bound.
  bool is_connected() const { return is_connected_; }

  const NetLogWithSource& NetLog() const { return net_log_; }

  // Call this to enable SO_REUSEADDR on the underlying socket.
  // Should be called between Open() and Bind().
  // Returns a net error code.
  int AllowAddressReuse();

  // Call this to allow or disallow sending and receiving packets to and from
  // broadcast addresses.
  // Returns a net error code.
  int SetBroadcast(bool broadcast);

  // Sets socket options to allow the socket to share the local address to which
  // the socket will be bound with other processes and attempt to allow all such
  // sockets to receive the same multicast messages. Returns a net error code.
  //
  // Ability and requirements for different sockets to receive the same messages
  // varies between POSIX platforms.  For best results in allowing the messages
  // to be shared, all sockets sharing the same address should join the same
  // multicast group and interface. Also, the socket should listen to the
  // specific multicast address rather than a wildcard address (e.g. 0.0.0.0).
  //
  // Should be called between Open() and Bind().
  int AllowAddressSharingForMulticast();

  // Joins the multicast group.
  // |group_address| is the group address to join, could be either
  // an IPv4 or IPv6 address.
  // Returns a net error code.
  int JoinGroup(const IPAddress& group_address) const;

  // Leaves the multicast group.
  // |group_address| is the group address to leave, could be either
  // an IPv4 or IPv6 address. If the socket hasn't joined the group,
  // it will be ignored.
  // It's optional to leave the multicast group before destroying
  // the socket. It will be done by the OS.
  // Returns a net error code.
  int LeaveGroup(const IPAddress& group_address) const;

  // Sets interface to use for multicast. If |interface_index| set to 0,
  // default interface is used.
  // Should be called before Bind().
  // Returns a net error code.
  int SetMulticastInterface(uint32_t interface_index);

  // Sets the time-to-live option for UDP packets sent to the multicast
  // group address. The default value of this option is 1.
  // Cannot be negative or more than 255.
  // Should be called before Bind().
  // Returns a net error code.
  int SetMulticastTimeToLive(int time_to_live);

  // Sets the loopback flag for UDP socket. If this flag is true, the host
  // will receive packets sent to the joined group from itself.
  // The default value of this option is true.
  // Should be called before Bind().
  // Returns a net error code.
  //
  // Note: the behavior of |SetMulticastLoopbackMode| is slightly
  // different between Windows and Unix-like systems. The inconsistency only
  // happens when there are more than one applications on the same host
  // joined to the same multicast group while having different settings on
  // multicast loopback mode. On Windows, the applications with loopback off
  // will not RECEIVE the loopback packets; while on Unix-like systems, the
  // applications with loopback off will not SEND the loopback packets to
  // other applications on the same host. See MSDN: http://goo.gl/6vqbj
  int SetMulticastLoopbackMode(bool loopback);

  // Sets the differentiated services flags on outgoing packets. May not
  // do anything on some platforms.
  // Returns a net error code.
  int SetDiffServCodePoint(DiffServCodePoint dscp);

  // Requests that packets sent by this socket have the DSCP and/or ECN
  // bits set. Returns a network error code if there was a problem. If
  // DSCP_NO_CHANGE or ECN_NO_CHANGE are set, will preserve those parts of
  // the original setting.
  // ECN values other than ECN_DEFAULT must not be used outside of tests,
  // without appropriate congestion control.
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn);

  // Sets IPV6_V6ONLY on the socket. If this flag is true, the socket will be
  // restricted to only IPv6; false allows both IPv4 and IPv6 traffic.
  int SetIPv6Only(bool ipv6_only);

  // Exposes the underlying socket descriptor for testing its state. Does not
  // release ownership of the descriptor.
  SocketDescriptor SocketDescriptorForTesting() const { return socket_; }

  // Resets the thread to be used for thread-safety checks.
  void DetachFromThread();

  // Apply |tag| to this socket.
  void ApplySocketTag(const SocketTag& tag);

  // Enables experimental optimization. This method should be called
  // before the socket is used to read data for the first time.
  void enable_experimental_recv_optimization() {
    DCHECK_EQ(kInvalidSocket, socket_);
    experimental_recv_optimization_enabled_ = true;
  }

  // Sets iOS Network Service Type for option SO_NET_SERVICE_TYPE.
  int SetIOSNetworkServiceType(int ios_network_service_type);

  // Takes ownership of `socket`, which should be a socket descriptor opened
  // with the specified address family. The socket should only be created but
  // not bound or connected to an address.
  int AdoptOpenedSocket(AddressFamily address_family, int socket);

  uint32_t get_multicast_interface_for_testing() {
    return multicast_interface_;
  }
  bool get_msg_confirm_for_testing() { return sendto_flags_; }
  bool get_experimental_recv_optimization_enabled_for_testing() {
    return experimental_recv_optimization_enabled_;
  }

  DscpAndEcn GetLastTos() const { return TosToDscpAndEcn(last_tos_); }

 private:
  enum SocketOptions {
    SOCKET_OPTION_MULTICAST_LOOP = 1 << 0
  };

  class ReadWatcher : public base::MessagePumpForIO::FdWatcher {
   public:
    explicit ReadWatcher(UDPSocketPosix* socket) : socket_(socket) {}

    ReadWatcher(const ReadWatcher&) = delete;
    ReadWatcher& operator=(const ReadWatcher&) = delete;

    // MessagePumpForIO::FdWatcher methods

    void OnFileCanReadWithoutBlocking(int /* fd */) override;

    void OnFileCanWriteWithoutBlocking(int /* fd */) override {}

   private:
    const raw_ptr<UDPSocketPosix> socket_;
  };

  class WriteWatcher : public base::MessagePumpForIO::FdWatcher {
   public:
    explicit WriteWatcher(UDPSocketPosix* socket) : socket_(socket) {}

    WriteWatcher(const WriteWatcher&) = delete;
    WriteWatcher& operator=(const WriteWatcher&) = delete;

    // MessagePumpForIO::FdWatcher methods

    void OnFileCanReadWithoutBlocking(int /* fd */) override {}

    void OnFileCanWriteWithoutBlocking(int /* fd */) override;

   private:
    const raw_ptr<UDPSocketPosix> socket_;
  };

  void DoReadCallback(int rv);
  void DoWriteCallback(int rv);
  void DidCompleteRead();
  void DidCompleteWrite();

  // Handles stats and logging. |result| is the number of bytes transferred, on
  // success, or the net error code on failure. On success, LogRead takes in a
  // sockaddr and its length, which are mandatory, while LogWrite takes in an
  // optional IPEndPoint.
  void LogRead(int result,
               const char* bytes,
               socklen_t addr_len,
               const sockaddr* addr);
  void LogWrite(int result, const char* bytes, const IPEndPoint* address);

  // Same as SendTo(), except that address is passed by pointer
  // instead of by reference. It is called from Write() with |address|
  // set to nullptr.
  int SendToOrWrite(IOBuffer* buf,
                    int buf_len,
                    const IPEndPoint* address,
                    CompletionOnceCallback callback);

  int InternalConnect(const IPEndPoint& address);

  // Reads data from a UDP socket. Depending whether the socket is connected or
  // not, the method delegates the call to InternalRecvFromConnectedSocket()
  // or InternalRecvFromNonConnectedSocket() respectively.
  // For proper detection of truncated reads, the |buf_len| should always be
  // one byte longer than the expected maximum packet length.
  int InternalRecvFrom(IOBuffer* buf, int buf_len, IPEndPoint* address);

  // A more efficient implementation of the InternalRecvFrom() method for
  // reading data from connected sockets. Internally the method uses the read()
  // system call.
  int InternalRecvFromConnectedSocket(IOBuffer* buf,
                                      int buf_len,
                                      IPEndPoint* address);

  // An implementation of the InternalRecvFrom() method for reading data
  // from non-connected sockets. Internally the method uses the recvmsg()
  // system call.
  int InternalRecvFromNonConnectedSocket(IOBuffer* buf,
                                         int buf_len,
                                         IPEndPoint* address);
  int InternalSendTo(IOBuffer* buf, int buf_len, const IPEndPoint* address);

  // Applies |socket_options_| to |socket_|. Should be called before
  // Bind().
  int SetMulticastOptions();
  int DoBind(const IPEndPoint& address);
  // Binds to a random port on |address|.
  int RandomBind(const IPAddress& address);

  // Sets `socket_hash_` and `tag_` on opened `socket_`.
  int ConfigureOpenedSocket();

  int socket_;

  // Hash of |socket_| to verify that it is not corrupted when calling close().
  // Used to debug https://crbug.com/906005.
  // TODO(crbug.com/41426706): Remove this once the bug is fixed.
  int socket_hash_ = 0;

  int addr_family_ = 0;
  bool is_connected_ = false;

  // Bitwise-or'd combination of SocketOptions. Specifies the set of
  // options that should be applied to |socket_| before Bind().
  int socket_options_ = SOCKET_OPTION_MULTICAST_LOOP;

  // Flags passed to sendto().
  int sendto_flags_ = 0;

  // Multicast interface.
  uint32_t multicast_interface_ = 0;

  // Multicast socket options cached for SetMulticastOption.
  // Cannot be used after Bind().
  int multicast_time_to_live_ = 1;

  // How to do source port binding, used only when UDPSocket is part of
  // UDPClientSocket, since UDPServerSocket provides Bind.
  DatagramSocket::BindType bind_type_;

  // These are mutable since they're just cached copies to make
  // GetPeerAddress/GetLocalAddress smarter.
  mutable std::unique_ptr<IPEndPoint> local_address_;
  mutable std::unique_ptr<IPEndPoint> remote_address_;

  // The socket's posix wrappers
  base::MessagePumpForIO::FdWatchController read_socket_watcher_;
  base::MessagePumpForIO::FdWatchController write_socket_watcher_;

  // The corresponding watchers for reads and writes.
  ReadWatcher read_watcher_;
  WriteWatcher write_watcher_;

  // The buffer used by InternalRead() to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_ = 0;
  raw_ptr<IPEndPoint> recv_from_address_ = nullptr;

  // The buffer used by InternalWrite() to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_ = 0;
  std::unique_ptr<IPEndPoint> send_to_address_;

  // External callback; called when read is complete.
  CompletionOnceCallback read_callback_;

  // External callback; called when write is complete.
  CompletionOnceCallback write_callback_;

  NetLogWithSource net_log_;

  // Network that this socket is bound to via BindToNetwork().
  handles::NetworkHandle bound_network_;

  // Current socket tag if |socket_| is valid, otherwise the tag to apply when
  // |socket_| is opened.
  SocketTag tag_;

  // If set to true, the socket will use an optimized experimental code path.
  // By default, the value is set to false. To use the optimization, the
  // client of the socket has to opt-in by calling the
  // enable_experimental_recv_optimization() method.
  bool experimental_recv_optimization_enabled_ = false;

  // Manages decrementing the global open UDP socket counter when this
  // UDPSocket is destroyed.
  OwnedUDPSocketCount owned_socket_count_;

  // The last TOS byte received on the socket.
  uint8_t last_tos_ = 0;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SOCKET_POSIX_H_
