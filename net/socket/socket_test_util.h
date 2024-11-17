// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_SOCKET_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_auth_controller.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
}

namespace net {

struct CommonConnectJobParams;
class NetLog;
struct NetworkTrafficAnnotationTag;
class X509Certificate;

const handles::NetworkHandle kDefaultNetworkForTests = 1;
const handles::NetworkHandle kNewNetworkForTests = 2;

enum {
  // A private network error code used by the socket test utility classes.
  // If the |result| member of a MockRead is
  // ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ, that MockRead is just a
  // marker that indicates the peer will close the connection after the next
  // MockRead.  The other members of that MockRead are ignored.
  ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ = -10000,
};

class AsyncSocket;
class MockClientSocket;
class MockTCPClientSocket;
class MockSSLClientSocket;
class SSLClientSocket;
class StreamSocket;

enum IoMode { ASYNC, SYNCHRONOUS };

// Used to delay MockClientSocket::Connect.
// Example usage:
// TEST(FooTest, Test) {
//   MockClientSocketFactory socket_factory;
//
//   MockConnectCompleter completer;
//   SequencedSocketData data;
//   data.set_connect_data(MockConnect(&completer));
//   socket_factory.AddSocketDataProvider(&data);
//
//   // Create a MockClientSocket somehow.
//   std::unique_ptr<StreamSocket> stream = CreateStreamSocket();
//   std::optional<int> delayed_result;
//   int rv = stream->Connect(base::BindLambdaForTesting([&](int result){
//      delayed_result = result;
//   }));
//   // Connect() returns ERR_IO_PENDING.
//   EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
//
//   RunUntilIdle();
//   // Connect() is still blocked.
//   ASSERT_FALSE(delayed_result.has_value());
//
//   completer.Complete(OK);
//   RunUntilIdle();
//   EXPECT_THAT(delayed_result, Optional(IsOk()));
// }
class MockConnectCompleter {
 public:
  MockConnectCompleter();

  MockConnectCompleter(const MockConnectCompleter&) = delete;
  MockConnectCompleter& operator=(const MockConnectCompleter&) = delete;

  ~MockConnectCompleter();

  // Completes Connect() with `result`.
  void Complete(int result);

 private:
  friend class MockTCPClientSocket;
  friend class MockSSLClientSocket;
  friend class MockUDPClientSocket;

  // Sets a completion callback that is passed to Connect(). Called by
  // MockClientSocket implementations.
  void SetCallback(CompletionOnceCallback callback);

  CompletionOnceCallback callback_;
};

struct MockConnect {
  // Asynchronous connection success.
  // Creates a MockConnect with |mode| ASYC, |result| OK, and
  // |peer_addr| 192.0.2.33.
  MockConnect();
  // Creates a MockConnect with the specified mode and result, with
  // |peer_addr| 192.0.2.33.
  MockConnect(IoMode io_mode, int r);
  MockConnect(IoMode io_mode, int r, IPEndPoint addr);
  MockConnect(IoMode io_mode, int r, IPEndPoint addr, bool first_attempt_fails);
  // Creates a MockConnect that delays connection until `completer` invokes
  // Complete().
  explicit MockConnect(MockConnectCompleter* completer);
  ~MockConnect();

  IoMode mode;
  int result;
  IPEndPoint peer_addr;
  bool first_attempt_fails = false;
  raw_ptr<MockConnectCompleter> completer;
};

struct MockConfirm {
  // Asynchronous confirm success.
  // Creates a MockConfirm with |mode| ASYC and |result| OK.
  MockConfirm();
  // Creates a MockConfirm with the specified mode and result.
  MockConfirm(IoMode io_mode, int r);
  ~MockConfirm();

  IoMode mode;
  int result;
};

// MockRead and MockWrite shares the same interface and members, but we'd like
// to have distinct types because we don't want to have them used
// interchangably. To do this, a struct template is defined, and MockRead and
// MockWrite are instantiated by using this template. Template parameter |type|
// is not used in the struct definition (it purely exists for creating a new
// type).
//
// |data| in MockRead and MockWrite has different meanings: |data| in MockRead
// is the data returned from the socket when MockTCPClientSocket::Read() is
// attempted, while |data| in MockWrite is the expected data that should be
// given in MockTCPClientSocket::Write().
enum MockReadWriteType { MOCK_READ, MOCK_WRITE };

template <MockReadWriteType type>
struct MockReadWrite {
  // Flag to indicate that the message loop should be terminated.
  enum { STOPLOOP = 1 << 31 };

  // Default
  MockReadWrite()
      : mode(SYNCHRONOUS),
        result(0),
        data(nullptr),
        data_len(0),
        sequence_number(0),
        tos(0) {}

  // Read/write failure (no data).
  MockReadWrite(IoMode io_mode, int result)
      : mode(io_mode),
        result(result),
        data(nullptr),
        data_len(0),
        sequence_number(0),
        tos(0) {}

  // Read/write failure (no data), with sequence information.
  MockReadWrite(IoMode io_mode, int result, int seq)
      : mode(io_mode),
        result(result),
        data(nullptr),
        data_len(0),
        sequence_number(seq),
        tos(0) {}

  // Asynchronous read/write success (inferred data length).
  explicit MockReadWrite(const char* data)
      : mode(ASYNC),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(0),
        tos(0) {}

  // Read/write success (inferred data length).
  MockReadWrite(IoMode io_mode, const char* data)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(0),
        tos(0) {}

  // Read/write success.
  MockReadWrite(IoMode io_mode, const char* data, int data_len)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(data_len),
        sequence_number(0),
        tos(0) {}

