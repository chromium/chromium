// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "echo_server.h"
#include "gtest/gtest.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/ossocket.h"
#include "nacl_io/ostypes.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi_simple/ps.h"

#ifdef PROVIDES_SOCKET_API

using namespace nacl_io;
using namespace sdk_util;

#define LOCAL_HOST 0x7F000001
#define PORT1 4006
#define PORT2 4007
#define ANY_PORT 0

namespace {

void IP4ToSockAddr(uint32_t ip, uint16_t port, struct sockaddr_in* addr) {
  memset(addr, 0, sizeof(*addr));

  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(ip);
}

static int ki_fcntl_wrapper(int fd, int request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_fcntl(fd, request, ap);
  va_end(ap);
  return rtn;
}

static void SetNonBlocking(int sock) {
  int flags = ki_fcntl_wrapper(sock, F_GETFL);
  ASSERT_NE(-1, flags);
  flags |= O_NONBLOCK;
  ASSERT_EQ(0, ki_fcntl_wrapper(sock, F_SETFL, flags));
  ASSERT_EQ(flags, ki_fcntl_wrapper(sock, F_GETFL));
}

class SocketTest : public ::testing::Test {
 public:
  SocketTest() : sock1_(-1), sock2_(-1) {}

  void TearDown() {
    if (sock1_ != -1)
      EXPECT_EQ(0, ki_close(sock1_));
    if (sock2_ != -1)
      EXPECT_EQ(0, ki_close(sock2_));
  }

  int Bind(int fd, uint32_t ip, uint16_t port) {
    sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    IP4ToSockAddr(ip, port, &addr);
    int err = ki_bind(fd, (sockaddr*)&addr, addrlen);

    if (err == -1)
      return errno;
    return 0;
  }

 protected:
  int sock1_;
  int sock2_;
};

class SocketTestUDP : public SocketTest {
 public:
  SocketTestUDP() {}

  void SetUp() {
    sock1_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
    sock2_ = ki_socket(AF_INET, SOCK_DGRAM, 0);

    EXPECT_GT(sock1_, -1);
    EXPECT_GT(sock2_, -1);
  }
};

class SocketTestTCP : public SocketTest {
 public:
  SocketTestTCP() {}

  void SetUp() {
    sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
    sock2_ = ki_socket(AF_INET, SOCK_STREAM, 0);

    EXPECT_GT(sock1_, -1);
    EXPECT_GT(sock2_, -1);
  }
};

class SocketTestWithServer : public ::testing::Test {
 public:
  SocketTestWithServer() : instance_(PSGetInstanceId()) {
    pthread_mutex_init(&ready_lock_, NULL);
    pthread_cond_init(&ready_cond_, NULL);
  }

  void ServerThreadMain() {
    loop_.AttachToCurrentThread();
    pp::Instance instance(PSGetInstanceId());
    EchoServer server(&instance, PORT1, ServerLog, &ready_cond_, &ready_lock_);
    loop_.Run();
  }

  static void* ServerThreadMainStatic(void* arg) {
    SocketTestWithServer* test = (SocketTestWithServer*)arg;
    test->ServerThreadMain();
    return NULL;
  }

  void SetUp() {
    loop_ = pp::MessageLoop(&instance_);
    pthread_mutex_lock(&ready_lock_);

    // Start an echo server on a background thread.
    pthread_create(&server_thread_, NULL, ServerThreadMainStatic, this);

    // Wait for thread to signal that it is ready to accept connections.
    pthread_cond_wait(&ready_cond_, &ready_lock_);
    pthread_mutex_unlock(&ready_lock_);

    sock_ = ki_socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GT(sock_, -1);
  }

  void TearDown() {
    // Stop the echo server and the background thread it runs on
    loop_.PostQuit(true);
    pthread_join(server_thread_, NULL);
    ASSERT_EQ(0, ki_close(sock_));
  }

  static void ServerLog(const char* msg) {
    // Uncomment to see logs of echo server on stdout
    //printf("server: %s\n", msg);
  }

 protected:
  int sock_;
  pp::MessageLoop loop_;
  pp::Instance instance_;
  pthread_cond_t ready_cond_;
  pthread_mutex_t ready_lock_;
  pthread_t server_thread_;
};

}  // namespace

