// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_transaction.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/url_util.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/dns_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_scoped_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1);

const char kMockHostname[] = "mock.http";

std::string DomainFromDot(const base::StringPiece& dotted) {
  std::string out;
  EXPECT_TRUE(DNSDomainFromDot(dotted, &out));
  return out;
}

enum class Transport { UDP, TCP, HTTPS };

// A SocketDataProvider builder.
class DnsSocketData {
 public:
  // The ctor takes parameters for the DnsQuery.
  DnsSocketData(uint16_t id,
                const char* dotted_name,
                uint16_t qtype,
                IoMode mode,
                Transport transport,
                const OptRecordRdata* opt_rdata = nullptr)
      : query_(new DnsQuery(id, DomainFromDot(dotted_name), qtype, opt_rdata)),
        transport_(transport) {
    if (Transport::TCP == transport_) {
      std::unique_ptr<uint16_t> length(new uint16_t);
      *length = base::HostToNet16(query_->io_buffer()->size());
      writes_.push_back(MockWrite(mode,
                                  reinterpret_cast<const char*>(length.get()),
                                  sizeof(uint16_t), num_reads_and_writes()));
      lengths_.push_back(std::move(length));
    }
    writes_.push_back(MockWrite(mode, query_->io_buffer()->data(),
                                query_->io_buffer()->size(),
                                num_reads_and_writes()));
  }
  ~DnsSocketData() = default;

  // All responses must be added before GetProvider.

  // Adds pre-built DnsResponse. |tcp_length| will be used in TCP mode only.
  void AddResponseWithLength(std::unique_ptr<DnsResponse> response,
                             IoMode mode,
                             uint16_t tcp_length) {
    CHECK(!provider_.get());
    if (Transport::TCP == transport_) {
      std::unique_ptr<uint16_t> length(new uint16_t);
      *length = base::HostToNet16(tcp_length);
      reads_.push_back(MockRead(mode,
                                reinterpret_cast<const char*>(length.get()),
                                sizeof(uint16_t), num_reads_and_writes()));
      lengths_.push_back(std::move(length));
    }
    reads_.push_back(MockRead(mode, response->io_buffer()->data(),
                              response->io_buffer_size(),
                              num_reads_and_writes()));
    responses_.push_back(std::move(response));
  }

  // Adds pre-built DnsResponse.
  void AddResponse(std::unique_ptr<DnsResponse> response, IoMode mode) {
    uint16_t tcp_length = response->io_buffer_size();
    AddResponseWithLength(std::move(response), mode, tcp_length);
  }

  // Adds pre-built response from |data| buffer.
  void AddResponseData(const uint8_t* data, size_t length, IoMode mode) {
    CHECK(!provider_.get());
    AddResponse(std::make_unique<DnsResponse>(
                    reinterpret_cast<const char*>(data), length, 0),
                mode);
  }

  // Adds pre-built response from |data| buffer.
  void AddResponseData(const uint8_t* data,
                       size_t length,
                       int offset,
                       IoMode mode) {
    CHECK(!provider_.get());
    AddResponse(
        std::make_unique<DnsResponse>(reinterpret_cast<const char*>(data),
                                      length - offset, offset),
        mode);
  }

  // Add no-answer (RCODE only) response matching the query.
  void AddRcode(int rcode, IoMode mode) {
    std::unique_ptr<DnsResponse> response(new DnsResponse(
        query_->io_buffer()->data(), query_->io_buffer()->size(), 0));
    dns_protocol::Header* header =
        reinterpret_cast<dns_protocol::Header*>(response->io_buffer()->data());
    header->flags |= base::HostToNet16(dns_protocol::kFlagResponse | rcode);
    AddResponse(std::move(response), mode);
  }

  // Add error response.
  void AddReadError(int error, IoMode mode) {
    reads_.push_back(MockRead(mode, error, num_reads_and_writes()));
  }

  // Build, if needed, and return the SocketDataProvider. No new responses
  // should be added afterwards.
  SequencedSocketData* GetProvider() {
    if (provider_.get())
      return provider_.get();
    // Terminate the reads with ERR_IO_PENDING to prevent overrun and default to
    // timeout.
    if (transport_ != Transport::HTTPS) {
      reads_.push_back(MockRead(SYNCHRONOUS, ERR_IO_PENDING,
                                writes_.size() + reads_.size()));
    }
    provider_.reset(new SequencedSocketData(reads_, writes_));
    if (Transport::TCP == transport_ || Transport::HTTPS == transport_) {
      provider_->set_connect_data(MockConnect(reads_[0].mode, OK));
    }
    return provider_.get();
  }

  uint16_t query_id() const { return query_->id(); }

  IOBufferWithSize* query_buffer() { return query_->io_buffer(); }

 private:
  size_t num_reads_and_writes() const { return reads_.size() + writes_.size(); }

  std::unique_ptr<DnsQuery> query_;
  Transport transport_;
  std::vector<std::unique_ptr<uint16_t>> lengths_;
  std::vector<std::unique_ptr<DnsResponse>> responses_;
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  std::unique_ptr<SequencedSocketData> provider_;

  DISALLOW_COPY_AND_ASSIGN(DnsSocketData);
};

class TestSocketFactory;

// A variant of MockUDPClientSocket which always fails to Connect.
class FailingUDPClientSocket : public MockUDPClientSocket {
 public:
  FailingUDPClientSocket(SocketDataProvider* data, net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log) {}
  ~FailingUDPClientSocket() override = default;
  int Connect(const IPEndPoint& endpoint) override {
    return ERR_CONNECTION_REFUSED;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingUDPClientSocket);
};

// A variant of MockUDPClientSocket which notifies the factory OnConnect.
class TestUDPClientSocket : public MockUDPClientSocket {
 public:
  TestUDPClientSocket(TestSocketFactory* factory,
                      SocketDataProvider* data,
                      net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log), factory_(factory) {}
  ~TestUDPClientSocket() override = default;
  int Connect(const IPEndPoint& endpoint) override;

 private:
  TestSocketFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(TestUDPClientSocket);
};

// Creates TestUDPClientSockets and keeps endpoints reported via OnConnect.
class TestSocketFactory : public MockClientSocketFactory {
 public:
  TestSocketFactory() : fail_next_socket_(false) {}
  ~TestSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    if (fail_next_socket_) {
      fail_next_socket_ = false;
      return std::unique_ptr<DatagramClientSocket>(
          new FailingUDPClientSocket(&empty_data_, net_log));
    }
    SocketDataProvider* data_provider = mock_data().GetNext();
    return std::make_unique<TestUDPClientSocket>(this, data_provider, net_log);
  }

  void OnConnect(const IPEndPoint& endpoint) {
    remote_endpoints_.push_back(endpoint);
  }

  std::vector<IPEndPoint> remote_endpoints_;
  bool fail_next_socket_;

 private:
  StaticSocketDataProvider empty_data_;

  DISALLOW_COPY_AND_ASSIGN(TestSocketFactory);
};