  // Read/write success (inferred data length) with sequence information.
  MockReadWrite(IoMode io_mode, int seq, const char* data)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(strlen(data)),
        sequence_number(seq),
        tos(0) {}

  // Read/write success with sequence information.
  MockReadWrite(IoMode io_mode, const char* data, int data_len, int seq)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(data_len),
        sequence_number(seq),
        tos(0) {}

  // Read/write success with sequence and TOS information.
  MockReadWrite(IoMode io_mode,
                const char* data,
                int data_len,
                int seq,
                uint8_t tos_byte)
      : mode(io_mode),
        result(0),
        data(data),
        data_len(data_len),
        sequence_number(seq),
        tos(tos_byte) {}

  IoMode mode;
  int result;
  const char* data;
  int data_len;

  // For data providers that only allows reads to occur in a particular
  // sequence.  If a read occurs before the given |sequence_number| is reached,
  // an ERR_IO_PENDING is returned.
  int sequence_number;  // The sequence number at which a read is allowed
                        // to occur.

  // The TOS byte of the datagram, for datagram sockets only.
  uint8_t tos;
};

typedef MockReadWrite<MOCK_READ> MockRead;
typedef MockReadWrite<MOCK_WRITE> MockWrite;

struct MockWriteResult {
  MockWriteResult(IoMode io_mode, int result) : mode(io_mode), result(result) {}

  IoMode mode;
  int result;
};

class SocketDataPrinter {
 public:
  ~SocketDataPrinter() = default;

  // Prints the write in |data| using some sort of protocol-specific
  // format.
  virtual std::string PrintWrite(const std::string& data) = 0;
};

// The SocketDataProvider is an interface used by the MockClientSocket
// for getting data about individual reads and writes on the socket.  Can be
// used with at most one socket at a time.
// TODO(mmenke):  Do these really need to be re-useable?
class SocketDataProvider {
 public:
  SocketDataProvider();

  SocketDataProvider(const SocketDataProvider&) = delete;
  SocketDataProvider& operator=(const SocketDataProvider&) = delete;

  virtual ~SocketDataProvider();

  // Returns the buffer and result code for the next simulated read.
  // If the |MockRead.result| is ERR_IO_PENDING, it informs the caller
  // that it will be called via the AsyncSocket::OnReadComplete()
  // function at a later time.
  virtual MockRead OnRead() = 0;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual bool AllReadDataConsumed() const = 0;
  virtual bool AllWriteDataConsumed() const = 0;
  virtual void CancelPendingRead() {}

  // Returns the last set receive buffer size, or -1 if never set.
  int receive_buffer_size() const { return receive_buffer_size_; }
  void set_receive_buffer_size(int receive_buffer_size) {
    receive_buffer_size_ = receive_buffer_size;
  }

  // Returns the last set send buffer size, or -1 if never set.
  int send_buffer_size() const { return send_buffer_size_; }
  void set_send_buffer_size(int send_buffer_size) {
    send_buffer_size_ = send_buffer_size;
  }

  // Returns the last set value of TCP no delay, or false if never set.
  bool no_delay() const { return no_delay_; }
  void set_no_delay(bool no_delay) { no_delay_ = no_delay; }

  // Returns whether TCP keepalives were enabled or not. Returns kDefault by
  // default.
  enum class KeepAliveState { kEnabled, kDisabled, kDefault };
  KeepAliveState keep_alive_state() const { return keep_alive_state_; }
  // Last set TCP keepalive delay.
  int keep_alive_delay() const { return keep_alive_delay_; }
  void set_keep_alive(bool enable, int delay) {
    keep_alive_state_ =
        enable ? KeepAliveState::kEnabled : KeepAliveState::kDisabled;
    keep_alive_delay_ = delay;
  }

  // Setters / getters for the return values of the corresponding Set*()
  // methods. By default, they all succeed, if the socket is connected.

  void set_set_receive_buffer_size_result(int receive_buffer_size_result) {
    set_receive_buffer_size_result_ = receive_buffer_size_result;
  }
  int set_receive_buffer_size_result() const {
    return set_receive_buffer_size_result_;
  }

  void set_set_send_buffer_size_result(int set_send_buffer_size_result) {
    set_send_buffer_size_result_ = set_send_buffer_size_result;
  }
  int set_send_buffer_size_result() const {
    return set_send_buffer_size_result_;
  }

  void set_set_no_delay_result(bool set_no_delay_result) {
    set_no_delay_result_ = set_no_delay_result;
  }
  bool set_no_delay_result() const { return set_no_delay_result_; }

  void set_set_keep_alive_result(bool set_keep_alive_result) {
    set_keep_alive_result_ = set_keep_alive_result;
  }
  bool set_keep_alive_result() const { return set_keep_alive_result_; }

  const std::optional<AddressList>& expected_addresses() const {
    return expected_addresses_;
  }
  void set_expected_addresses(net::AddressList addresses) {
    expected_addresses_ = std::move(addresses);
  }

  // Returns true if the request should be considered idle, for the purposes of
  // IsConnectedAndIdle.
  virtual bool IsIdle() const;

  // Initializes the SocketDataProvider for use with |socket|.  Must be called
  // before use
  void Initialize(AsyncSocket* socket);
  // Detaches the socket associated with a SocketDataProvider.  Must be called
  // before |socket_| is destroyed, unless the SocketDataProvider has informed
  // |socket_| it was destroyed.  Must also be called before Initialize() may
  // be called again with a new socket.
  void DetachSocket();

  // Accessor for the socket which is using the SocketDataProvider.
  AsyncSocket* socket() { return socket_; }

  MockConnect connect_data() const { return connect_; }
  void set_connect_data(const MockConnect& connect) { connect_ = connect; }

