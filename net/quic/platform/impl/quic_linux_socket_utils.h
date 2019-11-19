// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_LINUX_SOCKET_UTILS_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_LINUX_SOCKET_UTILS_H_

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <deque>
#include <functional>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "net/quic/platform/impl/quic_socket_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ip_address.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_socket_address.h"

#ifndef SOL_UDP
#define SOL_UDP 17
#endif

#ifndef UDP_SEGMENT
#define UDP_SEGMENT 103
#endif

#ifndef UDP_MAX_SEGMENTS
#define UDP_MAX_SEGMENTS (1 << 6UL)
#endif

namespace quic {

// BufferedWrite holds all information needed to send a packet.
struct BufferedWrite {
  BufferedWrite(const char* buffer,
                size_t buf_len,
                const QuicIpAddress& self_address,
                const QuicSocketAddress& peer_address);

  BufferedWrite(const char* buffer,
                size_t buf_len,
                const QuicIpAddress& self_address,
                const QuicSocketAddress& peer_address,
                std::unique_ptr<PerPacketOptions> options);

  BufferedWrite(BufferedWrite&&);
  ~BufferedWrite();

  const char* buffer;  // Not owned.
  size_t buf_len;
  QuicIpAddress self_address;
  QuicSocketAddress peer_address;
  std::unique_ptr<PerPacketOptions> options;
};

// QuicMMsgHdr is used to build mmsghdr objects that can be used to send
// multiple packets at once via ::sendmmsg.
//
// Example:
//   QuicDeque<BufferedWrite> buffered_writes;
//   ... (Populate buffered_writes) ...
//
//   QuicMMsgHdr mhdr(
//       buffered_writes.begin(), buffered_writes.end(), kCmsgSpaceForIp,
//       [](QuicMMsgHdr* mhdr, int i, const BufferedWrite& buffered_write) {
//         mhdr->SetIpInNextCmsg(i, buffered_write.self_address);
//       });
//
//   int num_packets_sent;
//   QuicSocketUtils::WriteMultiplePackets(fd, &mhdr, &num_packets_sent);
class QuicMMsgHdr {
 public:
  typedef std::function<
      void(QuicMMsgHdr* mhdr, int i, const BufferedWrite& buffered_write)>
      ControlBufferInitializer;
  template <typename IteratorT>
  QuicMMsgHdr(const IteratorT& first,
              const IteratorT& last,
              size_t cbuf_size,
              ControlBufferInitializer cbuf_initializer)
      : num_msgs_(std::distance(first, last)), cbuf_size_(cbuf_size) {
    static_assert(
        std::is_same<typename std::iterator_traits<IteratorT>::value_type,
                     BufferedWrite>::value,
        "Must iterate over a collection of BufferedWrite.");

    DCHECK_LE(0, num_msgs_);
    if (num_msgs_ == 0) {
      return;
    }

    storage_.reset(new char[StorageSize()]);
    memset(&storage_[0], 0, StorageSize());

    int i = -1;
    for (auto it = first; it != last; ++it) {
      ++i;

      InitOneHeader(i, *it);
      if (cbuf_initializer) {
        cbuf_initializer(this, i, *it);
      }
    }
  }

  ~QuicMMsgHdr();

  void SetIpInNextCmsg(int i, const QuicIpAddress& self_address);

  template <typename DataType>
  DataType* GetNextCmsgData(int i, int cmsg_level, int cmsg_type) {
    return reinterpret_cast<DataType*>(
        GetNextCmsgDataInternal(i, cmsg_level, cmsg_type, sizeof(DataType)));
  }

  mmsghdr* mhdr() { return GetMMsgHdr(0); }

  int num_msgs() const { return num_msgs_; }

  // Get the total number of bytes in the first |num_packets_sent| packets.
  int num_bytes_sent(int num_packets_sent);

 protected:
  void InitOneHeader(int i, const BufferedWrite& buffered_write);

  void* GetNextCmsgDataInternal(int i,
                                int cmsg_level,
                                int cmsg_type,
                                size_t data_size);

  size_t StorageSize() const {
    return num_msgs_ *
           (sizeof(mmsghdr) + sizeof(iovec) + sizeof(sockaddr_storage) +
            sizeof(cmsghdr*) + cbuf_size_);
  }

  mmsghdr* GetMMsgHdr(int i) {
    auto* first = reinterpret_cast<mmsghdr*>(&storage_[0]);
    return &first[i];
  }

  iovec* GetIov(int i) {
    auto* first = reinterpret_cast<iovec*>(GetMMsgHdr(num_msgs_));
    return &first[i];
  }

  sockaddr_storage* GetPeerAddressStorage(int i) {
    auto* first = reinterpret_cast<sockaddr_storage*>(GetIov(num_msgs_));
    return &first[i];
  }

  cmsghdr** GetCmsgHdr(int i) {
    auto** first =
        reinterpret_cast<cmsghdr**>(GetPeerAddressStorage(num_msgs_));
    return &first[i];
  }

  char* GetCbuf(int i) {
    auto* first = reinterpret_cast<char*>(GetCmsgHdr(num_msgs_));
    return &first[i * cbuf_size_];
  }

  const int num_msgs_;
  // Size of cmsg buffer for each message.
  const size_t cbuf_size_;
  // storage_ holds the memory of
  // |num_msgs_| mmsghdr
  // |num_msgs_| iovec
  // |num_msgs_| sockaddr_storage, for peer addresses
  // |num_msgs_| cmsghdr*
  // |num_msgs_| cbuf, each of size cbuf_size
  std::unique_ptr<char[]> storage_;
};

// QuicSyscallWrapper wraps system calls for testing.
class QuicSyscallWrapper {
 public:
  int Sendmmsg(int sockfd,
               mmsghdr* msgvec,
               unsigned int vlen,
               int flags) const {
    return ::sendmmsg(sockfd, msgvec, vlen, flags);
  }
};

class QuicLinuxSocketUtils : public QuicSocketUtils {
 public:
  // Return the UDP segment size of |fd|, 0 means segment size has not been set
  // on this socket. If GSO is not supported, return -1.
  static int GetUDPSegmentSize(int fd);

  // Writes the packets in |mhdr| to the socket, using ::sendmmsg.
  static WriteResult WriteMultiplePackets(int fd,
                                          QuicMMsgHdr* mhdr,
                                          int* num_packets_sent) {
    return WriteMultiplePackets(fd, mhdr, num_packets_sent,
                                QuicSyscallWrapper());
  }

  template <typename SyscallWrapper>
  static WriteResult WriteMultiplePackets(int fd,
                                          QuicMMsgHdr* mhdr,
                                          int* num_packets_sent,
                                          const SyscallWrapper& syscall) {
    *num_packets_sent = 0;

    if (mhdr->num_msgs() <= 0) {
      return WriteResult(WRITE_STATUS_ERROR, EINVAL);
    }

    int rc;
    do {
      rc = syscall.Sendmmsg(fd, mhdr->mhdr(), mhdr->num_msgs(), 0);
    } while (rc < 0 && errno == EINTR);

    if (rc > 0) {
      *num_packets_sent = rc;

      return WriteResult(WRITE_STATUS_OK, mhdr->num_bytes_sent(rc));
    } else if (rc == 0) {
      QUIC_BUG << "sendmmsg returned 0, returning WRITE_STATUS_ERROR. errno: "
               << errno;
      errno = EIO;
    }

    return WriteResult((errno == EAGAIN || errno == EWOULDBLOCK)
                           ? WRITE_STATUS_BLOCKED
                           : WRITE_STATUS_ERROR,
                       errno);
  }
};

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_LINUX_SOCKET_UTILS_H_