int TestUDPClientSocket::Connect(const IPEndPoint& endpoint) {
  factory_->OnConnect(endpoint);
  return MockUDPClientSocket::Connect(endpoint);
}

// Helper class that holds a DnsTransaction and handles OnTransactionComplete.
class TransactionHelper {
 public:
  // If |expected_answer_count| < 0 then it is the expected net error.
  TransactionHelper(const char* hostname,
                    uint16_t qtype,
                    int expected_answer_count)
      : hostname_(hostname),
        qtype_(qtype),
        response_(nullptr),
        expected_answer_count_(expected_answer_count),
        cancel_in_callback_(false),
        completed_(false) {}

  // Mark that the transaction shall be destroyed immediately upon callback.
  void set_cancel_in_callback() { cancel_in_callback_ = true; }

  void StartTransaction(DnsTransactionFactory* factory) {
    EXPECT_EQ(NULL, transaction_.get());
    transaction_ = factory->CreateTransaction(
        hostname_, qtype_,
        base::Bind(&TransactionHelper::OnTransactionComplete,
                   base::Unretained(this)),
        NetLogWithSource::Make(&net_log_, net::NetLogSourceType::NONE));
    transaction_->SetRequestContext(&request_context_);
    transaction_->SetRequestPriority(DEFAULT_PRIORITY);
    EXPECT_EQ(hostname_, transaction_->GetHostname());
    EXPECT_EQ(qtype_, transaction_->GetType());
    transaction_->Start();
  }

  void Cancel() {
    ASSERT_TRUE(transaction_.get() != NULL);
    transaction_.reset(NULL);
  }

  void OnTransactionComplete(DnsTransaction* t,
                             int rv,
                             const DnsResponse* response) {
    EXPECT_FALSE(completed_);
    EXPECT_EQ(transaction_.get(), t);

    completed_ = true;
    response_ = response;

    if (transaction_complete_run_loop_)
      transaction_complete_run_loop_->QuitWhenIdle();

    if (cancel_in_callback_) {
      Cancel();
      return;
    }

    if (response)
      EXPECT_TRUE(response->IsValid());

    if (expected_answer_count_ >= 0) {
      ASSERT_THAT(rv, IsOk());
      ASSERT_TRUE(response != NULL);
      EXPECT_EQ(static_cast<unsigned>(expected_answer_count_),
                response->answer_count());
      EXPECT_EQ(qtype_, response->qtype());

      DnsRecordParser parser = response->Parser();
      DnsResourceRecord record;
      for (int i = 0; i < expected_answer_count_; ++i) {
        EXPECT_TRUE(parser.ReadRecord(&record));
      }
    } else {
      EXPECT_EQ(expected_answer_count_, rv);
    }
  }

  bool has_completed() const { return completed_; }
  const DnsResponse* response() const { return response_; }

  // Shorthands for commonly used commands.

  bool Run(DnsTransactionFactory* factory) {
    StartTransaction(factory);
    base::RunLoop().RunUntilIdle();
    return has_completed();
  }

  bool RunUntilDone(DnsTransactionFactory* factory) {
    DCHECK(!transaction_complete_run_loop_);
    transaction_complete_run_loop_ = std::make_unique<base::RunLoop>();
    StartTransaction(factory);
    transaction_complete_run_loop_->Run();
    transaction_complete_run_loop_.reset();
    return has_completed();
  }

  TestURLRequestContext* request_context() { return &request_context_; }

  NetLog* net_log() { return &net_log_; }

 private:
  std::string hostname_;
  uint16_t qtype_;
  std::unique_ptr<DnsTransaction> transaction_;
  const DnsResponse* response_;
  int expected_answer_count_;
  bool cancel_in_callback_;
  TestURLRequestContext request_context_;
  std::unique_ptr<base::RunLoop> transaction_complete_run_loop_;
  bool completed_;
  NetLog net_log_;
};

// Callback that allows a test to modify HttpResponseinfo
// before the response is sent to the requester. This allows
// response headers to be changed.
typedef base::RepeatingCallback<void(URLRequest* request,
                                     HttpResponseInfo* info)>
    ResponseModifierCallback;

// Callback that allows the test to substitute its own implementation
// of URLRequestJob to handle the request.
typedef base::RepeatingCallback<URLRequestJob*(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    SocketDataProvider* data_provider)>
    DohJobMakerCallback;

// Subclass of URLRequestJob which takes a SocketDataProvider with data
// representing both a DNS over HTTPS query and response.
class URLRequestMockDohJob : public URLRequestJob, public AsyncSocket {
 public:
  URLRequestMockDohJob(
      URLRequest* request,
      NetworkDelegate* network_delegate,
      SocketDataProvider* data_provider,
      ResponseModifierCallback response_modifier = ResponseModifierCallback())
      : URLRequestJob(request, network_delegate),
        content_length_(0),
        leftover_data_len_(0),
        data_provider_(data_provider),
        response_modifier_(response_modifier),
        weak_factory_(this) {
    data_provider_->Initialize(this);
    MatchQueryData(request, data_provider);
  }

  // Compare the query contained in either the POST body or the body
  // parameter of the GET query to the write data of the SocketDataProvider.
  static void MatchQueryData(URLRequest* request,
                             SocketDataProvider* data_provider) {
    std::string decoded_query;
    if (request->method() == "GET") {
      std::string encoded_query;
      EXPECT_TRUE(GetValueForKeyInQuery(request->url(), "dns", &encoded_query));
      EXPECT_GT(encoded_query.size(), 0ul);

      EXPECT_TRUE(base::Base64UrlDecode(
          encoded_query, base::Base64UrlDecodePolicy::IGNORE_PADDING,
          &decoded_query));
    } else if (request->method() == "POST") {
      const UploadDataStream* stream = request->get_upload();
      auto* readers = stream->GetElementReaders();
      EXPECT_TRUE(readers);
      EXPECT_FALSE(readers->empty());
      for (auto& reader : *readers) {
        const UploadBytesElementReader* byte_reader = reader->AsBytesReader();
        decoded_query +=
            std::string(byte_reader->bytes(), byte_reader->length());
      }
    }

    std::string query(decoded_query);
    MockWriteResult result(SYNCHRONOUS, 1);
    while (result.result > 0 && query.length() > 0) {
      result = data_provider->OnWrite(query);
      if (result.result > 0)
        query = query.substr(result.result);
    }
  }

  static std::string GetMockHttpsUrl(const std::string& path) {
    return "https://" + (kMockHostname + ("/" + path));
  }