 private:
  // Called to inform subclasses of initialization.
  virtual void Reset() = 0;

  MockConnect connect_;
  raw_ptr<AsyncSocket> socket_ = nullptr;

  int receive_buffer_size_ = -1;
  int send_buffer_size_ = -1;
  // This reflects the default state of TCPClientSockets.
  bool no_delay_ = true;

  KeepAliveState keep_alive_state_ = KeepAliveState::kDefault;
  int keep_alive_delay_ = 0;

  int set_receive_buffer_size_result_ = net::OK;
  int set_send_buffer_size_result_ = net::OK;
  bool set_no_delay_result_ = true;
  bool set_keep_alive_result_ = true;
  std::optional<AddressList> expected_addresses_;
};

// The AsyncSocket is an interface used by the SocketDataProvider to
// complete the asynchronous read operation.
class AsyncSocket {
 public:
  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the AsyncSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  // data.async is ignored, and this read is completed synchronously as
  // part of this call.
  // TODO(rch): this should take a std::string_view since most of the fields
  // are ignored.
  virtual void OnReadComplete(const MockRead& data) = 0;
  // If an async IO is pending because the SocketDataProvider returned
  // ERR_IO_PENDING, then the AsyncSocket waits until this OnReadComplete
  // is called to complete the asynchronous read operation.
  virtual void OnWriteComplete(int rv) = 0;
  virtual void OnConnectComplete(const MockConnect& data) = 0;

  // Called when the SocketDataProvider associated with the socket is destroyed.
  // The socket may continue to be used after the data provider is destroyed,
  // so it should be sure not to dereference the provider after this is called.
  virtual void OnDataProviderDestroyed() = 0;
};

// StaticSocketDataHelper manages a list of reads and writes.
class StaticSocketDataHelper {
 public:
  StaticSocketDataHelper(base::span<const MockRead> reads,
                         base::span<const MockWrite> writes);

  StaticSocketDataHelper(const StaticSocketDataHelper&) = delete;
  StaticSocketDataHelper& operator=(const StaticSocketDataHelper&) = delete;

  ~StaticSocketDataHelper();

  // These functions get access to the next available read and write data. They
  // CHECK fail if there is no data available.
  const MockRead& PeekRead() const;
  const MockWrite& PeekWrite() const;

  // Returns the current read or write, and then advances to the next one.
  const MockRead& AdvanceRead();
  const MockWrite& AdvanceWrite();

  // Resets the read and write indexes to 0.
  void Reset();

  // Returns true if |data| is valid data for the next write. In order
  // to support short writes, the next write may be longer than |data|
  // in which case this method will still return true.
  bool VerifyWriteData(const std::string& data, SocketDataPrinter* printer);

  size_t read_index() const { return read_index_; }
  size_t write_index() const { return write_index_; }
  size_t read_count() const { return reads_.size(); }
  size_t write_count() const { return writes_.size(); }

  bool AllReadDataConsumed() const { return read_index() >= read_count(); }
  bool AllWriteDataConsumed() const { return write_index() >= write_count(); }

  void ExpectAllReadDataConsumed(SocketDataPrinter* printer) const;
  void ExpectAllWriteDataConsumed(SocketDataPrinter* printer) const;

 private:
  // Returns the next available read or write that is not a pause event. CHECK
  // fails if no data is available.
  const MockWrite& PeekRealWrite() const;

  const base::raw_span<const MockRead, DanglingUntriaged> reads_;
  size_t read_index_ = 0;
  const base::raw_span<const MockWrite, DanglingUntriaged> writes_;
  size_t write_index_ = 0;
};

// SocketDataProvider which responds based on static tables of mock reads and
// writes.
class StaticSocketDataProvider : public SocketDataProvider {
 public:
  StaticSocketDataProvider();
  StaticSocketDataProvider(base::span<const MockRead> reads,
                           base::span<const MockWrite> writes);

  StaticSocketDataProvider(const StaticSocketDataProvider&) = delete;
  StaticSocketDataProvider& operator=(const StaticSocketDataProvider&) = delete;

  ~StaticSocketDataProvider() override;

  // Pause/resume reads from this provider.
  void Pause();
  void Resume();

  // From SocketDataProvider:
  MockRead OnRead() override;
  MockWriteResult OnWrite(const std::string& data) override;
  bool AllReadDataConsumed() const override;
  bool AllWriteDataConsumed() const override;

  size_t read_index() const { return helper_.read_index(); }
  size_t write_index() const { return helper_.write_index(); }
  size_t read_count() const { return helper_.read_count(); }
  size_t write_count() const { return helper_.write_count(); }

  void set_printer(SocketDataPrinter* printer) { printer_ = printer; }

 private:
  // From SocketDataProvider:
  void Reset() override;

  StaticSocketDataHelper helper_;
  raw_ptr<SocketDataPrinter> printer_ = nullptr;
  bool paused_ = false;
};

// SSLSocketDataProviders only need to keep track of the return code from calls
// to Connect().
struct SSLSocketDataProvider {
  SSLSocketDataProvider(IoMode mode, int result);
  explicit SSLSocketDataProvider(MockConnectCompleter* completer);
  SSLSocketDataProvider(const SSLSocketDataProvider& other);
  ~SSLSocketDataProvider();

  // Returns whether MockConnect data has been consumed.
  bool ConnectDataConsumed() const { return is_connect_data_consumed; }

  // Returns whether MockConfirm data has been consumed.
  bool ConfirmDataConsumed() const { return is_confirm_data_consumed; }

  // Returns whether a Write occurred before ConfirmHandshake completed.
  bool WriteBeforeConfirm() const { return write_called_before_confirm; }