TEST(SocketTestSimple, Socket) {
  EXPECT_EQ(-1, ki_socket(AF_UNIX, SOCK_STREAM, 0));
  EXPECT_EQ(errno, EAFNOSUPPORT);

  // We don't support RAW sockets
  EXPECT_EQ(-1, ki_socket(AF_INET, SOCK_RAW, IPPROTO_TCP));
  EXPECT_EQ(EPROTONOSUPPORT, errno);

  // Invalid type
  EXPECT_EQ(-1, ki_socket(AF_INET, -1, 0));
  EXPECT_EQ(EINVAL, errno);

  int sock1_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock1_);

  int sock2_ = ki_socket(AF_INET6, SOCK_DGRAM, 0);
  EXPECT_NE(-1, sock2_);

  int sock3 = ki_socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock3);

  int sock4 = ki_socket(AF_INET6, SOCK_STREAM, 0);
  EXPECT_NE(-1, sock4);

  ki_close(sock1_);
  ki_close(sock2_);
  ki_close(sock3);
  ki_close(sock4);
}

TEST_F(SocketTestUDP, Bind) {
  // Bind away.
  EXPECT_EQ(0, Bind(sock1_, LOCAL_HOST, PORT1));

  // Invalid to rebind a socket.
  EXPECT_EQ(EINVAL, Bind(sock1_, LOCAL_HOST, PORT1));

  // Addr in use.
  EXPECT_EQ(EADDRINUSE, Bind(sock2_, LOCAL_HOST, PORT1));

  // Bind with a wildcard.
  EXPECT_EQ(0, Bind(sock2_, LOCAL_HOST, ANY_PORT));

  // Invalid to rebind after wildcard
  EXPECT_EQ(EINVAL, Bind(sock2_, LOCAL_HOST, PORT1));
}

TEST_F(SocketTestUDP, SendRecv) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));
  memset(inbuf, 0, sizeof(inbuf));

  EXPECT_EQ(0, Bind(sock1_, LOCAL_HOST, PORT1));
  EXPECT_EQ(0, Bind(sock2_, LOCAL_HOST, PORT2));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr);

  int len1 =
     ki_sendto(sock1_, outbuf, sizeof(outbuf), 0, (sockaddr*) &addr, addrlen);
  EXPECT_EQ(sizeof(outbuf), len1);

  // Ensure the buffers are different
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  memset(&addr, 0, sizeof(addr));

  // Try to receive the previously sent packet
  int len2 =
    ki_recvfrom(sock2_, inbuf, sizeof(inbuf), 0, (sockaddr*) &addr, &addrlen);
  EXPECT_EQ(sizeof(outbuf), len2);
  EXPECT_EQ(sizeof(sockaddr_in), addrlen);
  EXPECT_EQ(PORT1, htons(addr.sin_port));

  // Now they should be the same
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

TEST_F(SocketTestUDP, SendRecvUnbound) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));
  memset(inbuf, 0, sizeof(inbuf));

  // Don't bind sock1_, this will automatically bind sock1_ to a random port
  // at the time of the first send.
  EXPECT_EQ(0, Bind(sock2_, LOCAL_HOST, PORT2));

  sockaddr_in addr;
  sockaddr_in addr2;
  socklen_t addrlen = sizeof(addr2);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr2);

  // The first send hasn't occurred, so the socket is not yet bound.
  socklen_t out_addrlen = sizeof(addr);
  ASSERT_EQ(0, ki_getsockname(sock1_, (sockaddr*)&addr, &out_addrlen));
  EXPECT_EQ(addrlen, out_addrlen);
  EXPECT_EQ(0, htonl(addr.sin_addr.s_addr));
  EXPECT_EQ(0, htons(addr.sin_port));

  int len1 =
     ki_sendto(sock1_, outbuf, sizeof(outbuf), 0, (sockaddr*) &addr2, addrlen);
  EXPECT_EQ(sizeof(outbuf), len1);

  // After the first send, the socket should be bound; the port is set, but
  // the address is still 0.
  ASSERT_EQ(0, ki_getsockname(sock1_, (sockaddr*)&addr, &out_addrlen));
  EXPECT_EQ(addrlen, out_addrlen);
  EXPECT_EQ(0, htonl(addr.sin_addr.s_addr));
  EXPECT_NE(0, htons(addr.sin_port));

  // Ensure the buffers are different
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  // Try to receive the previously sent packet
  int len2 =
    ki_recvfrom(sock2_, inbuf, sizeof(inbuf), 0, (sockaddr*) &addr, &addrlen);
  EXPECT_EQ(sizeof(outbuf), len2);
  EXPECT_EQ(sizeof(sockaddr_in), addrlen);
  EXPECT_EQ(LOCAL_HOST, htonl(addr.sin_addr.s_addr));
  EXPECT_NE(0, htons(addr.sin_port));

  // Now they should be the same
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