  // URLRequestJob implementation:
  void Start() override {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&URLRequestMockDohJob::StartAsync,
                              weak_factory_.GetWeakPtr()));
  }

  ~URLRequestMockDohJob() override {
    if (data_provider_)
      data_provider_->DetachSocket();
  }

  int ReadRawData(IOBuffer* buf, int buf_size) override {
    if (!data_provider_)
      return ERR_FAILED;
    if (leftover_data_len_ > 0) {
      int rv = DoBufferCopy(leftover_data_, leftover_data_len_, buf, buf_size);
      return rv;
    }

    if (data_provider_->AllReadDataConsumed())
      return 0;

    MockRead read = data_provider_->OnRead();

    if (read.result < ERR_IO_PENDING)
      return read.result;

    if (read.result == ERR_IO_PENDING) {
      pending_buf_ = buf;
      pending_buf_size_ = buf_size;
      return ERR_IO_PENDING;
    }
    return DoBufferCopy(read.data, read.data_len, buf, buf_size);
  }

  void GetResponseInfo(HttpResponseInfo* info) override {
    // Send back mock headers.
    std::string raw_headers;
    raw_headers.append(
        "HTTP/1.1 200 OK\n"
        "Content-type: application/dns-message\n");
    if (content_length_ > 0) {
      raw_headers.append(base::StringPrintf("Content-Length: %1d\n",
                                            static_cast<int>(content_length_)));
    }
    info->headers =
        base::MakeRefCounted<HttpResponseHeaders>(HttpUtil::AssembleRawHeaders(
            raw_headers.c_str(), static_cast<int>(raw_headers.length())));
    if (response_modifier_)
      response_modifier_.Run(request(), info);
  }

  // AsyncSocket implementation:
  void OnReadComplete(const MockRead& data) override {
    EXPECT_NE(data.result, ERR_IO_PENDING);
    if (data.result < 0)
      return ReadRawDataComplete(data.result);
    ReadRawDataComplete(DoBufferCopy(data.data, data.data_len, pending_buf_,
                                     pending_buf_size_));
  }
  void OnWriteComplete(int rv) override {}
  void OnConnectComplete(const MockConnect& data) override {}
  void OnDataProviderDestroyed() override { data_provider_ = nullptr; }

 private:
  void StartAsync() {
    if (!request_)
      return;
    if (content_length_)
      set_expected_content_size(content_length_);
    NotifyHeadersComplete();
  }

  int DoBufferCopy(const char* data,
                   int data_len,
                   IOBuffer* buf,
                   int buf_size) {
    if (data_len > buf_size) {
      memcpy(buf->data(), data, buf_size);
      leftover_data_ = data + buf_size;
      leftover_data_len_ = data_len - buf_size;
      return buf_size;
    }
    memcpy(buf->data(), data, data_len);
    return data_len;
  }

  const int content_length_;
  const char* leftover_data_;
  int leftover_data_len_;
  SocketDataProvider* data_provider_;
  const ResponseModifierCallback response_modifier_;
  IOBuffer* pending_buf_;
  int pending_buf_size_;

  base::WeakPtrFactory<URLRequestMockDohJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestMockDohJob);
};

class DnsTransactionTestBase : public testing::Test {
 public:
  DnsTransactionTestBase() = default;
  ~DnsTransactionTestBase() override = default;

  // Generates |nameservers| for DnsConfig.
  void ConfigureNumServers(unsigned num_servers) {
    CHECK_LE(num_servers, 255u);
    config_.nameservers.clear();
    for (unsigned i = 0; i < num_servers; ++i) {
      config_.nameservers.push_back(
          IPEndPoint(IPAddress(192, 168, 1, i), dns_protocol::kDefaultPort));
    }
  }

  // Called after fully configuring |config|.
  void ConfigureFactory() {
    socket_factory_.reset(new TestSocketFactory());
    session_ = new DnsSession(
        config_,
        DnsSocketPool::CreateNull(socket_factory_.get(),
                                  base::Bind(base::RandInt)),
        base::Bind(&DnsTransactionTestBase::GetNextId, base::Unretained(this)),
        NULL /* NetLog */);
    transaction_factory_ = DnsTransactionFactory::CreateFactory(session_.get());
  }

  void AddSocketData(std::unique_ptr<DnsSocketData> data) {
    CHECK(socket_factory_.get());
    transaction_ids_.push_back(data->query_id());
    socket_factory_->AddSocketDataProvider(data->GetProvider());
    socket_data_.push_back(std::move(data));
  }