  // Result for Connect().
  MockConnect connect;
  // Callback to run when Connect() is called. This is called at most once per
  // socket but is repeating because SSLSocketDataProvider is copyable.
  base::RepeatingClosure connect_callback;

  // Result for ConfirmHandshake().
  MockConfirm confirm;
  // Callback to run when ConfirmHandshake() is called. This is called at most
  // once per socket but is repeating because SSLSocketDataProvider is
  // copyable.
  base::RepeatingClosure confirm_callback;

  // Result for GetNegotiatedProtocol().
  NextProto next_proto = kProtoUnknown;

  // Result for GetPeerApplicationSettings().
  std::optional<std::string> peer_application_settings;

  // Result for GetSSLInfo().
  SSLInfo ssl_info;

  // Result for GetSSLCertRequestInfo().
  scoped_refptr<SSLCertRequestInfo> cert_request_info;

  // Result for GetECHRetryConfigs().
  std::vector<uint8_t> ech_retry_configs;

  std::optional<NextProtoVector> next_protos_expected_in_ssl_config;
  std::optional<SSLConfig::ApplicationSettings> expected_application_settings;

  uint16_t expected_ssl_version_min;
  uint16_t expected_ssl_version_max;
  std::optional<bool> expected_early_data_enabled;
  std::optional<bool> expected_send_client_cert;
  scoped_refptr<X509Certificate> expected_client_cert;
  std::optional<HostPortPair> expected_host_and_port;
  std::optional<bool> expected_ignore_certificate_errors;
  std::optional<NetworkAnonymizationKey> expected_network_anonymization_key;
  std::optional<std::vector<uint8_t>> expected_ech_config_list;

  bool is_connect_data_consumed = false;
  bool is_confirm_data_consumed = false;
  bool write_called_before_confirm = false;
};

// Uses the sequence_number field in the mock reads and writes to
// complete the operations in a specified order.
class SequencedSocketData : public SocketDataProvider {
 public:
  SequencedSocketData();

  // |reads| is the list of MockRead completions.
  // |writes| is the list of MockWrite completions.
  SequencedSocketData(base::span<const MockRead> reads,
                      base::span<const MockWrite> writes);

  // |connect| is the result for the connect phase.
  // |reads| is the list of MockRead completions.
  // |writes| is the list of MockWrite completions.
  SequencedSocketData(const MockConnect& connect,
                      base::span<const MockRead> reads,
                      base::span<const MockWrite> writes);

  SequencedSocketData(const SequencedSocketData&) = delete;
  SequencedSocketData& operator=(const SequencedSocketData&) = delete;

  ~SequencedSocketData() override;

  // From SocketDataProvider:
  MockRead OnRead() override;
  MockWriteResult OnWrite(const std::string& data) override;
  bool AllReadDataConsumed() const override;
  bool AllWriteDataConsumed() const override;
  bool IsIdle() const override;
  void CancelPendingRead() override;

  // EXPECTs that all data has been consumed, printing any un-consumed data.
  void ExpectAllReadDataConsumed() const;
  void ExpectAllWriteDataConsumed() const;

  // An ASYNC read event with a return value of ERR_IO_PENDING will cause the
  // socket data to pause at that event, and advance no further, until Resume is
  // invoked.  At that point, the socket will continue at the next event in the
  // sequence.
  //
  // If a request just wants to simulate a connection that stays open and never
  // receives any more data, instead of pausing and then resuming a request, it
  // should use a SYNCHRONOUS event with a return value of ERR_IO_PENDING
  // instead.
  bool IsPaused() const;
  // Resumes events once |this| is in the paused state.  The next event will
  // occur synchronously with the call if it can.
  void Resume();
  void RunUntilPaused();

  // When true, IsConnectedAndIdle() will return false if the next event in the
  // sequence is a synchronous.  Otherwise, the socket claims to be idle as
  // long as it's connected.  Defaults to false.
  // TODO(mmenke):  See if this can be made the default behavior, and consider
  // removing this mehtod.  Need to make sure it doesn't change what code any
  // tests are targetted at testing.
  void set_busy_before_sync_reads(bool busy_before_sync_reads) {
    busy_before_sync_reads_ = busy_before_sync_reads;
  }

  void set_printer(SocketDataPrinter* printer) { printer_ = printer; }

 private:
  // Defines the state for the read or write path.
  enum class IoState {
    kIdle,        // No async operation is in progress.
    kPending,     // An async operation in waiting for another operation to
                  // complete.
    kCompleting,  // A task has been posted to complete an async operation.
    kPaused,      // IO is paused until Resume() is called.
  };

  // From SocketDataProvider:
  void Reset() override;

  void OnReadComplete();
  void OnWriteComplete();

  void MaybePostReadCompleteTask();
  void MaybePostWriteCompleteTask();

  StaticSocketDataHelper helper_;
  raw_ptr<SocketDataPrinter> printer_ = nullptr;
  int sequence_number_ = 0;
  IoState read_state_ = IoState::kIdle;
  IoState write_state_ = IoState::kIdle;

  bool busy_before_sync_reads_ = false;

  // Used by RunUntilPaused.  NULL at all other times.
  std::unique_ptr<base::RunLoop> run_until_paused_run_loop_;

  base::WeakPtrFactory<SequencedSocketData> weak_factory_{this};
};

// Holds an array of SocketDataProvider elements.  As Mock{TCP,SSL}StreamSocket
// objects get instantiated, they take their data from the i'th element of this
// array.
template <typename T>
class SocketDataProviderArray {
 public:
  SocketDataProviderArray() = default;

  T* GetNext() {
    DCHECK_LT(next_index_, data_providers_.size());
    return data_providers_[next_index_++];
  }

