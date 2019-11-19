// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SOCKET_POSIX_H_
#define NET_SOCKET_UDP_SOCKET_POSIX_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/datagram_buffer.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/diff_serv_code_point.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if defined(__ANDROID__) && defined(__aarch64__)
#define HAVE_SENDMMSG 1
#elif defined(OS_LINUX)
#define HAVE_SENDMMSG 1
#else
#define HAVE_SENDMMSG 0
#endif

namespace net {

class IPAddress;
class NetLog;
struct NetLogSource;
class SocketTag;

// Sendresult is inspired by sendmmsg, but unlike sendmmsg it is not
// convenient to require that a positive |write_count| and a negative
// error code are mutually exclusive.
struct NET_EXPORT SendResult {
  explicit SendResult();
  ~SendResult();
  SendResult(int rv, int write_count, DatagramBuffers buffers);
  SendResult(SendResult& other) = delete;
  SendResult& operator=(SendResult& other) = delete;
  SendResult(SendResult&& other);
  SendResult& operator=(SendResult&& other) = default;
  int rv;
  // number of successful writes.
  int write_count;
  DatagramBuffers buffers;
};

// Don't delay writes more than this.
const base::TimeDelta kWriteAsyncMsThreshold =
    base::TimeDelta::FromMilliseconds(1);
// Prefer local if number of writes is not more than this.
const int kWriteAsyncMinBuffersThreshold = 2;
// Don't allow more than this many outstanding async writes.
const int kWriteAsyncMaxBuffersThreshold = 16;
// PostTask immediately when unwritten buffers reaches this.
const int kWriteAsyncPostBuffersThreshold = kWriteAsyncMaxBuffersThreshold / 2;
// Don't unblock writer unless pending async writes are less than this.
const int kWriteAsyncCallbackBuffersThreshold = kWriteAsyncMaxBuffersThreshold;

// To allow mock |Send|/|Sendmsg| in testing.  This has to be
// reference counted thread safe because |SendBuffers| and
// |SendmmsgBuffers| may be invoked in another thread via PostTask*.
class NET_EXPORT UDPSocketPosixSender
    : public base::RefCountedThreadSafe<UDPSocketPosixSender> {
 public:
  UDPSocketPosixSender();

  SendResult SendBuffers(int fd, DatagramBuffers buffers);

  void SetSendmmsgEnabled(bool enabled) {
#if HAVE_SENDMMSG
    sendmmsg_enabled_ = enabled;
#endif
  }

 protected:
  friend class base::RefCountedThreadSafe<UDPSocketPosixSender>;

  virtual ~UDPSocketPosixSender();
  virtual ssize_t Send(int sockfd,
                       const void* buf,
                       size_t len,
                       int flags) const;
#if HAVE_SENDMMSG
  virtual int Sendmmsg(int sockfd,
                       struct mmsghdr* msgvec,
                       unsigned int vlen,
                       unsigned int flags) const;
#endif

  SendResult InternalSendBuffers(int fd, DatagramBuffers buffers) const;
#if HAVE_SENDMMSG
  SendResult InternalSendmmsgBuffers(int fd, DatagramBuffers buffers) const;
#endif

 private:
  UDPSocketPosixSender(const UDPSocketPosixSender&) = delete;
  UDPSocketPosixSender& operator=(const UDPSocketPosixSender&) = delete;
  bool sendmmsg_enabled_;
};

class NET_EXPORT UDPSocketPosix {
 public:
  // Performance helper for NetworkActivityMonitor, it batches
  // throughput samples, subject to a byte limit threshold (64 KB) or
  // timer (100 ms), whichever comes first.  The batching is subject
  // to a minimum number of samples (2) required by NQE to update its
  // throughput estimate.
  class ActivityMonitor {
   public:
    ActivityMonitor() : bytes_(0), increments_(0) {}
    virtual ~ActivityMonitor() {}
    // Provided by sent/received subclass.
    // Update throughput, but batch to limit overhead of NetworkActivityMonitor.
    void Increment(uint32_t bytes);
    // For flushing cached values.
    void OnClose();

   private:
    virtual void NetworkActivityMonitorIncrement(uint32_t bytes) = 0;
    void Update();
    void OnTimerFired();

    uint32_t bytes_;
    uint32_t increments_;
    base::RepeatingTimer timer_;
    DISALLOW_COPY_AND_ASSIGN(ActivityMonitor);
  };

  class SentActivityMonitor : public ActivityMonitor {
   public:
    ~SentActivityMonitor() override {}

   private:
    void NetworkActivityMonitorIncrement(uint32_t bytes) override;
  };