  // Add expected query for |dotted_name| and |qtype| with |id| and response
  // taken verbatim from |data| of |data_length| bytes. The transaction id in
  // |data| should equal |id|, unless testing mismatched response.
  void AddQueryAndResponse(uint16_t id,
                           const char* dotted_name,
                           uint16_t qtype,
                           const uint8_t* response_data,
                           size_t response_length,
                           IoMode mode,
                           Transport transport,
                           const OptRecordRdata* opt_rdata = nullptr) {
    CHECK(socket_factory_.get());
    std::unique_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, mode, transport, opt_rdata));
    data->AddResponseData(response_data, response_length, mode);
    AddSocketData(std::move(data));
  }

  void AddQueryAndErrorResponse(uint16_t id,
                                const char* dotted_name,
                                uint16_t qtype,
                                int error,
                                IoMode mode,
                                Transport transport) {
    CHECK(socket_factory_.get());
    std::unique_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, mode, transport));
    data->AddReadError(error, mode);
    AddSocketData(std::move(data));
  }

  void AddAsyncQueryAndResponse(uint16_t id,
                                const char* dotted_name,
                                uint16_t qtype,
                                const uint8_t* data,
                                size_t data_length,
                                const OptRecordRdata* opt_rdata = nullptr) {
    AddQueryAndResponse(id, dotted_name, qtype, data, data_length, ASYNC,
                        Transport::UDP, opt_rdata);
  }

  void AddSyncQueryAndResponse(uint16_t id,
                               const char* dotted_name,
                               uint16_t qtype,
                               const uint8_t* data,
                               size_t data_length,
                               const OptRecordRdata* opt_rdata = nullptr) {
    AddQueryAndResponse(id, dotted_name, qtype, data, data_length, SYNCHRONOUS,
                        Transport::UDP, opt_rdata);
  }

  // Add expected query of |dotted_name| and |qtype| and no response.
  void AddQueryAndTimeout(const char* dotted_name, uint16_t qtype) {
    uint16_t id = base::RandInt(0, std::numeric_limits<uint16_t>::max());
    std::unique_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, ASYNC, Transport::UDP));
    AddSocketData(std::move(data));
  }

  // Add expected query of |dotted_name| and |qtype| and matching response with
  // no answer and RCODE set to |rcode|. The id will be generated randomly.
  void AddQueryAndRcode(const char* dotted_name,
                        uint16_t qtype,
                        int rcode,
                        IoMode mode,
                        Transport trans) {
    CHECK_NE(dns_protocol::kRcodeNOERROR, rcode);
    uint16_t id = base::RandInt(0, std::numeric_limits<uint16_t>::max());
    std::unique_ptr<DnsSocketData> data(
        new DnsSocketData(id, dotted_name, qtype, mode, trans));
    data->AddRcode(rcode, mode);
    AddSocketData(std::move(data));
  }

  void AddAsyncQueryAndRcode(const char* dotted_name,
                             uint16_t qtype,
                             int rcode) {
    AddQueryAndRcode(dotted_name, qtype, rcode, ASYNC, Transport::UDP);
  }

  void AddSyncQueryAndRcode(const char* dotted_name,
                            uint16_t qtype,
                            int rcode) {
    AddQueryAndRcode(dotted_name, qtype, rcode, SYNCHRONOUS, Transport::UDP);
  }

  // Checks if the sockets were connected in the order matching the indices in
  // |servers|.
  void CheckServerOrder(const unsigned* servers, size_t num_attempts) {
    ASSERT_EQ(num_attempts, socket_factory_->remote_endpoints_.size());
    for (size_t i = 0; i < num_attempts; ++i) {
      EXPECT_EQ(socket_factory_->remote_endpoints_[i],
                session_->config().nameservers[servers[i]]);
    }
  }

  void SetUp() override {
    // By default set one server,
    ConfigureNumServers(1);
    // and no retransmissions,
    config_.attempts = 1;
    // and an arbitrary timeout.
    config_.timeout = kTimeout;
    ConfigureFactory();
  }

  void TearDown() override {
    // Check that all socket data was at least written to.
    for (size_t i = 0; i < socket_data_.size(); ++i) {
      EXPECT_TRUE(socket_data_[i]->GetProvider()->AllWriteDataConsumed()) << i;
    }
  }

 protected:
  int GetNextId(int min, int max) {
    EXPECT_FALSE(transaction_ids_.empty());
    int id = transaction_ids_.front();
    transaction_ids_.pop_front();
    EXPECT_GE(id, min);
    EXPECT_LE(id, max);
    return id;
  }

  DnsConfig config_;

  std::vector<std::unique_ptr<DnsSocketData>> socket_data_;

  base::circular_deque<int> transaction_ids_;
  std::unique_ptr<TestSocketFactory> socket_factory_;
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> transaction_factory_;
};

class DnsTransactionTest : public DnsTransactionTestBase,
                           public WithScopedTaskEnvironment {
 public:
  DnsTransactionTest() = default;
  ~DnsTransactionTest() override = default;

  // Generates |nameservers| for DnsConfig.
  void ConfigureDohServers(unsigned num_servers, bool use_post) {
    CHECK_LE(num_servers, 255u);
    for (unsigned i = 0; i < num_servers; ++i) {
      std::string server_template(URLRequestMockDohJob::GetMockHttpsUrl(
                                      base::StringPrintf("doh_test_%d", i)) +
                                  "{?dns}");
      config_.dns_over_https_servers.push_back(
          DnsConfig::DnsOverHttpsServerConfig(server_template, use_post));
    }
  }

  // Configures the DnsConfig with one dns over https server, which either
  // accepts GET or POST requests based on use_post. If |clear_udp| is true,
  // existing IP name servers are removed from the DnsConfig. If a
  // ResponseModifierCallback is provided it will be called to contruct the
  // HTTPResponse.
  void ConfigDohServers(bool clear_udp,
                        bool use_post,
                        int num_doh_servers = 1) {
    if (clear_udp)
      ConfigureNumServers(0);
    GURL url(URLRequestMockDohJob::GetMockHttpsUrl("doh_test"));
    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->AddHostnameInterceptor(url.scheme(), url.host(),
                                   std::make_unique<DohJobInterceptor>(this));
    ConfigureDohServers(num_doh_servers, use_post);
    ConfigureFactory();
  }

  URLRequestJob* MaybeInterceptRequest(URLRequest* request,
                                       NetworkDelegate* network_delegate) {
    // If the path indicates a redirct, skip checking the list of
    // configured servers, because it won't be there and we still want
    // to handle it.
    bool server_found = request->url().path() == "/redirect-destination";
    for (auto server : config_.dns_over_https_servers) {
      if (server_found)
        break;
      std::string url_base =
          GetURLFromTemplateWithoutParameters(server.server_template);
      if (server.use_post && request->method() == "POST") {
        if (url_base == request->url().spec()) {
          server_found = true;
        }
      } else if (!server.use_post && request->method() == "GET") {
        std::string prefix = url_base + "?dns=";
        auto mispair = std::mismatch(prefix.begin(), prefix.end(),
                                     request->url().spec().begin());
        if (mispair.first == prefix.end()) {
          server_found = true;
        }
      }
    }
    EXPECT_TRUE(server_found);

    HttpRequestHeaders* headers = nullptr;
    if (request->GetFullRequestHeaders(headers)) {
      EXPECT_FALSE(headers->HasHeader(HttpRequestHeaders::kCookie));
    }
    EXPECT_FALSE(request->extra_request_headers().HasHeader(
        HttpRequestHeaders::kCookie));

    std::string accept;
    EXPECT_TRUE(request->extra_request_headers().GetHeader("Accept", &accept));
    EXPECT_EQ(accept, "application/dns-message");

    SocketDataProvider* provider = socket_factory_->mock_data().GetNext();

    if (doh_job_maker_)
      return doh_job_maker_.Run(request, network_delegate, provider);

    return new URLRequestMockDohJob(request, network_delegate, provider,
                                    response_modifier_);
  }

  class DohJobInterceptor : public URLRequestInterceptor {
   public:
    explicit DohJobInterceptor(DnsTransactionTest* test) : test_(test) {}
    ~DohJobInterceptor() override {}

    // URLRequestInterceptor implementation:
    URLRequestJob* MaybeInterceptRequest(
        URLRequest* request,
        NetworkDelegate* network_delegate) const override {
      return test_->MaybeInterceptRequest(request, network_delegate);
    }

   private:
    DnsTransactionTest* test_;

    DISALLOW_COPY_AND_ASSIGN(DohJobInterceptor);
  };

  void TearDown() override {
    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->ClearHandlers();
  }

  void SetResponseModifierCallback(ResponseModifierCallback response_modifier) {
    response_modifier_ = response_modifier;
  }

  void SetDohJobMakerCallback(DohJobMakerCallback doh_job_maker) {
    doh_job_maker_ = doh_job_maker;
  }

 private:
  ResponseModifierCallback response_modifier_;
  DohJobMakerCallback doh_job_maker_;
};