  // Like GetNext(), but returns nullptr when the end of the array is reached,
  // instead of DCHECKing. GetNext() should generally be preferred, unless
  // having no remaining elements is expected in some cases and is handled
  // safely.
  T* GetNextWithoutAsserting() {
    if (next_index_ == data_providers_.size())
      return nullptr;
    return data_providers_[next_index_++];
  }

  void Add(T* data_provider) {
    DCHECK(data_provider);
    data_providers_.push_back(data_provider);
  }

  size_t next_index() { return next_index_; }

  void ResetNextIndex() { next_index_ = 0; }

 private:
  // Index of the next |data_providers_| element to use. Not an iterator
  // because those are invalidated on vector reallocation.
  size_t next_index_ = 0;

  // SocketDataProviders to be returned.
  std::vector<T*> data_providers_;
};

class MockUDPClientSocket;
class MockTCPClientSocket;
class MockSSLClientSocket;

// ClientSocketFactory which contains arrays of sockets of each type.
// You should first fill the arrays using Add{SSL,}SocketDataProvider(). When
// the factory is asked to create a socket, it takes next entry from appropriate
// array. You can use ResetNextMockIndexes to reset that next entry index for
// all mock socket types.
class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory();

  MockClientSocketFactory(const MockClientSocketFactory&) = delete;
  MockClientSocketFactory& operator=(const MockClientSocketFactory&) = delete;

  ~MockClientSocketFactory() override;

  // Adds a SocketDataProvider that can be used to served either TCP or UDP
  // connection requests. Sockets are returned in FIFO order.
  void AddSocketDataProvider(SocketDataProvider* socket);

  // Like AddSocketDataProvider(), except sockets will only be used to service
  // TCP connection requests. Sockets added with this method are used first,
  // before sockets added with AddSocketDataProvider(). Particularly useful for
  // QUIC tests with multiple sockets, where TCP connections may or may not be
  // made, and have no guaranteed order, relative to UDP connections.
  void AddTcpSocketDataProvider(SocketDataProvider* socket);

  void AddSSLSocketDataProvider(SSLSocketDataProvider* socket);
  void ResetNextMockIndexes();

  SocketDataProviderArray<SocketDataProvider>& mock_data() {
    return mock_data_;
  }

  void set_enable_read_if_ready(bool enable_read_if_ready) {
    enable_read_if_ready_ = enable_read_if_ready;
  }

  // ClientSocketFactory
  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override;
  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override;
  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override;
  const std::vector<uint16_t>& udp_client_socket_ports() const {
    return udp_client_socket_ports_;
  }

 private:
  SocketDataProviderArray<SocketDataProvider> mock_data_;
  SocketDataProviderArray<SocketDataProvider> mock_tcp_data_;
  SocketDataProviderArray<SSLSocketDataProvider> mock_ssl_data_;
  std::vector<uint16_t> udp_client_socket_ports_;

  // If true, ReadIfReady() is enabled; otherwise ReadIfReady() returns
  // ERR_READ_IF_READY_NOT_IMPLEMENTED.
  bool enable_read_if_ready_ = false;
};

class MockClientSocket : public TransportClientSocket {
 public:
  // The NetLogWithSource is needed to test LoadTimingInfo, which uses NetLog
  // IDs as
  // unique socket IDs.
  explicit MockClientSocket(const NetLogWithSource& net_log);

  MockClientSocket(const MockClientSocket&) = delete;
  MockClientSocket& operator=(const MockClientSocket&) = delete;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override = 0;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override = 0;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // TransportClientSocket implementation.
  int Bind(const net::IPEndPoint& local_addr) override;
  bool SetNoDelay(bool no_delay) override;
  bool SetKeepAlive(bool enable, int delay) override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override = 0;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  NextProto GetNegotiatedProtocol() const override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override {}

 protected:
  ~MockClientSocket() override;
  void RunCallbackAsync(CompletionOnceCallback callback, int result);
  void RunCallback(CompletionOnceCallback callback, int result);

  // True if Connect completed successfully and Disconnect hasn't been called.
  bool connected_ = false;

  IPEndPoint local_addr_;
  IPEndPoint peer_addr_;

  NetLogWithSource net_log_;

 private:
  base::WeakPtrFactory<MockClientSocket> weak_factory_{this};
};

class MockTCPClientSocket : public MockClientSocket, public AsyncSocket {
 public:
  MockTCPClientSocket(const AddressList& addresses,
                      net::NetLog* net_log,
                      SocketDataProvider* socket);

  MockTCPClientSocket(const MockTCPClientSocket&) = delete;
  MockTCPClientSocket& operator=(const MockTCPClientSocket&) = delete;

  ~MockTCPClientSocket() override;

  const AddressList& addresses() const { return addresses_; }

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // TransportClientSocket implementation.
  bool SetNoDelay(bool no_delay) override;
  bool SetKeepAlive(bool enable, int delay) override;

  // StreamSocket implementation.
  void SetBeforeConnectCallback(
      const BeforeConnectCallback& before_connect_callback) override;
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  bool WasEverUsed() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;

  // AsyncSocket:
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;
  void OnDataProviderDestroyed() override;

  void set_enable_read_if_ready(bool enable_read_if_ready) {
    enable_read_if_ready_ = enable_read_if_ready;
  }

 private:
  void RetryRead(int rv);
  int ReadIfReadyImpl(IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback);

  // Helper method to run |pending_read_if_ready_callback_| if it is not null.
  void RunReadIfReadyCallback(int result);

  AddressList addresses_;

  raw_ptr<SocketDataProvider> data_;
  int read_offset_ = 0;
  MockRead read_data_;
  bool need_read_data_ = true;