TEST_F(SocketTestUDP, SendmsgRecvmsg) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));
  memset(inbuf, 0, sizeof(inbuf));

  struct iovec outvec[1];
  outvec[0].iov_base = &outbuf;
  outvec[0].iov_len = sizeof(outbuf);

  EXPECT_EQ(0, Bind(sock1_, LOCAL_HOST, PORT1));
  EXPECT_EQ(0, Bind(sock2_, LOCAL_HOST, PORT2));

  sockaddr_in outaddr;
  socklen_t outaddrlen = sizeof(outaddr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &outaddr);

  struct msghdr outmsg;
  outmsg.msg_name = &outaddr;
  outmsg.msg_namelen = outaddrlen;
  outmsg.msg_iov = outvec;
  outmsg.msg_iovlen = 1;

  int len1 = ki_sendmsg(sock1_, &outmsg, 0);
  EXPECT_EQ(sizeof(outbuf), len1);

  // Ensure the buffers are different
  EXPECT_NE(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
  memset(&outaddr, 0, sizeof(outaddr));

  // Try to receive the previously sent packet
  sockaddr_in inaddr;
  socklen_t inaddrlen = sizeof(inaddr);

  struct iovec invec[1];
  invec[0].iov_base = &inbuf;
  invec[0].iov_len = sizeof(inbuf);

  struct msghdr inmsg;
  inmsg.msg_name = &inaddr;
  inmsg.msg_namelen = inaddrlen;
  inmsg.msg_iov = invec;
  inmsg.msg_iovlen = 1;

  int len2 = ki_recvmsg(sock2_, &inmsg, 0);
  EXPECT_EQ(sizeof(outbuf), len2);
  EXPECT_EQ(PORT1, htons(static_cast<sockaddr_in *>(inmsg.msg_name)->sin_port));

  // Now they should be the same
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

const size_t kQueueSize = 65536 * 8;
TEST_F(SocketTestUDP, FullFifo) {
  char outbuf[16 * 1024];

  ASSERT_EQ(0, Bind(sock1_, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, Bind(sock2_, LOCAL_HOST, PORT2));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  IP4ToSockAddr(LOCAL_HOST, PORT2, &addr);

  size_t total = 0;
  while (total < kQueueSize * 8) {
    int len = ki_sendto(sock1_, outbuf, sizeof(outbuf), MSG_DONTWAIT,
                     (sockaddr*) &addr, addrlen);

    if (len <= 0) {
      EXPECT_EQ(-1, len);
      EXPECT_EQ(EWOULDBLOCK, errno);
      break;
    }

    if (len >= 0) {
      EXPECT_EQ(sizeof(outbuf), len);
      total += len;
    }
  }
  EXPECT_GT(total, kQueueSize - 1);
  EXPECT_LT(total, kQueueSize * 8);
}

TEST_F(SocketTestWithServer, TCPConnect) {
  char outbuf[256];
  char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);

  ASSERT_EQ(0, ki_connect(sock_, (sockaddr*) &addr, addrlen))
      << "Failed with " << errno << ": " << strerror(errno);

  // Send two different messages to the echo server and verify the
  // response matches.
  strcpy(outbuf, "hello");
  memset(inbuf, 0, sizeof(inbuf));
  ASSERT_EQ(sizeof(outbuf), ki_write(sock_, outbuf, sizeof(outbuf)))
      << "socket write failed with: " << strerror(errno);
  ASSERT_EQ(sizeof(outbuf), ki_read(sock_, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));

  strcpy(outbuf, "world");
  memset(inbuf, 0, sizeof(inbuf));
  ASSERT_EQ(sizeof(outbuf), ki_write(sock_, outbuf, sizeof(outbuf)));
  ASSERT_EQ(sizeof(outbuf), ki_read(sock_, inbuf, sizeof(inbuf)));
  EXPECT_EQ(0, memcmp(outbuf, inbuf, sizeof(outbuf)));
}

TEST_F(SocketTestWithServer, TCPConnectNonBlock) {
  char outbuf[256];
  //char inbuf[512];

  memset(outbuf, 1, sizeof(outbuf));

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);

  SetNonBlocking(sock_);
  ASSERT_EQ(-1, ki_connect(sock_, (sockaddr*) &addr, addrlen));
  ASSERT_EQ(EINPROGRESS, errno)
     << "expected EINPROGRESS but got: " << strerror(errno);
  ASSERT_EQ(-1, ki_connect(sock_, (sockaddr*) &addr, addrlen));
  ASSERT_EQ(EALREADY, errno);

  // Wait for the socket connection to complete using poll()
  struct pollfd pollfd = { sock_, POLLIN|POLLOUT, 0 };
  ASSERT_EQ(1, ki_poll(&pollfd, 1, -1));
  ASSERT_EQ(POLLOUT, pollfd.revents);

  // Attempts to connect again should yield EISCONN
  ASSERT_EQ(-1, ki_connect(sock_, (sockaddr*) &addr, addrlen));
  ASSERT_EQ(EISCONN, errno);

  // And SO_ERROR should be 0.
}