class DnsTransactionTestWithMockTime : public DnsTransactionTestBase,
                                       public WithScopedTaskEnvironment {
 protected:
  DnsTransactionTestWithMockTime()
      : WithScopedTaskEnvironment(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {}
  ~DnsTransactionTestWithMockTime() override = default;
};

TEST_F(DnsTransactionTest, Lookup) {
  base::HistogramTester histograms;
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
  histograms.ExpectUniqueSample("AsyncDNS.Rcode", dns_protocol::kRcodeNOERROR,
                                1);
}

TEST_F(DnsTransactionTest, LookupWithEDNSOption) {
  OptRecordRdata expected_opt_rdata;

  const OptRecordRdata::Opt ednsOpt(123, "\xbe\xef");
  transaction_factory_->AddEDNSOption(ednsOpt);
  expected_opt_rdata.AddOpt(ednsOpt);

  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                           &expected_opt_rdata);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, LookupWithMultipleEDNSOptions) {
  OptRecordRdata expected_opt_rdata;

  for (const auto& ednsOpt : {
           // Two options with the same code, to check that both are included.
           OptRecordRdata::Opt(1, "\xde\xad"),
           OptRecordRdata::Opt(1, "\xbe\xef"),
           // Try a different code and different length of data.
           OptRecordRdata::Opt(2, "\xff"),
       }) {
    transaction_factory_->AddEDNSOption(ednsOpt);
    expected_opt_rdata.AddOpt(ednsOpt);
  }

  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                           &expected_opt_rdata);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

// Concurrent lookup tests assume that DnsTransaction::Start immediately
// consumes a socket from ClientSocketFactory.
TEST_F(DnsTransactionTest, ConcurrentLookup) {
  base::HistogramTester histograms;
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram, arraysize(kT1ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
  histograms.ExpectUniqueSample("AsyncDNS.Rcode", dns_protocol::kRcodeNOERROR,
                                2);
}

TEST_F(DnsTransactionTest, CancelLookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram, arraysize(kT1ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get());

  helper0.Cancel();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DestroyFactory) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get());

  // Destroying the client does not affect running requests.
  transaction_factory_.reset(NULL);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTest, CancelFromCallback) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  helper0.set_cancel_in_callback();
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseSync) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response synchronously.
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  data->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                        SYNCHRONOUS);
  AddSocketData(std::move(data));

  // Second attempt receives valid response synchronously.
  std::unique_ptr<DnsSocketData> data1(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  data1->AddResponseData(kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                         SYNCHRONOUS);
  AddSocketData(std::move(data1));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseAsync) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response asynchronously.
  std::unique_ptr<DnsSocketData> data0(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::UDP));
  data0->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                         ASYNC);
  AddSocketData(std::move(data0));

  // Second attempt receives valid response asynchronously.
  std::unique_ptr<DnsSocketData> data1(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::UDP));
  data1->AddResponseData(kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                         ASYNC);
  AddSocketData(std::move(data1));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseFail) {
  ConfigureFactory();

  // Attempt receives mismatched response and fails because only one attempt is
  // allowed.
  AddAsyncQueryAndResponse(1 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedResponseNxdomain) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response followed by valid NXDOMAIN
  // response.
  // Second attempt receives valid NXDOMAIN response.
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  data->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                        SYNCHRONOUS);
  data->AddRcode(dns_protocol::kRcodeNXDOMAIN, ASYNC);
  AddSocketData(std::move(data));
  AddSyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ServerFail) {
  base::HistogramTester histograms;
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
  histograms.ExpectUniqueSample("AsyncDNS.Rcode", dns_protocol::kRcodeSERVFAIL,
                                1);
}

TEST_F(DnsTransactionTest, NoDomain) {
  base::HistogramTester histograms;
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
  histograms.ExpectUniqueSample("AsyncDNS.Rcode", dns_protocol::kRcodeNXDOMAIN,
                                1);
}

TEST_F(DnsTransactionTestWithMockTime, Timeout) {
  config_.attempts = 3;
  ConfigureFactory();

  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_TIMED_OUT);

  // Finish when the third attempt times out.
  EXPECT_FALSE(helper0.Run(transaction_factory_.get()));
  FastForwardBy(session_->NextTimeout(0, 0));
  EXPECT_FALSE(helper0.has_completed());
  FastForwardBy(session_->NextTimeout(0, 1));
  EXPECT_FALSE(helper0.has_completed());
  FastForwardBy(session_->NextTimeout(0, 2));
  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTestWithMockTime, ServerFallbackAndRotate) {
  // Test that we fallback on both server failure and timeout.
  config_.attempts = 2;
  // The next request should start from the next server.
  config_.rotate = true;
  ConfigureNumServers(3);
  ConfigureFactory();

  // Responses for first request.
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second request.
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  TransactionHelper helper1(kT1HostName, kT1Qtype, ERR_NAME_NOT_RESOLVED);

  EXPECT_FALSE(helper0.Run(transaction_factory_.get()));
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(helper0.has_completed());
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  unsigned kOrder[] = {
      0, 1, 2, 0, 1,  // The first transaction.
      1, 2, 0,        // The second transaction starts from the next server.
  };
  CheckServerOrder(kOrder, arraysize(kOrder));
}

TEST_F(DnsTransactionTest, SuffixSearchAboveNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.rotate = true;
  ConfigureNumServers(2);
  ConfigureFactory();

  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // Also check if suffix search causes server rotation.
  unsigned kOrder0[] = {0, 1, 0, 1};
  CheckServerOrder(kOrder0, arraysize(kOrder0));
}