  class ReceivedActivityMonitor : public ActivityMonitor {
   public:
    ~ReceivedActivityMonitor() override {}

   private:
    void NetworkActivityMonitorIncrement(uint32_t bytes) override;
  };

  UDPSocketPosix(DatagramSocket::BindType bind_type,
                 net::NetLog* net_log,
                 const net::NetLogSource& source);
  virtual ~UDPSocketPosix();

  // Opens the socket.
  // Returns a net error code.
  int Open(AddressFamily address_family);

  // Binds this socket to |network|. All data traffic on the socket will be sent
  // and received via |network|. Must be called before Connect(). This call will
  // fail if |network| has disconnected. Communication using this socket will
  // fail if |network| disconnects.
  // Returns a net error code.
  int BindToNetwork(NetworkChangeNotifier::NetworkHandle network);

  // Connects the socket to connect with a certain |address|.
  // Should be called after Open().
  // Returns a net error code.
  int Connect(const IPEndPoint& address);

  // Binds the address/port for this socket to |address|.  This is generally
  // only used on a server. Should be called after Open().
  // Returns a net error code.
  int Bind(const IPEndPoint& address);

  // Closes the socket.
  // TODO(rvargas, hidehiko): Disallow re-Open() after Close().
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

  // Refer to datagram_client_socket.h
  int WriteAsync(DatagramBuffers buffers,
                 CompletionOnceCallback callback,
                 const NetworkTrafficAnnotationTag& traffic_annotation);
  int WriteAsync(const char* buffer,
                 size_t buf_len,
                 CompletionOnceCallback callback,
                 const NetworkTrafficAnnotationTag& traffic_annotation);

  DatagramBuffers GetUnwrittenBuffers();

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
  // May not be supported by all platforms. Returns a return a network error
  // code if there was a problem, but the socket will still be usable. Can not
  // return ERR_IO_PENDING.
  int SetDoNotFragment();

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

  // Resets the thread to be used for thread-safety checks.
  void DetachFromThread();

  // Apply |tag| to this socket.
  void ApplySocketTag(const SocketTag& tag);

  void SetWriteAsyncEnabled(bool enabled) { write_async_enabled_ = enabled; }
  bool WriteAsyncEnabled() { return write_async_enabled_; }

  void SetMaxPacketSize(size_t max_packet_size);

  void SetWriteMultiCoreEnabled(bool enabled) {
    write_multi_core_enabled_ = enabled;
  }

  void SetSendmmsgEnabled(bool enabled) {
    DCHECK(sender_ != nullptr);
    sender_->SetSendmmsgEnabled(enabled);
  }

  void SetWriteBatchingActive(bool active) { write_batching_active_ = active; }

  void SetWriteAsyncMaxBuffers(int value) {
    LOG(INFO) << "SetWriteAsyncMaxBuffers: " << value;
    write_async_max_buffers_ = value;
  }

  // Enables experimental optimization. This method should be called
  // before the socket is used to read data for the first time.
  void enable_experimental_recv_optimization() {
    DCHECK_EQ(kInvalidSocket, socket_);
    experimental_recv_optimization_enabled_ = true;
  }

 protected:
  // WriteAsync batching etc. are to improve throughput of large high
  // bandwidth uploads.

  // Watcher for WriteAsync paths.
  class WriteAsyncWatcher : public base::MessagePumpForIO::FdWatcher {
   public:
    explicit WriteAsyncWatcher(UDPSocketPosix* socket)
        : socket_(socket), watching_(false) {}

    // MessagePumpForIO::FdWatcher methods

    void OnFileCanReadWithoutBlocking(int /* fd */) override {}

    void OnFileCanWriteWithoutBlocking(int /* fd */) override;

    void set_watching(bool watching) { watching_ = watching; }

    bool watching() { return watching_; }

   private:
    UDPSocketPosix* const socket_;
    bool watching_;

    DISALLOW_COPY_AND_ASSIGN(WriteAsyncWatcher);
  };

  void IncreaseWriteAsyncOutstanding(int increment) {
    write_async_outstanding_ += increment;
  }

  virtual bool InternalWatchFileDescriptor();
  virtual void InternalStopWatchingFileDescriptor();

  void SetWriteCallback(CompletionOnceCallback callback) {
    write_callback_ = std::move(callback);
  }

  void DidSendBuffers(SendResult buffers);
  void FlushPending();