  // True if the peer has closed the connection.  This allows us to simulate
  // the recv(..., MSG_PEEK) call in the IsConnectedAndIdle method of the real
  // TCPClientSocket.
  bool peer_closed_connection_ = false;

  // While an asynchronous read is pending, we save our user-buffer state.
  scoped_refptr<IOBuffer> pending_read_buf_ = nullptr;
  int pending_read_buf_len_ = 0;
  CompletionOnceCallback pending_read_callback_;

  // Non-null when a ReadIfReady() is pending.
  CompletionOnceCallback pending_read_if_ready_callback_;

  CompletionOnceCallback pending_connect_callback_;
  CompletionOnceCallback pending_write_callback_;
  bool was_used_to_convey_data_ = false;

  // If true, ReadIfReady() is enabled; otherwise ReadIfReady() returns
  // ERR_READ_IF_READY_NOT_IMPLEMENTED.
  bool enable_read_if_ready_ = false;

  BeforeConnectCallback before_connect_callback_;
};

class MockSSLClientSocket : public AsyncSocket, public SSLClientSocket {
 public:
  MockSSLClientSocket(std::unique_ptr<StreamSocket> stream_socket,
                      const HostPortPair& host_and_port,
                      const SSLConfig& ssl_config,
                      SSLSocketDataProvider* socket);

  MockSSLClientSocket(const MockSSLClientSocket&) = delete;
  MockSSLClientSocket& operator=(const MockSSLClientSocket&) = delete;

  ~MockSSLClientSocket() override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int CancelReadIfReady() override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  int ConfirmHandshake(CompletionOnceCallback callback) override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  bool WasEverUsed() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  NextProto GetNegotiatedProtocol() const override;
  std::optional<std::string_view> GetPeerApplicationSettings() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) const override;
  void ApplySocketTag(const SocketTag& tag) override;
  const NetLogWithSource& NetLog() const override;
  int64_t GetTotalReceivedBytes() const override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // SSLSocket implementation.
  int ExportKeyingMaterial(std::string_view label,
                           bool has_context,
                           std::string_view context,
                           unsigned char* out,
                           unsigned int outlen) override;

  // SSLClientSocket implementation.
  std::vector<uint8_t> GetECHRetryConfigs() override;

  // This MockSocket does not implement the manual async IO feature.
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;
  // SSL sockets don't need magic to deal with destruction of their data
  // provider.
  // TODO(mmenke):  Probably a good idea to support it, anyways.
  void OnDataProviderDestroyed() override {}

 private:
  static void ConnectCallback(MockSSLClientSocket* ssl_client_socket,
                              CompletionOnceCallback callback,
                              int rv);

  void RunCallbackAsync(CompletionOnceCallback callback, int result);
  void RunCallback(CompletionOnceCallback callback, int result);

  void RunConfirmHandshakeCallback(CompletionOnceCallback callback, int result);

  bool connected_ = false;
  bool in_confirm_handshake_ = false;
  NetLogWithSource net_log_;
  std::unique_ptr<StreamSocket> stream_socket_;
  raw_ptr<SSLSocketDataProvider, AcrossTasksDanglingUntriaged> data_;
  // Address of the "remote" peer we're connected to.
  IPEndPoint peer_addr_;

  base::WeakPtrFactory<MockSSLClientSocket> weak_factory_{this};
};

class MockUDPClientSocket : public DatagramClientSocket, public AsyncSocket {
 public:
  explicit MockUDPClientSocket(SocketDataProvider* data = nullptr,
                               net::NetLog* net_log = nullptr);

  MockUDPClientSocket(const MockUDPClientSocket&) = delete;
  MockUDPClientSocket& operator=(const MockUDPClientSocket&) = delete;

  ~MockUDPClientSocket() override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  int SetMulticastInterface(uint32_t interface_index) override;
  const NetLogWithSource& NetLog() const override;

  // DatagramClientSocket implementation.
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) override;
  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  void SetMsgConfirm(bool confirm) override {}
  DscpAndEcn GetLastTos() const override;

  // AsyncSocket implementation.
  void OnReadComplete(const MockRead& data) override;
  void OnWriteComplete(int rv) override;
  void OnConnectComplete(const MockConnect& data) override;
  void OnDataProviderDestroyed() override;

  void set_source_port(uint16_t port) { source_port_ = port; }
  uint16_t source_port() const { return source_port_; }
  void set_source_host(IPAddress addr) { source_host_ = addr; }
  IPAddress source_host() const { return source_host_; }

  // Returns last tag applied to socket.
  SocketTag tag() const { return tag_; }

  // Returns false if socket's tag was changed after the socket was used for
  // data transfer (e.g. Read/Write() called), otherwise returns true.
  bool tagged_before_data_transferred() const {
    return tagged_before_data_transferred_;
  }

 private:
  int CompleteRead();

  void RunCallbackAsync(CompletionOnceCallback callback, int result);
  void RunCallback(CompletionOnceCallback callback, int result);

  bool connected_ = false;
  raw_ptr<SocketDataProvider> data_;
  int read_offset_ = 0;
  MockRead read_data_;
  bool need_read_data_ = true;
  IPAddress source_host_;
  uint16_t source_port_ = 123;  // Ephemeral source port.

  // Address of the "remote" peer we're connected to.
  IPEndPoint peer_addr_;

  // Network that the socket is bound to.
  handles::NetworkHandle network_ = handles::kInvalidNetworkHandle;

  // While an asynchronous IO is pending, we save our user-buffer state.
  scoped_refptr<IOBuffer> pending_read_buf_ = nullptr;
  int pending_read_buf_len_ = 0;
  CompletionOnceCallback pending_read_callback_;
  CompletionOnceCallback pending_write_callback_;

  NetLogWithSource net_log_;

  DatagramBuffers unwritten_buffers_;

  SocketTag tag_;
  bool data_transferred_ = false;
  bool tagged_before_data_transferred_ = true;

  uint8_t last_tos_ = 0;

  base::WeakPtrFactory<MockUDPClientSocket> weak_factory_{this};
};