TEST_F(DnsTransactionTest, SuffixSearchBelowNdots) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncQueryAndRcode("x.y.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncQueryAndRcode("x.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncQueryAndRcode("x", dns_protocol::kTypeAAAA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single-label name.
  TransactionHelper helper1("x", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  // A fully-qualified name.
  TransactionHelper helper2("x.", dns_protocol::kTypeAAAA,
                            ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, EmptySuffixSearch) {
  // Responses for first transaction.
  AddAsyncQueryAndRcode("x", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  // A fully-qualified name.
  TransactionHelper helper0("x.", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  // A single label name is not even attempted.
  TransactionHelper helper1("singlelabel", dns_protocol::kTypeA,
                            ERR_DNS_SEARCH_EMPTY);

  helper1.Run(transaction_factory_.get());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DontAppendToMultiLabelName) {
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  config_.append_to_multi_label_name = false;
  ConfigureFactory();

  // Responses for first transaction.
  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for second transaction.
  AddAsyncQueryAndRcode("x.y", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  // Responses for third transaction.
  AddAsyncQueryAndRcode("x.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.b", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.c", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA,
                            ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  TransactionHelper helper1("x.y", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));

  TransactionHelper helper2("x", dns_protocol::kTypeA, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper2.Run(transaction_factory_.get()));
}

const uint8_t kResponseNoData[] = {
    0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    // Question
    0x01, 'x', 0x01, 'y', 0x01, 'z', 0x01, 'b', 0x00, 0x00, 0x01, 0x00, 0x01,
    // Authority section, SOA record, TTL 0x3E6
    0x01, 'z', 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x00, 0x03, 0xE6,
    // Minimal RDATA, 18 bytes
    0x00, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TEST_F(DnsTransactionTest, SuffixSearchStop) {
  config_.ndots = 2;
  config_.search.push_back("a");
  config_.search.push_back("b");
  config_.search.push_back("c");
  ConfigureFactory();

  AddAsyncQueryAndRcode("x.y.z", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndRcode("x.y.z.a", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddAsyncQueryAndResponse(0 /* id */, "x.y.z.b", dns_protocol::kTypeA,
                           kResponseNoData, arraysize(kResponseNoData));

  TransactionHelper helper0("x.y.z", dns_protocol::kTypeA, 0 /* answers */);

  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                          kT0ResponseDatagram, arraysize(kT0ResponseDatagram));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncFirstQueryWithSearch) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndRcode("www.lab.ccs.neu.edu", kT2Qtype,
                       dns_protocol::kRcodeNXDOMAIN);
  // "www.ccs.neu.edu"
  AddAsyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                           kT2ResponseDatagram, arraysize(kT2ResponseDatagram));

  TransactionHelper helper0("www", kT2Qtype, kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, SyncSearchQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddAsyncQueryAndRcode("www.lab.ccs.neu.edu", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddSyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                          kT2ResponseDatagram, arraysize(kT2ResponseDatagram));

  TransactionHelper helper0("www", kT2Qtype, kT2RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ConnectFailure) {
  socket_factory_->fail_next_socket_ = true;
  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  TransactionHelper helper0("www.chromium.org", dns_protocol::kTypeA,
                            ERR_CONNECTION_REFUSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, ConnectFailureFollowedBySuccess) {
  // Retry after server failure.
  config_.attempts = 2;
  ConfigureFactory();
  // First server connection attempt fails.
  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  socket_factory_->fail_next_socket_ = true;
  // Second DNS query succeeds.
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsGetLookup) {
  ConfigDohServers(true /* clear_udp */, false /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsGetFailure) {
  ConfigDohServers(true /* clear_udp */, false /* use_post */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, HttpsGetMalformed) {
  ConfigDohServers(true /* clear_udp */, false /* use_post */);
  AddQueryAndResponse(1 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookup) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostFailure) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, HttpsPostMalformed) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(1 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsync) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

URLRequestJob* DohJobMakerCallbackFailStart(URLRequest* request,
                                            NetworkDelegate* network_delegate,
                                            SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return new URLRequestFailedJob(request, network_delegate,
                                 URLRequestFailedJob::START, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailStart) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

URLRequestJob* DohJobMakerCallbackFailSync(URLRequest* request,
                                           NetworkDelegate* network_delegate,
                                           SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return new URLRequestFailedJob(request, network_delegate,
                                 URLRequestFailedJob::READ_SYNC, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailSync) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseWithLength(std::make_unique<DnsResponse>(), SYNCHRONOUS, 0);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailSync));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

URLRequestJob* DohJobMakerCallbackFailAsync(URLRequest* request,
                                            NetworkDelegate* network_delegate,
                                            SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return new URLRequestFailedJob(request, network_delegate,
                                 URLRequestFailedJob::READ_ASYNC, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailAsync) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailAsync));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookup2Sync) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, SYNCHRONOUS);
  data->AddResponseData(kT0ResponseDatagram + 20,
                        arraysize(kT0ResponseDatagram) - 20, SYNCHRONOUS);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookup2Async) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, ASYNC);
  data->AddResponseData(kT0ResponseDatagram + 20,
                        arraysize(kT0ResponseDatagram) - 20, ASYNC);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncWithAsyncZeroRead) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                        ASYNC);
  data->AddResponseData(kT0ResponseDatagram, 0, ASYNC);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncWithAsyncZeroRead) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, arraysize(kT0ResponseDatagram),
                        SYNCHRONOUS);
  data->AddResponseData(kT0ResponseDatagram, 0, ASYNC);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenSync) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, ASYNC);
  data->AddResponseData(kT0ResponseDatagram + 20,
                        arraysize(kT0ResponseDatagram) - 20, SYNCHRONOUS);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenSyncError) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, ASYNC);
  data->AddReadError(ERR_FAILED, SYNCHRONOUS);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenAsyncError) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, ASYNC);
  data->AddReadError(ERR_FAILED, ASYNC);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncThenAsyncError) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, SYNCHRONOUS);
  data->AddReadError(ERR_FAILED, ASYNC);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncThenSyncError) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS));
  data->AddResponseData(kT0ResponseDatagram, 20, SYNCHRONOUS);
  data->AddReadError(ERR_FAILED, SYNCHRONOUS);
  AddSocketData(std::move(data));
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostFailThenUDPFallback) {
  config_.attempts = 2;
  ConfigDohServers(false /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::UDP);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostFailThenUDPFailThenUDPFallback) {
  ConfigureNumServers(3);
  ConfigDohServers(false /* clear_udp */, true /* use_post */);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));

  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndTimeout(kT0HostName, kT0Qtype);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::UDP);

  transaction_ids_.push_back(0);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));

  // Servers 3 (HTTP) and 0 (UDP) should be marked as bad. 1 and 2 should be
  // good.
  EXPECT_EQ(session_->NextGoodServerIndex(0), 1u);
  EXPECT_EQ(session_->NextGoodServerIndex(1), 1u);
  EXPECT_EQ(session_->NextGoodServerIndex(2), 2u);
}

TEST_F(DnsTransactionTest, HttpsMarkUdpBad) {
  config_.attempts = 1;
  ConfigureNumServers(2);
  ConfigDohServers(false /* clear_udp */, true /* use_post */);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::UDP);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::UDP);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  // Server 0 (UDP) should be marked bad. Server 1 (UDP) should be good
  // and since 2 is our only Doh server, it will be good.
  EXPECT_EQ(session_->NextGoodServerIndex(0), 1u);
  EXPECT_EQ(session_->NextGoodServerIndex(1), 1u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(2), 2u);

  AddQueryAndErrorResponse(1, kT1HostName, kT1Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);
  AddQueryAndErrorResponse(1, kT1HostName, kT1Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::UDP);

  AddQueryAndResponse(1, kT1HostName, kT1Qtype, kT1ResponseDatagram,
                      arraysize(kT1ResponseDatagram), ASYNC, Transport::UDP);

  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  EXPECT_TRUE(helper1.RunUntilDone(transaction_factory_.get()));
  // Since 0 was bad to start, we started with 1 which will now be the
  // most recent failure, so Server 1 (UDP) should be marked bad.
  // Server 0 (UDP) should be good and since 2 is our only Doh server.
  EXPECT_EQ(session_->NextGoodServerIndex(0), 0u);
  EXPECT_EQ(session_->NextGoodServerIndex(1), 0u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(2), 2u);
}