TEST_F(SocketTestTCP, TCPConnectFails) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  // 10 is an unassigned well-known port, nothing should be bound to it.
  IP4ToSockAddr(LOCAL_HOST, 10, &addr);
  ASSERT_EQ(-1, ki_connect(sock1_, (sockaddr*) &addr, addrlen));
  ASSERT_EQ(ECONNREFUSED, errno);
}

TEST_F(SocketTest, Getsockopt) {
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock1_, -1);
  sock2_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  EXPECT_GT(sock1_, -1);
  int socket_error = 99;
  socklen_t len = sizeof(socket_error);

  // Test for valid option (SO_ERROR) which should be 0 when a socket
  // is first created.
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_ERROR,
                             &socket_error, &len));
  ASSERT_EQ(0, socket_error);
  ASSERT_EQ(sizeof(socket_error), len);

  // Check SO_TYPE for TCP sockets
  int socket_type = 0;
  len = sizeof(socket_type);
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_TYPE, &socket_type, &len));
  ASSERT_EQ(SOCK_STREAM, socket_type);
  ASSERT_EQ(sizeof(socket_type), len);

  // Check SO_TYPE for UDP sockets
  socket_type = 0;
  len = sizeof(socket_type);
  ASSERT_EQ(0, ki_getsockopt(sock2_, SOL_SOCKET, SO_TYPE, &socket_type, &len));
  ASSERT_EQ(SOCK_DGRAM, socket_type);
  ASSERT_EQ(sizeof(socket_type), len);

  // Test for an invalid option (-1)
  ASSERT_EQ(-1, ki_getsockopt(sock1_, SOL_SOCKET, -1, &socket_error, &len));
  ASSERT_EQ(ENOPROTOOPT, errno);
}

TEST_F(SocketTest, Setsockopt) {
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_GT(sock1_, -1);

  // It should not be possible to set SO_ERROR using setsockopt.
  int socket_error = 10;
  socklen_t len = sizeof(socket_error);
  ASSERT_EQ(-1, ki_setsockopt(sock1_, SOL_SOCKET, SO_ERROR,
                              &socket_error, len));
  ASSERT_EQ(ENOPROTOOPT, errno);
}

TEST_F(SocketTest, Sockopt_TCP_NODELAY) {
  int option = 0;
  socklen_t len = sizeof(option);
  // Getting and setting TCP_NODELAY on UDP socket should fail
  sock1_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_EQ(-1, ki_setsockopt(sock1_, IPPROTO_TCP, TCP_NODELAY, &option, len));
  ASSERT_EQ(ENOPROTOOPT, errno);
  ASSERT_EQ(-1, ki_getsockopt(sock1_, IPPROTO_TCP, TCP_NODELAY, &option, &len));
  ASSERT_EQ(ENOPROTOOPT, errno);

  // Getting and setting TCP_NODELAY on TCP socket should preserve value
  sock2_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_EQ(0, ki_getsockopt(sock2_, IPPROTO_TCP, TCP_NODELAY, &option, &len));
  ASSERT_EQ(0, option);
  ASSERT_EQ(sizeof(option), len);

  option = 1;
  len = sizeof(option);
  ASSERT_EQ(0, ki_setsockopt(sock2_, IPPROTO_TCP, TCP_NODELAY, &option, len))
      << "Failed with " << errno << ": " << strerror(errno);
  ASSERT_EQ(1, option);
}

TEST_F(SocketTest, Sockopt_KEEPALIVE) {
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GT(sock1_, -1);
  sock2_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_GT(sock2_, -1);

  int value = 0;
  socklen_t len = sizeof(value);
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_KEEPALIVE, &value, &len));
  ASSERT_EQ(0, value);
  ASSERT_EQ(sizeof(int), len);
}

// Disabled until we support SO_LINGER (i.e. syncronouse close()/shutdown())
// TODO(sbc): re-enable once we fix http://crbug.com/312401
TEST_F(SocketTest, DISABLED_Sockopt_LINGER) {
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GT(sock1_, -1);
  sock2_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_GT(sock2_, -1);

  struct linger linger = { 7, 8 };
  socklen_t len = sizeof(linger);
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_LINGER, &linger, &len));
  ASSERT_EQ(0, linger.l_onoff);
  ASSERT_EQ(0, linger.l_linger);
  ASSERT_EQ(sizeof(struct linger), len);
  ASSERT_EQ(0, ki_getsockopt(sock2_, SOL_SOCKET, SO_LINGER, &linger, &len));
  ASSERT_EQ(0, linger.l_onoff);
  ASSERT_EQ(0, linger.l_linger);
  ASSERT_EQ(sizeof(struct linger), len);

  linger.l_onoff = 1;
  linger.l_linger = 77;
  len = sizeof(linger);
  ASSERT_EQ(0, ki_setsockopt(sock1_, SOL_SOCKET, SO_LINGER, &linger, len));
  linger.l_onoff = 1;
  linger.l_linger = 88;
  ASSERT_EQ(0, ki_setsockopt(sock2_, SOL_SOCKET, SO_LINGER, &linger, len));

  len = sizeof(linger);
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_LINGER, &linger, &len));
  ASSERT_EQ(1, linger.l_onoff);
  ASSERT_EQ(77, linger.l_linger);
  ASSERT_EQ(sizeof(struct linger), len);
  ASSERT_EQ(0, ki_getsockopt(sock2_, SOL_SOCKET, SO_LINGER, &linger, &len));
  ASSERT_EQ(1, linger.l_onoff);
  ASSERT_EQ(88, linger.l_linger);
  ASSERT_EQ(sizeof(struct linger), len);
}