  std::unique_ptr<WriteAsyncWatcher> write_async_watcher_;
  scoped_refptr<UDPSocketPosixSender> sender_;
  std::unique_ptr<DatagramBufferPool> datagram_buffer_pool_;
  // |WriteAsync| pending writes, does not include buffers that have
  // been |PostTask*|'d.
  DatagramBuffers pending_writes_;

 private:
  enum SocketOptions {
    SOCKET_OPTION_MULTICAST_LOOP = 1 << 0
  };

  class ReadWatcher : public base::MessagePumpForIO::FdWatcher {
   public:
    explicit ReadWatcher(UDPSocketPosix* socket) : socket_(socket) {}

    // MessagePumpForIO::FdWatcher methods

    void OnFileCanReadWithoutBlocking(int /* fd */) override;

    void OnFileCanWriteWithoutBlocking(int /* fd */) override {}

   private:
    UDPSocketPosix* const socket_;

    DISALLOW_COPY_AND_ASSIGN(ReadWatcher);
  };

  class WriteWatcher : public base::MessagePumpForIO::FdWatcher {
   public:
    explicit WriteWatcher(UDPSocketPosix* socket) : socket_(socket) {}

    // MessagePumpForIO::FdWatcher methods

    void OnFileCanReadWithoutBlocking(int /* fd */) override {}

    void OnFileCanWriteWithoutBlocking(int /* fd */) override;

   private:
    UDPSocketPosix* const socket_;

    DISALLOW_COPY_AND_ASSIGN(WriteWatcher);
  };

  int InternalWriteAsync(CompletionOnceCallback callback,
                         const NetworkTrafficAnnotationTag& traffic_annotation);
  bool WatchFileDescriptor();
  void StopWatchingFileDescriptor();

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
  // set to NULL.
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

  // Helpers for |WriteAsync|
  base::SequencedTaskRunner* GetTaskRunner();
  void OnWriteAsyncTimerFired();
  void LocalSendBuffers();
  void PostSendBuffers();
  int ResetLastAsyncResult();
  int ResetWrittenBytes();

  int socket_;

  // Hash of |socket_| to verify that it is not corrupted when calling close().
  // Used to debug https://crbug.com/906005.
  // TODO(crbug.com/906005): Remove this once the bug is fixed.
  int socket_hash_;

  int addr_family_;
  bool is_connected_;

  // Bitwise-or'd combination of SocketOptions. Specifies the set of
  // options that should be applied to |socket_| before Bind().
  int socket_options_;

  // Flags passed to sendto().
  int sendto_flags_;

  // Multicast interface.
  uint32_t multicast_interface_;

  // Multicast socket options cached for SetMulticastOption.
  // Cannot be used after Bind().
  int multicast_time_to_live_;

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

  // Various bits to support |WriteAsync()|.
  bool write_async_enabled_ = false;
  bool write_batching_active_ = false;
  bool write_multi_core_enabled_ = false;
  int write_async_max_buffers_ = 16;
  int written_bytes_ = 0;

  int last_async_result_;
  base::RepeatingTimer write_async_timer_;
  bool write_async_timer_running_;
  // Total writes in flight, including those |PostTask*|'d.
  int write_async_outstanding_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The buffer used by InternalRead() to retry Read requests
  scoped_refptr<IOBuffer> read_buf_;
  int read_buf_len_;
  IPEndPoint* recv_from_address_;

  // The buffer used by InternalWrite() to retry Write requests
  scoped_refptr<IOBuffer> write_buf_;
  int write_buf_len_;
  std::unique_ptr<IPEndPoint> send_to_address_;

  // External callback; called when read is complete.
  CompletionOnceCallback read_callback_;

  // External callback; called when write is complete.
  CompletionOnceCallback write_callback_;

  NetLogWithSource net_log_;

  // Network that this socket is bound to via BindToNetwork().
  NetworkChangeNotifier::NetworkHandle bound_network_;

  // These are used to lower the overhead updating activity monitor.
  SentActivityMonitor sent_activity_monitor_;
  ReceivedActivityMonitor received_activity_monitor_;

  // Current socket tag if |socket_| is valid, otherwise the tag to apply when
  // |socket_| is opened.
  SocketTag tag_;

  // If set to true, the socket will use an optimized experimental code path.
  // By default, the value is set to false. To use the optimization, the
  // client of the socket has to opt-in by calling the
  // enable_experimental_recv_optimization() method.
  bool experimental_recv_optimization_enabled_;

  THREAD_CHECKER(thread_checker_);

  // Used for alternate writes that are posted for concurrent execution.
  base::WeakPtrFactory<UDPSocketPosix> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UDPSocketPosix);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SOCKET_POSIX_H_