TEST_F(DnsTransactionTest, HttpsMarkHttpsBad) {
  config_.attempts = 1;
  ConfigDohServers(false /* clear_udp */, true /* use_post */, 3);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::HTTPS);
  AddQueryAndErrorResponse(1, kT1HostName, kT1Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);
  AddQueryAndErrorResponse(1, kT1HostName, kT1Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS);

  AddQueryAndResponse(1, kT1HostName, kT1Qtype, kT1ResponseDatagram,
                      arraysize(kT1ResponseDatagram), ASYNC, Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  // Server 0 is our only UDP server, so it will be good. HTTPS
  // servers 1 and 2 failed and will be marked bad. Server 3 succeeded
  // so will be good.
  EXPECT_EQ(session_->NextGoodServerIndex(0), 0u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(1), 3u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(2), 3u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(3), 3u);

  EXPECT_TRUE(helper1.RunUntilDone(transaction_factory_.get()));
  // Server 0 is still our only UDP server, so will be good by definition.
  // Server 3 started out as good, so was tried first and failed. Server 1
  // then had the oldest failure so would be the next good server and
  // failed so is marked bad. Next attempt was server 2, which succeded so is
  // good.
  EXPECT_EQ(session_->NextGoodServerIndex(0), 0u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(1), 2u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(2), 2u);
  EXPECT_EQ(session_->NextGoodDnsOverHttpsServerIndex(3), 2u);
}

TEST_F(DnsTransactionTest, HttpsPostFailThenHTTPFallback) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */, 2);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL, ASYNC,
                   Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostFailTwiceThenUDPFallback) {
  config_.attempts = 3;
  ConfigDohServers(false /* clear_udp */, true /* use_post */, 2);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::UDP);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsPostFailTwice) {
  config_.attempts = 2;
  ConfigDohServers(true /* clear_udp */, true /* use_post */, 2);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_FAILED);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseWithCookie(URLRequest* request, HttpResponseInfo* info) {
  info->headers->AddHeader("Set-Cookie: test-cookie=you-fail");
}

class CookieCallback {
 public:
  CookieCallback()
      : result_(false), loop_to_quit_(std::make_unique<base::RunLoop>()) {}

  void SetCookieCallback(bool result) {
    result_ = result;
    loop_to_quit_->Quit();
  }

  void GetAllCookiesCallback(const net::CookieList& list) {
    list_ = list;
    loop_to_quit_->Quit();
  }

  void Reset() { loop_to_quit_.reset(new base::RunLoop()); }

  void WaitUntilDone() { loop_to_quit_->Run(); }

  size_t cookie_list_size() { return list_.size(); }

 private:
  net::CookieList list_;
  bool result_;
  std::unique_ptr<base::RunLoop> loop_to_quit_;
  DISALLOW_COPY_AND_ASSIGN(CookieCallback);
};

TEST_F(DnsTransactionTest, HttpsPostTestNoCookies) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(1, kT1HostName, kT1Qtype, kT1ResponseDatagram,
                      arraysize(kT1ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  TransactionHelper helper1(kT1HostName, kT1Qtype, kT1RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWithCookie));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));

  CookieCallback callback;
  helper0.request_context()->cookie_store()->GetAllCookiesForURLAsync(
      GURL(GetURLFromTemplateWithoutParameters(
          config_.dns_over_https_servers[0].server_template)),
      base::Bind(&CookieCallback::GetAllCookiesCallback,
                 base::Unretained(&callback)));
  callback.WaitUntilDone();
  EXPECT_EQ(0u, callback.cookie_list_size());
  callback.Reset();
  net::CookieOptions options;
  helper1.request_context()->cookie_store()->SetCookieWithOptionsAsync(
      GURL(GetURLFromTemplateWithoutParameters(
          config_.dns_over_https_servers[0].server_template)),
      "test-cookie=you-still-fail", options,
      base::Bind(&CookieCallback::SetCookieCallback,
                 base::Unretained(&callback)));
  EXPECT_TRUE(helper1.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseWithoutLength(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Length");
}

TEST_F(DnsTransactionTest, HttpsPostNoContentLength) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWithoutLength));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseWithBadRequestResponse(URLRequest* request,
                                        HttpResponseInfo* info) {
  info->headers->ReplaceStatusLine("HTTP/1.1 400 Bad Request");
}

TEST_F(DnsTransactionTest, HttpsPostWithBadRequestResponse) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(
      base::BindRepeating(MakeResponseWithBadRequestResponse));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseWrongType(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Type");
  info->headers->AddHeader("Content-Type: text/html");
}

TEST_F(DnsTransactionTest, HttpsPostWithWrongType) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWrongType));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseRedirect(URLRequest* request, HttpResponseInfo* info) {
  if (request->url_chain().size() < 2) {
    info->headers->ReplaceStatusLine("HTTP/1.1 302 Found");
    info->headers->AddHeader("Location: /redirect-destination?" +
                             request->url().query());
  }
}

TEST_F(DnsTransactionTest, HttpsGetRedirect) {
  ConfigDohServers(true /* clear_udp */, false /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseRedirect));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

void MakeResponseNoType(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Type");
}

TEST_F(DnsTransactionTest, HttpsPostWithNoType) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseNoType));
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, HttpsCantLookupDohServers) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */, 2);
  TransactionHelper helper0(kMockHostname, kT0Qtype, ERR_CONNECTION_REFUSED);
  transaction_ids_.push_back(0);
  transaction_ids_.push_back(1);
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
}

class CountingObserver : public net::NetLog::ThreadSafeObserver {
 public:
  CountingObserver() : count_(0), dict_count_(0) {}

  ~CountingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override {
    ++count_;
    std::unique_ptr<base::Value> value = entry.ParametersToValue();
    if (value && value->is_dict())
      dict_count_++;
  }

  int count() const { return count_; }

  int dict_count() const { return dict_count_; }

 private:
  int count_;
  int dict_count_;
};

TEST_F(DnsTransactionTest, HttpsPostLookupWithLog) {
  ConfigDohServers(true /* clear_udp */, true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), SYNCHRONOUS,
                      Transport::HTTPS);
  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  CountingObserver observer;
  helper0.net_log()->AddObserver(&observer,
                                 NetLogCaptureMode::IncludeSocketBytes());
  EXPECT_TRUE(helper0.RunUntilDone(transaction_factory_.get()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.count(), 5);
  EXPECT_EQ(observer.dict_count(), 3);
}