TEST_F(SocketTest, Sockopt_REUSEADDR) {
  int value = 1;
  socklen_t len = sizeof(value);
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);

  ASSERT_GT(sock1_, -1);
  ASSERT_EQ(0, ki_setsockopt(sock1_, SOL_SOCKET, SO_REUSEADDR, &value, len));

  value = 0;
  len = sizeof(value);
  ASSERT_EQ(0, ki_getsockopt(sock1_, SOL_SOCKET, SO_REUSEADDR, &value, &len));
  ASSERT_EQ(1, value);
  ASSERT_EQ(sizeof(int), len);
}

// The size of the data to send is deliberately chosen to be
// larger than the TCP buffer in nacl_io.
// TODO(sbc): use ioctl to discover the actual buffer size at
// runtime.
#define LARGE_SEND_BYTES (800 * 1024)
TEST_F(SocketTestWithServer, LargeSend) {
  char* outbuf = (char*)malloc(LARGE_SEND_BYTES);
  char* inbuf = (char*)malloc(LARGE_SEND_BYTES);
  int bytes_sent = 0;
  int bytes_received = 0;

  // Fill output buffer with ascending integers
  int* outbuf_int = (int*)outbuf;
  int* inbuf_int = (int*)inbuf;
  for (int i = 0; i < LARGE_SEND_BYTES/sizeof(int); i++) {
    outbuf_int[i] = i;
  }

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  ASSERT_EQ(0, ki_connect(sock_, (sockaddr*) &addr, addrlen))
      << "Failed with " << errno << ": " << strerror(errno);

  // Call send an recv until all bytes have been transferred.
  while (bytes_received < LARGE_SEND_BYTES) {
    if (bytes_sent < LARGE_SEND_BYTES) {
      int sent = ki_send(sock_, outbuf + bytes_sent,
                      LARGE_SEND_BYTES - bytes_sent, MSG_DONTWAIT);
      if (sent < 0)
        ASSERT_EQ(EWOULDBLOCK, errno) << "send failed: " << strerror(errno);
      else
        bytes_sent += sent;
    }

    int received = ki_recv(sock_, inbuf + bytes_received,
                           LARGE_SEND_BYTES - bytes_received, MSG_DONTWAIT);
    if (received < 0)
      ASSERT_EQ(EWOULDBLOCK, errno) << "recv failed: " << strerror(errno);
    else
      bytes_received += received;
  }

  // Make sure there is nothing else to recv at this point
  char dummy[10];
  ASSERT_EQ(-1, ki_recv(sock_, dummy, 10, MSG_DONTWAIT));
  ASSERT_EQ(EWOULDBLOCK, errno);

  int errors = 0;
  for (int i = 0; i < LARGE_SEND_BYTES/4; i++) {
    if (inbuf_int[i] != outbuf_int[i]) {
      printf("%d: in=%d out=%d\n", i, inbuf_int[i], outbuf_int[i]);
      if (errors++ > 50)
        break;
    }
  }

  for (int i = 0; i < LARGE_SEND_BYTES; i++) {
    ASSERT_EQ(outbuf[i], inbuf[i]) << "cmp failed at " << i;
  }

  ASSERT_EQ(0, memcmp(inbuf, outbuf, LARGE_SEND_BYTES));

  free(inbuf);
  free(outbuf);
}

TEST_F(SocketTestUDP, Listen) {
  EXPECT_EQ(-1, ki_listen(sock1_, 10));
  EXPECT_EQ(errno, EOPNOTSUPP);
}