class TestSocketRequest : public TestCompletionCallbackBase {
 public:
  TestSocketRequest(std::vector<raw_ptr<TestSocketRequest, VectorExperimental>>*
                        request_order,
                    size_t* completion_count);

  TestSocketRequest(const TestSocketRequest&) = delete;
  TestSocketRequest& operator=(const TestSocketRequest&) = delete;

  ~TestSocketRequest() override;

  ClientSocketHandle* handle() { return &handle_; }

  CompletionOnceCallback callback() {
    return base::BindOnce(&TestSocketRequest::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result);

  ClientSocketHandle handle_;
  raw_ptr<std::vector<raw_ptr<TestSocketRequest, VectorExperimental>>>
      request_order_;
  raw_ptr<size_t> completion_count_;
};

class ClientSocketPoolTest {
 public:
  enum KeepAlive {
    KEEP_ALIVE,

    // A socket will be disconnected in addition to handle being reset.
    NO_KEEP_ALIVE,
  };

  static const int kIndexOutOfBounds;
  static const int kRequestNotFound;

  ClientSocketPoolTest();

  ClientSocketPoolTest(const ClientSocketPoolTest&) = delete;
  ClientSocketPoolTest& operator=(const ClientSocketPoolTest&) = delete;

  ~ClientSocketPoolTest();

  template <typename PoolType>
  int StartRequestUsingPool(
      PoolType* socket_pool,
      const ClientSocketPool::GroupId& group_id,
      RequestPriority priority,
      ClientSocketPool::RespectLimits respect_limits,
      const scoped_refptr<typename PoolType::SocketParams>& socket_params) {
    DCHECK(socket_pool);
    TestSocketRequest* request(
        new TestSocketRequest(&request_order_, &completion_count_));
    requests_.push_back(base::WrapUnique(request));
    int rv = request->handle()->Init(
        group_id, socket_params, std::nullopt /* proxy_annotation_tag */,
        priority, SocketTag(), respect_limits, request->callback(),
        ClientSocketPool::ProxyAuthCallback(), socket_pool, NetLogWithSource());
    if (rv != ERR_IO_PENDING)
      request_order_.push_back(request);
    return rv;
  }

  // Provided there were n requests started, takes |index| in range 1..n
  // and returns order in which that request completed, in range 1..n,
  // or kIndexOutOfBounds if |index| is out of bounds, or kRequestNotFound
  // if that request did not complete (for example was canceled).
  int GetOrderOfRequest(size_t index) const;

  // Resets first initialized socket handle from |requests_|. If found such
  // a handle, returns true.
  bool ReleaseOneConnection(KeepAlive keep_alive);

  // Releases connections until there is nothing to release.
  void ReleaseAllConnections(KeepAlive keep_alive);

  // Note that this uses 0-based indices, while GetOrderOfRequest takes and
  // returns 1-based indices.
  TestSocketRequest* request(int i) { return requests_[i].get(); }

  size_t requests_size() const { return requests_.size(); }
  std::vector<std::unique_ptr<TestSocketRequest>>* requests() {
    return &requests_;
  }
  size_t completion_count() const { return completion_count_; }

 private:
  std::vector<std::unique_ptr<TestSocketRequest>> requests_;
  std::vector<raw_ptr<TestSocketRequest, VectorExperimental>> request_order_;
  size_t completion_count_ = 0;
};

class MockTransportSocketParams
    : public base::RefCounted<MockTransportSocketParams> {
 public:
  MockTransportSocketParams(const MockTransportSocketParams&) = delete;
  MockTransportSocketParams& operator=(const MockTransportSocketParams&) =
      delete;

 private:
  friend class base::RefCounted<MockTransportSocketParams>;
  ~MockTransportSocketParams() = default;
};

class MockTransportClientSocketPool : public TransportClientSocketPool {
 public:
  class MockConnectJob {
   public:
    MockConnectJob(std::unique_ptr<StreamSocket> socket,
                   ClientSocketHandle* handle,
                   const SocketTag& socket_tag,
                   CompletionOnceCallback callback,
                   RequestPriority priority);

    MockConnectJob(const MockConnectJob&) = delete;
    MockConnectJob& operator=(const MockConnectJob&) = delete;

    ~MockConnectJob();

    int Connect();
    bool CancelHandle(const ClientSocketHandle* handle);

    ClientSocketHandle* handle() const { return handle_; }

    RequestPriority priority() const { return priority_; }
    void set_priority(RequestPriority priority) { priority_ = priority; }

   private:
    void OnConnect(int rv);

    std::unique_ptr<StreamSocket> socket_;
    raw_ptr<ClientSocketHandle> handle_;
    const SocketTag socket_tag_;
    CompletionOnceCallback user_callback_;
    RequestPriority priority_;
  };

  MockTransportClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      const CommonConnectJobParams* common_connect_job_params);

  MockTransportClientSocketPool(const MockTransportClientSocketPool&) = delete;
  MockTransportClientSocketPool& operator=(
      const MockTransportClientSocketPool&) = delete;

  ~MockTransportClientSocketPool() override;

  RequestPriority last_request_priority() const {
    return last_request_priority_;
  }

  const std::vector<std::unique_ptr<MockConnectJob>>& requests() const {
    return job_list_;
  }

  int release_count() const { return release_count_; }
  int cancel_count() const { return cancel_count_; }

