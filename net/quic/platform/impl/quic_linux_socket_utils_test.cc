// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_linux_socket_utils.h"

#include <netinet/in.h>
#include <stdint.h>
#include <sstream>
#include <vector>

#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;

namespace quic {
namespace test {
namespace {

class MockQuicSyscallWrapper {
 public:
  MOCK_CONST_METHOD4(
      Sendmmsg,
      int(int sockfd, mmsghdr* msgvec, unsigned int vlen, int flags));
};

class QuicLinuxSocketUtilsTest : public QuicTest {
 protected:
  WriteResult TestWriteMultiplePackets(
      int fd,
      const QuicDeque<BufferedWrite>::const_iterator& first,
      const QuicDeque<BufferedWrite>::const_iterator& last,
      int* num_packets_sent) {
    QuicMMsgHdr mhdr(
        first, last, kCmsgSpaceForIp,
        [](QuicMMsgHdr* mhdr, int i, const BufferedWrite& buffered_write) {
          mhdr->SetIpInNextCmsg(i, buffered_write.self_address);
        });

    WriteResult res = QuicLinuxSocketUtils::WriteMultiplePackets(
        fd, &mhdr, num_packets_sent, mock_syscalls_);
    return res;
  }

  MockQuicSyscallWrapper mock_syscalls_;
};

void CheckMsghdrWithoutCbuf(const msghdr* hdr,
                            const void* buffer,
                            size_t buf_len,
                            const QuicSocketAddress& peer_addr) {
  EXPECT_EQ(
      peer_addr.host().IsIPv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6),
      hdr->msg_namelen);
  sockaddr_storage peer_generic_addr = peer_addr.generic_address();
  EXPECT_EQ(0, memcmp(hdr->msg_name, &peer_generic_addr, hdr->msg_namelen));
  EXPECT_EQ(1u, hdr->msg_iovlen);
  EXPECT_EQ(buffer, hdr->msg_iov->iov_base);
  EXPECT_EQ(buf_len, hdr->msg_iov->iov_len);
  EXPECT_EQ(0, hdr->msg_flags);
  EXPECT_EQ(nullptr, hdr->msg_control);
  EXPECT_EQ(0u, hdr->msg_controllen);
}

void CheckIpAndGsoSizeInCbuf(msghdr* hdr,
                             const void* cbuf,
                             const QuicIpAddress& self_addr,
                             uint16_t gso_size) {
  const bool is_ipv4 = self_addr.IsIPv4();
  const size_t ip_cmsg_space = is_ipv4 ? kCmsgSpaceForIpv4 : kCmsgSpaceForIpv6;

  EXPECT_EQ(cbuf, hdr->msg_control);
  EXPECT_EQ(ip_cmsg_space + CMSG_SPACE(sizeof(uint16_t)), hdr->msg_controllen);

  cmsghdr* cmsg = CMSG_FIRSTHDR(hdr);
  EXPECT_EQ(cmsg->cmsg_len, is_ipv4 ? CMSG_LEN(sizeof(in_pktinfo))
                                    : CMSG_LEN(sizeof(in6_pktinfo)));
  EXPECT_EQ(cmsg->cmsg_level, is_ipv4 ? IPPROTO_IP : IPPROTO_IPV6);
  EXPECT_EQ(cmsg->cmsg_type, is_ipv4 ? IP_PKTINFO : IPV6_PKTINFO);

  const std::string& self_addr_str = self_addr.ToPackedString();
  if (is_ipv4) {
    in_pktinfo* pktinfo = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
    EXPECT_EQ(0, memcmp(&pktinfo->ipi_spec_dst, self_addr_str.c_str(),
                        self_addr_str.length()));
  } else {
    in6_pktinfo* pktinfo = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
    EXPECT_EQ(0, memcmp(&pktinfo->ipi6_addr, self_addr_str.c_str(),
                        self_addr_str.length()));
  }

  cmsg = CMSG_NXTHDR(hdr, cmsg);
  EXPECT_EQ(cmsg->cmsg_len, CMSG_LEN(sizeof(uint16_t)));
  EXPECT_EQ(cmsg->cmsg_level, SOL_UDP);
  EXPECT_EQ(cmsg->cmsg_type, UDP_SEGMENT);
  EXPECT_EQ(gso_size, *reinterpret_cast<uint16_t*>(CMSG_DATA(cmsg)));

  EXPECT_EQ(nullptr, CMSG_NXTHDR(hdr, cmsg));
}

TEST_F(QuicLinuxSocketUtilsTest, QuicMMsgHdr) {
  QuicDeque<BufferedWrite> buffered_writes;
  char packet_buf1[1024];
  char packet_buf2[512];
  buffered_writes.emplace_back(
      packet_buf1, sizeof(packet_buf1), QuicIpAddress::Loopback4(),
      QuicSocketAddress(QuicIpAddress::Loopback4(), 4));
  buffered_writes.emplace_back(
      packet_buf2, sizeof(packet_buf2), QuicIpAddress::Loopback6(),
      QuicSocketAddress(QuicIpAddress::Loopback6(), 6));

  QuicMMsgHdr quic_mhdr_without_cbuf(buffered_writes.begin(),
                                     buffered_writes.end(), 0, nullptr);
  for (size_t i = 0; i < buffered_writes.size(); ++i) {
    const BufferedWrite& bw = buffered_writes[i];
    CheckMsghdrWithoutCbuf(&quic_mhdr_without_cbuf.mhdr()[i].msg_hdr, bw.buffer,
                           bw.buf_len, bw.peer_address);
  }

  QuicMMsgHdr quic_mhdr_with_cbuf(
      buffered_writes.begin(), buffered_writes.end(),
      kCmsgSpaceForIp + kCmsgSpaceForSegmentSize,
      [](QuicMMsgHdr* mhdr, int i, const BufferedWrite& buffered_write) {
        mhdr->SetIpInNextCmsg(i, buffered_write.self_address);
        *mhdr->GetNextCmsgData<uint16_t>(i, SOL_UDP, UDP_SEGMENT) = 1300;
      });
  for (size_t i = 0; i < buffered_writes.size(); ++i) {
    const BufferedWrite& bw = buffered_writes[i];
    msghdr* hdr = &quic_mhdr_with_cbuf.mhdr()[i].msg_hdr;
    CheckIpAndGsoSizeInCbuf(hdr, hdr->msg_control, bw.self_address, 1300);
  }
}

TEST_F(QuicLinuxSocketUtilsTest, WriteMultiplePackets_NoPacketsToSend) {
  int num_packets_sent;
  QuicDeque<BufferedWrite> buffered_writes;

  EXPECT_CALL(mock_syscalls_, Sendmmsg(_, _, _, _)).Times(0);

  EXPECT_EQ(WriteResult(WRITE_STATUS_ERROR, EINVAL),
            TestWriteMultiplePackets(1, buffered_writes.begin(),
                                     buffered_writes.end(), &num_packets_sent));
}

TEST_F(QuicLinuxSocketUtilsTest, WriteMultiplePackets_WriteBlocked) {
  int num_packets_sent;
  QuicDeque<BufferedWrite> buffered_writes;
  buffered_writes.emplace_back(nullptr, 0, QuicIpAddress(),
                               QuicSocketAddress(QuicIpAddress::Any4(), 0));

  EXPECT_CALL(mock_syscalls_, Sendmmsg(_, _, _, _))
      .WillOnce(
          Invoke([](int fd, mmsghdr* msgvec, unsigned int vlen, int flags) {
            errno = EWOULDBLOCK;
            return -1;
          }));

  EXPECT_EQ(WriteResult(WRITE_STATUS_BLOCKED, EWOULDBLOCK),
            TestWriteMultiplePackets(1, buffered_writes.begin(),
                                     buffered_writes.end(), &num_packets_sent));
  EXPECT_EQ(0, num_packets_sent);
}

TEST_F(QuicLinuxSocketUtilsTest, WriteMultiplePackets_WriteError) {
  int num_packets_sent;
  QuicDeque<BufferedWrite> buffered_writes;
  buffered_writes.emplace_back(nullptr, 0, QuicIpAddress(),
                               QuicSocketAddress(QuicIpAddress::Any4(), 0));

  EXPECT_CALL(mock_syscalls_, Sendmmsg(_, _, _, _))
      .WillOnce(
          Invoke([](int fd, mmsghdr* msgvec, unsigned int vlen, int flags) {
            errno = EPERM;
            return -1;
          }));

  EXPECT_EQ(WriteResult(WRITE_STATUS_ERROR, EPERM),
            TestWriteMultiplePackets(1, buffered_writes.begin(),
                                     buffered_writes.end(), &num_packets_sent));
  EXPECT_EQ(0, num_packets_sent);
}

TEST_F(QuicLinuxSocketUtilsTest, WriteMultiplePackets_WriteSuccess) {
  int num_packets_sent;
  QuicDeque<BufferedWrite> buffered_writes;
  const int kNumBufferedWrites = 10;
  static_assert(kNumBufferedWrites < 256, "Must be less than 256");
  std::vector<std::string> buffer_holder;
  for (int i = 0; i < kNumBufferedWrites; ++i) {
    size_t buf_len = (i + 1) * 2;
    std::ostringstream buffer_ostream;
    while (buffer_ostream.str().length() < buf_len) {
      buffer_ostream << i;
    }
    buffer_holder.push_back(buffer_ostream.str().substr(0, buf_len - 1) + '$');

    buffered_writes.emplace_back(buffer_holder.back().data(), buf_len,
                                 QuicIpAddress(),
                                 QuicSocketAddress(QuicIpAddress::Any4(), 0));

    // Leave the first self_address uninitialized.
    if (i != 0) {
      ASSERT_TRUE(buffered_writes.back().self_address.FromString("127.0.0.1"));
    }

    std::ostringstream peer_ip_ostream;
    QuicIpAddress peer_ip_address;
    peer_ip_ostream << "127.0.1." << i + 1;
    ASSERT_TRUE(peer_ip_address.FromString(peer_ip_ostream.str()));
    buffered_writes.back().peer_address =
        QuicSocketAddress(peer_ip_address, i + 1);
  }

  InSequence s;

  for (int expected_num_packets_sent : {1, 2, 3, 10}) {
    SCOPED_TRACE(testing::Message()
                 << "expected_num_packets_sent=" << expected_num_packets_sent);
    EXPECT_CALL(mock_syscalls_, Sendmmsg(_, _, _, _))
        .WillOnce(Invoke([&](int fd, mmsghdr* msgvec, unsigned int vlen,
                             int flags) {
          EXPECT_LE(static_cast<unsigned int>(expected_num_packets_sent), vlen);
          for (unsigned int i = 0; i < vlen; ++i) {
            const BufferedWrite& buffered_write = buffered_writes[i];
            const msghdr& hdr = msgvec[i].msg_hdr;
            EXPECT_EQ(1u, hdr.msg_iovlen);
            EXPECT_EQ(buffered_write.buffer, hdr.msg_iov->iov_base);
            EXPECT_EQ(buffered_write.buf_len, hdr.msg_iov->iov_len);
            sockaddr_storage expected_peer_address =
                buffered_write.peer_address.generic_address();
            EXPECT_EQ(0, memcmp(&expected_peer_address, hdr.msg_name,
                                sizeof(sockaddr_storage)));
            EXPECT_EQ(buffered_write.self_address.IsInitialized(),
                      hdr.msg_control != nullptr);
          }
          return expected_num_packets_sent;
        }))
        .RetiresOnSaturation();

    int expected_bytes_written = 0;
    for (auto it = buffered_writes.cbegin();
         it != buffered_writes.cbegin() + expected_num_packets_sent; ++it) {
      expected_bytes_written += it->buf_len;
    }

    EXPECT_EQ(
        WriteResult(WRITE_STATUS_OK, expected_bytes_written),
        TestWriteMultiplePackets(1, buffered_writes.cbegin(),
                                 buffered_writes.cend(), &num_packets_sent));
    EXPECT_EQ(expected_num_packets_sent, num_packets_sent);
  }
}

}  // namespace
}  // namespace test
}  // namespace quic