TEST_F(SocketTestUDP, Sockopt_BUFSIZE) {
  int option = 1024*1024;
  socklen_t len = sizeof(option);

  ASSERT_EQ(0, Bind(sock1_, LOCAL_HOST, ANY_PORT));

  // Modify the test to verify the change by calling getsockopt
  // once UDPInterface supports GetOption() call
  ASSERT_EQ(0, ki_setsockopt(sock1_, SOL_SOCKET, SO_RCVBUF, &option, len))
    << "failed with: " << strerror(errno);
  ASSERT_EQ(0, ki_setsockopt(sock1_, SOL_SOCKET, SO_SNDBUF, &option, len))
    << "failed with: " << strerror(errno);
}

TEST_F(SocketTestUDP, Sockopt_BROADCAST) {
  int option = 1;
  socklen_t len = sizeof(option);

  ASSERT_EQ(0, Bind(sock1_, LOCAL_HOST, ANY_PORT));

  // Modify the test to verify the change by calling getsockopt
  // once UDPInterface supports GetOption() call
  ASSERT_EQ(0, ki_setsockopt(sock1_, SOL_SOCKET, SO_BROADCAST, &option, len))
    << "failed with: " << strerror(errno);
}

TEST_F(SocketTestTCP, AcceptNoParams) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int server_sock = sock1_;

  // Bind and Listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10));

  // Connect to listening socket
  int client_sock = sock2_;
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  addrlen = sizeof(addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  // Accept without addr and len should succeed
  int new_socket = ki_accept(server_sock, NULL, NULL);
  ASSERT_GT(new_socket, -1);

  ASSERT_EQ(0, ki_close(new_socket));
}

TEST_F(SocketTestTCP, Listen) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  const char* client_greeting = "hello";
  const char* server_reply = "reply";
  const int greeting_len = strlen(client_greeting);
  const int reply_len = strlen(server_reply);

  int server_sock = sock1_;

  // Accept before listen should fail
  ASSERT_EQ(-1, ki_accept(server_sock, (sockaddr*)&addr, &addrlen));

  // Listen should fail on unbound socket
  ASSERT_EQ(-1,  ki_listen(server_sock, 10));

  // Bind and Listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0,  ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // Connect to listening socket, and send greeting
  int client_sock = sock2_;
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  addrlen = sizeof(addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  ASSERT_EQ(greeting_len, ki_send(client_sock, client_greeting,
                                  greeting_len, 0));

  // Pass in addrlen that is larger than our actual address to make
  // sure that it is correctly set back to sizeof(sockaddr_in)
  sockaddr_in client_addr[2];
  sockaddr_in cmp_addr;
  memset(&client_addr[0], 0, sizeof(client_addr[0]));
  memset(&client_addr[1], 0xab, sizeof(client_addr[1]));
  memset(&cmp_addr, 0xab, sizeof(cmp_addr));
  addrlen = sizeof(client_addr[0]) + 10;
  int new_socket = ki_accept(server_sock, (sockaddr*)&client_addr[0],
                             &addrlen);
  ASSERT_GT(new_socket, -1)
    << "accept failed with " << errno << ": " << strerror(errno);
  ASSERT_EQ(addrlen, sizeof(sockaddr_in));
  // Check that client_addr[1] and cmp_addr are the same (not overwritten).
  ASSERT_EQ(0, memcmp(&client_addr[1], &cmp_addr, sizeof(cmp_addr)));
  ASSERT_EQ(0xabab, client_addr[1].sin_port);

  // Verify addr and addrlen were set correctly
  ASSERT_EQ(addrlen, sizeof(sockaddr_in));
  ASSERT_EQ(0, ki_getsockname(client_sock, (sockaddr*)&client_addr[1],
                              &addrlen));
  ASSERT_EQ(client_addr[1].sin_family, client_addr[0].sin_family);
  ASSERT_EQ(client_addr[1].sin_port, client_addr[0].sin_port);
  ASSERT_EQ(client_addr[1].sin_addr.s_addr, client_addr[0].sin_addr.s_addr);

  // Try a call where the supplied len is smaller than the expected length.
  // The API should only write up to that amount, but should return the
  // expected length.
  sockaddr_in client_addr2;
  memset(&client_addr2, 0, sizeof(client_addr2));

  // truncated_len is the size of the structure up to and including sin_family.
  // TODO(sbc): Fix this test so it doesn't depend on the layout of the
  // sockaddr_in structure.
  socklen_t truncated_len = offsetof(sockaddr_in, sin_family) +
      sizeof(client_addr2.sin_family);
  ASSERT_GT(sizeof(sockaddr_in), truncated_len);
  ASSERT_EQ(0, ki_getsockname(client_sock, (sockaddr*)&client_addr2,
                              &truncated_len));
  ASSERT_EQ(sizeof(sockaddr_in), truncated_len);
  ASSERT_EQ(client_addr2.sin_family, client_addr[0].sin_family);
  ASSERT_EQ(client_addr2.sin_port, 0);
  ASSERT_EQ(client_addr2.sin_addr.s_addr, 0);

  // Recv greeting from client and send reply
  char inbuf[512];
  ASSERT_EQ(greeting_len, ki_recv(new_socket, inbuf, sizeof(inbuf), 0));
  inbuf[greeting_len] = 0;
  ASSERT_STREQ(inbuf, client_greeting);
  ASSERT_EQ(reply_len, ki_send(new_socket, server_reply, reply_len, 0));

  // Recv reply on client socket
  ASSERT_EQ(reply_len, ki_recv(client_sock, inbuf, sizeof(inbuf), 0));
  inbuf[reply_len] = 0;
  ASSERT_STREQ(inbuf, server_reply);

  ASSERT_EQ(0, ki_close(new_socket));
}