  // TransportClientSocketPool implementation.
  int RequestSocket(
      const GroupId& group_id,
      scoped_refptr<ClientSocketPool::SocketParams> socket_params,
      const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
      RequestPriority priority,
      const SocketTag& socket_tag,
      RespectLimits respect_limits,
      ClientSocketHandle* handle,
      CompletionOnceCallback callback,
      const ProxyAuthCallback& on_auth_callback,
      const NetLogWithSource& net_log) override;
  void SetPriority(const GroupId& group_id,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;
  void CancelRequest(const GroupId& group_id,
                     ClientSocketHandle* handle,
                     bool cancel_connect_job) override;
  void ReleaseSocket(const GroupId& group_id,
                     std::unique_ptr<StreamSocket> socket,
                     int64_t generation) override;

 private:
  raw_ptr<ClientSocketFactory> client_socket_factory_;
  std::vector<std::unique_ptr<MockConnectJob>> job_list_;
  RequestPriority last_request_priority_ = DEFAULT_PRIORITY;
  int release_count_ = 0;
  int cancel_count_ = 0;
};

// WrappedStreamSocket is a base class that wraps an existing StreamSocket,
// forwarding the Socket and StreamSocket interfaces to the underlying
// transport.
// This is to provide a common base class for subclasses to override specific
// StreamSocket methods for testing, while still communicating with a 'real'
// StreamSocket.
class WrappedStreamSocket : public TransportClientSocket {
 public:
  explicit WrappedStreamSocket(std::unique_ptr<StreamSocket> transport);
  ~WrappedStreamSocket() override;

  // StreamSocket implementation:
  int Bind(const net::IPEndPoint& local_addr) override;
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

 protected:
  std::unique_ptr<StreamSocket> transport_;
};

// StreamSocket that wraps another StreamSocket, but keeps track of any
// SocketTag applied to the socket.
class MockTaggingStreamSocket : public WrappedStreamSocket {
 public:
  explicit MockTaggingStreamSocket(std::unique_ptr<StreamSocket> transport)
      : WrappedStreamSocket(std::move(transport)) {}

  MockTaggingStreamSocket(const MockTaggingStreamSocket&) = delete;
  MockTaggingStreamSocket& operator=(const MockTaggingStreamSocket&) = delete;

  ~MockTaggingStreamSocket() override = default;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Returns false if socket's tag was changed after the socket was connected,
  // otherwise returns true.
  bool tagged_before_connected() const { return tagged_before_connected_; }

  // Returns last tag applied to socket.
  SocketTag tag() const { return tag_; }

 private:
  bool connected_ = false;
  bool tagged_before_connected_ = true;
  SocketTag tag_;
};

// Extend MockClientSocketFactory to return MockTaggingStreamSockets and
// keep track of last socket produced for test inspection.
class MockTaggingClientSocketFactory : public MockClientSocketFactory {
 public:
  MockTaggingClientSocketFactory() = default;

  MockTaggingClientSocketFactory(const MockTaggingClientSocketFactory&) =
      delete;
  MockTaggingClientSocketFactory& operator=(
      const MockTaggingClientSocketFactory&) = delete;

  // ClientSocketFactory implementation.
  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override;
  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override;

  // These methods return pointers to last TCP and UDP sockets produced by this
  // factory. NOTE: Socket must still exist, or pointer will be to freed memory.
  MockTaggingStreamSocket* GetLastProducedTCPSocket() const {
    return tcp_socket_;
  }
  MockUDPClientSocket* GetLastProducedUDPSocket() const { return udp_socket_; }

 private:
  raw_ptr<MockTaggingStreamSocket, AcrossTasksDanglingUntriaged> tcp_socket_ =
      nullptr;
  raw_ptr<MockUDPClientSocket, AcrossTasksDanglingUntriaged> udp_socket_ =
      nullptr;
};

// Host / port used for SOCKS4 test strings.
extern const char kSOCKS4TestHost[];
extern const int kSOCKS4TestPort;

// Constants for a successful SOCKS v4 handshake (connecting to kSOCKS4TestHost
// on port kSOCKS4TestPort, for the request).
extern const char kSOCKS4OkRequestLocalHostPort80[];
extern const int kSOCKS4OkRequestLocalHostPort80Length;

extern const char kSOCKS4OkReply[];
extern const int kSOCKS4OkReplyLength;

// Host / port used for SOCKS5 test strings.
extern const char kSOCKS5TestHost[];
extern const int kSOCKS5TestPort;

// Constants for a successful SOCKS v5 handshake (connecting to kSOCKS5TestHost
// on port kSOCKS5TestPort, for the request)..
extern const char kSOCKS5GreetRequest[];
extern const int kSOCKS5GreetRequestLength;

extern const char kSOCKS5GreetResponse[];
extern const int kSOCKS5GreetResponseLength;

extern const char kSOCKS5OkRequest[];
extern const int kSOCKS5OkRequestLength;

extern const char kSOCKS5OkResponse[];
extern const int kSOCKS5OkResponseLength;

// Helper function to get the total data size of the MockReads in |reads|.
int64_t CountReadBytes(base::span<const MockRead> reads);

// Helper function to get the total data size of the MockWrites in |writes|.
int64_t CountWriteBytes(base::span<const MockWrite> writes);

#if BUILDFLAG(IS_ANDROID)
// Returns whether the device supports calling GetTaggedBytes().
bool CanGetTaggedBytes();

// Query the system to find out how many bytes were received with tag
// |expected_tag| for our UID.  Return the count of received bytes.
uint64_t GetTaggedBytes(int32_t expected_tag);
#endif

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TEST_UTIL_H_