TEST_F(DnsTransactionTest, TCPLookup) {
  base::HistogramTester histograms;
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      arraysize(kT0ResponseDatagram), ASYNC, Transport::TCP);

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
  histograms.ExpectUniqueSample("AsyncDNS.Rcode", dns_protocol::kRcodeNOERROR,
                                2);
}

TEST_F(DnsTransactionTest, TCPFailure) {
  base::HistogramTester histograms;
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL, ASYNC,
                   Transport::TCP);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_SERVER_FAILED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
  histograms.ExpectBucketCount("AsyncDNS.Rcode", dns_protocol::kRcodeNOERROR,
                               1);
  histograms.ExpectBucketCount("AsyncDNS.Rcode", dns_protocol::kRcodeSERVFAIL,
                               1);
  histograms.ExpectTotalCount("AsyncDNS.Rcode", 2);
}

TEST_F(DnsTransactionTest, TCPMalformed) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  // Valid response but length too short.
  // This must be truncated in the question section. The DnsResponse doesn't
  // examine the answer section until asked to parse it, so truncating it in
  // the answer section would result in the DnsTransaction itself succeeding.
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(
          reinterpret_cast<const char*>(kT0ResponseDatagram),
          arraysize(kT0ResponseDatagram), 0),
      ASYNC, static_cast<uint16_t>(kT0QuerySize - 1));
  AddSocketData(std::move(data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_MALFORMED_RESPONSE);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTestWithMockTime, TCPTimeout) {
  ConfigureFactory();
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddSocketData(std::make_unique<DnsSocketData>(
      1 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_DNS_TIMED_OUT);
  EXPECT_FALSE(helper0.Run(transaction_factory_.get()));
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTest, TCPReadReturnsZeroAsync) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  // Return all but the last byte of the response.
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(
          reinterpret_cast<const char*>(kT0ResponseDatagram),
          arraysize(kT0ResponseDatagram) - 1, 0),
      ASYNC, static_cast<uint16_t>(arraysize(kT0ResponseDatagram)));
  // Then return a 0-length read.
  data->AddReadError(0, ASYNC);
  AddSocketData(std::move(data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_CONNECTION_CLOSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPReadReturnsZeroSynchronous) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  // Return all but the last byte of the response.
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(
          reinterpret_cast<const char*>(kT0ResponseDatagram),
          arraysize(kT0ResponseDatagram) - 1, 0),
      SYNCHRONOUS, static_cast<uint16_t>(arraysize(kT0ResponseDatagram)));
  // Then return a 0-length read.
  data->AddReadError(0, SYNCHRONOUS);
  AddSocketData(std::move(data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_CONNECTION_CLOSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPConnectionClosedAsync) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  data->AddReadError(ERR_CONNECTION_CLOSED, ASYNC);
  AddSocketData(std::move(data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_CONNECTION_CLOSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, TCPConnectionClosedSynchronous) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  data->AddReadError(ERR_CONNECTION_CLOSED, SYNCHRONOUS);
  AddSocketData(std::move(data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_CONNECTION_CLOSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedThenNxdomainThenTCP) {
  config_.attempts = 2;
  ConfigureFactory();
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  // First attempt gets a mismatched response.
  data->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                        SYNCHRONOUS);
  // Second read from first attempt gets TCP required.
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));
  // Second attempt gets NXDOMAIN, which happens before the TCP required.
  AddSyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  std::unique_ptr<DnsSocketData> tcp_data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  tcp_data->AddReadError(ERR_CONNECTION_CLOSED, SYNCHRONOUS);
  AddSocketData(std::move(tcp_data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_NAME_NOT_RESOLVED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedThenOkThenTCP) {
  config_.attempts = 2;
  ConfigureFactory();
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  // First attempt gets a mismatched response.
  data->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                        SYNCHRONOUS);
  // Second read from first attempt gets TCP required.
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));
  // Second attempt gets a valid response, which happens before the TCP
  // required.
  AddSyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                          kT0ResponseDatagram, arraysize(kT0ResponseDatagram));
  std::unique_ptr<DnsSocketData> tcp_data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));
  tcp_data->AddReadError(ERR_CONNECTION_CLOSED, SYNCHRONOUS);
  AddSocketData(std::move(tcp_data));

  TransactionHelper helper0(kT0HostName, kT0Qtype, kT0RecordCount);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, MismatchedThenRefusedThenTCP) {
  // Set up the expected sequence of events:
  // 1) First attempt (UDP) gets a synchronous mismatched response. On such
  //    malformed responses, DnsTransaction triggers an immediate retry to read
  //    again from the socket within the same "attempt".
  // 2) Second read (within the first attempt) starts. Test is configured to
  //    give an asynchronous TCP required response which will complete later.
  //    On asynchronous action after a malformed response, the attempt will
  //    immediately produce a retriable error result while the retry continues,
  //    thus forking the running attempts.
  // 3) Error result triggers a second attempt (UDP) which test gives a
  //    synchronous ERR_CONNECTION_REFUSED, which is a retriable error, but
  //    DnsTransaction has exhausted max retries (2 attempts), so this result
  //    gets posted as the result of the transaction and other running attempts
  //    should be cancelled.
  // 4) First attempt should be cancelled when the transaction result is posted,
  //    so first attempt's second read should never complete. If it did
  //    complete, it would complete with a TCP-required error, and
  //    DnsTransaction would start a TCP attempt and clear previous attempts. It
  //    would be very bad if that then cleared the attempt posted as the final
  //    result, as result handling does not expect that memory to go away.

  config_.attempts = 2;
  ConfigureFactory();

  // Attempt 1.
  std::unique_ptr<DnsSocketData> data(new DnsSocketData(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP));
  data->AddResponseData(kT1ResponseDatagram, arraysize(kT1ResponseDatagram),
                        SYNCHRONOUS);
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));

  // Attempt 2.
  AddQueryAndErrorResponse(0 /* id */, kT0HostName, kT0Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS, Transport::UDP);

  TransactionHelper helper0(kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));
}

TEST_F(DnsTransactionTest, InvalidQuery) {
  ConfigureFactory();

  TransactionHelper helper0(".", dns_protocol::kTypeA, ERR_INVALID_ARGUMENT);
  EXPECT_TRUE(helper0.Run(transaction_factory_.get()));

  TransactionHelper helper1("foo,bar.com", dns_protocol::kTypeA,
                            ERR_INVALID_ARGUMENT);
  EXPECT_TRUE(helper1.Run(transaction_factory_.get()));
}

}  // namespace

}  // namespace net