TEST_F(SocketTestTCP, BindAndGetSockName) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  // Bind
  ASSERT_EQ(0, Bind(sock1_, LOCAL_HOST, 0));
  EXPECT_EQ(0, ki_getsockname(sock1_, (struct sockaddr*)&addr, &addrlen));
  EXPECT_NE(0, addr.sin_port);
}

TEST_F(SocketTestTCP, ListenNonBlocking) {
  int server_sock = sock1_;

  // Set non-blocking
  SetNonBlocking(server_sock);

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // Accept should fail with EAGAIN since there is no incomming
  // connection.
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  ASSERT_EQ(-1, ki_accept(server_sock, (sockaddr*)&addr, &addrlen));
  ASSERT_EQ(EAGAIN, errno);

  // If we poll the listening socket it should also return
  // not readable to indicate that no connections are available
  // to accept.
  struct pollfd pollfd = { server_sock, POLLIN|POLLOUT, 0 };
  ASSERT_EQ(0, ki_poll(&pollfd, 1, 0));

  // Connect to listening socket
  int client_sock = sock2_;
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  addrlen = sizeof(addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  // Not poll again but with an infintie timeout.
  pollfd.fd = server_sock;
  pollfd.events = POLLIN | POLLOUT;
  ASSERT_EQ(1, ki_poll(&pollfd, 1, -1));

  // Now non-blocking accept should return the new socket
  int new_socket = ki_accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_NE(-1, new_socket)
    << "accept failed with: " << strerror(errno);
  ASSERT_EQ(0, ki_close(new_socket));

  // Accept calls should once again fail with EAGAIN
  ASSERT_EQ(-1, ki_accept(server_sock, (sockaddr*)&addr, &addrlen));
  ASSERT_EQ(EAGAIN, errno);

  // As should polling the listening socket
  pollfd.fd = server_sock;
  pollfd.events = POLLIN | POLLOUT;
  ASSERT_EQ(0, ki_poll(&pollfd, 1, 0));
}

TEST_F(SocketTestTCP, SendRecvAfterRemoteShutdown) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int server_sock = sock1_;
  int client_sock = sock2_;

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // connect to listening socket
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  addrlen = sizeof(addr);
  int new_sock = ki_accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_NE(-1, new_sock);

  const char* send_buf = "hello world";
  ASSERT_EQ(strlen(send_buf), ki_send(new_sock, send_buf, strlen(send_buf), 0));

  // Recv first 10 bytes
  char buf[256];
  ASSERT_EQ(10, ki_recv(client_sock, buf, 10, 0));

  // Close the new socket
  ASSERT_EQ(0, ki_close(new_sock));

  // Sleep for 10 milliseconds. This is designed to allow the shutdown
  // event to make its way to the client socket beofre the recv below().
  // TODO(sbc): Find a way to test this that doesn't rely on arbitrary sleep.
  usleep(100 * 1000);

  // Recv remainder
  int bytes_remaining = strlen(send_buf) - 10;
  ASSERT_EQ(bytes_remaining, ki_recv(client_sock, buf, 256, 0));

  // Attempt to read/write after remote shutdown, with no bytes remaining
  ASSERT_EQ(0, ki_recv(client_sock, buf, 10, 0));
  ASSERT_EQ(0, ki_recv(client_sock, buf, 10, 0));

  // It is still legal to send to the remote socket, even after it is closed.
  ASSERT_EQ(10, ki_send(client_sock, buf, 10, 0));
}

TEST_F(SocketTestTCP, SendRecvAfterLocalShutdown) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int server_sock = sock1_;
  int client_sock = sock2_;

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // connect to listening socket
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  addrlen = sizeof(addr);
  int new_sock = ki_accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_NE(-1, new_sock);

  // Close the new socket
  ASSERT_EQ(0, ki_shutdown(client_sock, SHUT_RDWR));

  // Attempt to read/write after shutdown
  char buffer[10];
  ASSERT_EQ(0, ki_recv(client_sock, buffer, sizeof(buffer), 0));
  ASSERT_EQ(-1, ki_send(client_sock, buffer, sizeof(buffer), 0));
  ASSERT_EQ(errno, EPIPE);
}

#define SEND_BYTES (1024)
TEST_F(SocketTestTCP, SendBufferedDataAfterShutdown) {
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int server_sock = sock1_;
  int client_sock = sock2_;

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // connect to listening socket
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  addrlen = sizeof(addr);
  int new_sock = ki_accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_NE(-1, new_sock);

  // send a fairly large amount of data and immediately close
  // the socket.
  void* buffer = alloca(SEND_BYTES);
  ASSERT_EQ(SEND_BYTES, ki_send(client_sock, buffer, SEND_BYTES, 0));
  ASSERT_EQ(0, ki_close(client_sock));

  // avoid double close of sock2_
  sock2_ = -1;

  // Attempt to recv() all the sent data.  None should be lost.
  int remainder = SEND_BYTES;
  while (remainder > 0) {
    int rtn = ki_recv(new_sock, buffer, remainder, 0);
    ASSERT_GT(rtn, 0);
    remainder -= rtn;
  }

  ASSERT_EQ(0, ki_close(new_sock));
}

TEST_F(SocketTestTCP, Sockopt_BUFSIZE) {
  int option = 1024*1024;
  socklen_t len = sizeof(option);
  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  int server_sock = sock1_;
  int client_sock = sock2_;

  // bind and listen
  ASSERT_EQ(0, Bind(server_sock, LOCAL_HOST, PORT1));
  ASSERT_EQ(0, ki_listen(server_sock, 10))
    << "listen failed with: " << strerror(errno);

  // connect to listening socket
  IP4ToSockAddr(LOCAL_HOST, PORT1, &addr);
  ASSERT_EQ(0, ki_connect(client_sock, (sockaddr*)&addr, addrlen))
    << "Failed with " << errno << ": " << strerror(errno);

  addrlen = sizeof(addr);
  int new_sock = ki_accept(server_sock, (sockaddr*)&addr, &addrlen);
  ASSERT_NE(-1, new_sock);

  // Modify the test to verify the change by calling getsockopt
  // once TCPInterface supports GetOption() call
  ASSERT_EQ(0, ki_setsockopt(sock2_, SOL_SOCKET, SO_RCVBUF, &option, len))
      << "failed with: " << strerror(errno);
  ASSERT_EQ(0, ki_setsockopt(sock2_, SOL_SOCKET, SO_SNDBUF, &option, len))
      << "failed with: " << strerror(errno);
}

TEST_F(SocketTest, Sockopt_IP_MULTICAST) {
  int ttl = 1;
  socklen_t ttl_len = sizeof(ttl);
  int loop = 1;
  socklen_t loop_len = sizeof(loop);

  // Modify the test to verify the change by calling getsockopt
  // once UDPInterface supports GetOption() call
  //
  // Setting multicast options on TCP socket should fail
  sock1_ = ki_socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_GT(sock1_, -1);
  ASSERT_EQ(-1,
            ki_setsockopt(sock1_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ttl_len));
  ASSERT_EQ(ENOPROTOOPT, errno);
  ASSERT_EQ(-1, ki_setsockopt(sock1_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
                              loop_len));
  ASSERT_EQ(ENOPROTOOPT, errno);
  EXPECT_EQ(0, Bind(sock1_, LOCAL_HOST, PORT1));

  // Setting SO_BROADCAST on UDP socket should work
  sock2_ = ki_socket(AF_INET, SOCK_DGRAM, 0);
  ASSERT_GT(sock2_, -1);

  // Test invalid values for IP_MULTICAST_TTL (0 <= ttl < 256)
  ttl = -1;
  ASSERT_EQ(-1,
            ki_setsockopt(sock2_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ttl_len));
  ASSERT_EQ(EINVAL, errno);
  ttl = 256;
  ASSERT_EQ(-1,
            ki_setsockopt(sock2_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ttl_len));
  ASSERT_EQ(EINVAL, errno);

  // Valid IP_MULTICAST_TTL value
  ttl = 1;
  ASSERT_EQ(0,
            ki_setsockopt(sock2_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ttl_len));
  ASSERT_EQ(1, ttl);
  ASSERT_EQ(sizeof(ttl), ttl_len);

  // IP_MULTICAST_LOOP
  ASSERT_EQ(
      0, ki_setsockopt(sock2_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, loop_len));
  ASSERT_EQ(1, loop);
  ASSERT_EQ(sizeof(loop), loop_len);

  EXPECT_EQ(0, Bind(sock2_, LOCAL_HOST, PORT2));
}

#endif  // PROVIDES_SOCKET_API
