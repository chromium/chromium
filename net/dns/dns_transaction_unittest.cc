// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/dns/dns_transaction.h"

#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/idempotency.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_names_util.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_server_iterator.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/dns/resolve_context.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/third_party/uri_template/uri_template.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

base::TimeDelta kFallbackPeriod = base::Seconds(1);

const char kMockHostname[] = "mock.http";

std::vector<uint8_t> DomainFromDot(std::string_view dotted_name) {
  std::optional<std::vector<uint8_t>> dns_name =
      dns_names_util::DottedNameToNetwork(dotted_name);
  CHECK(dns_name.has_value());
  return dns_name.value();
}

enum class Transport { UDP, TCP, HTTPS };

class NetLogCountingObserver : public net::NetLog::ThreadSafeObserver {
 public:
  NetLogCountingObserver() = default;

  ~NetLogCountingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override {
    ++count_;
    if (!entry.params.empty()) {
      dict_count_++;
    }
  }

  int count() const { return count_; }

  int dict_count() const { return dict_count_; }

 private:
  int count_ = 0;
  int dict_count_ = 0;
};

// A SocketDataProvider builder.
class DnsSocketData {
 public:
  // The ctor takes parameters for the DnsQuery.
  DnsSocketData(uint16_t id,
                const char* dotted_name,
                uint16_t qtype,
                IoMode mode,
                Transport transport,
                const OptRecordRdata* opt_rdata = nullptr,
                DnsQuery::PaddingStrategy padding_strategy =
                    DnsQuery::PaddingStrategy::NONE)
      : query_(std::make_unique<DnsQuery>(id,
                                          DomainFromDot(dotted_name),
                                          qtype,
                                          opt_rdata,
                                          padding_strategy)),
        transport_(transport) {
    if (Transport::TCP == transport_) {
      auto length = std::make_unique<uint16_t>();
      *length = base::HostToNet16(query_->io_buffer()->size());
      writes_.emplace_back(mode, reinterpret_cast<const char*>(length.get()),
                           sizeof(uint16_t), num_reads_and_writes());
      lengths_.push_back(std::move(length));
    }
    writes_.emplace_back(mode, query_->io_buffer()->data(),
                         query_->io_buffer()->size(), num_reads_and_writes());
  }

  DnsSocketData(const DnsSocketData&) = delete;
  DnsSocketData& operator=(const DnsSocketData&) = delete;

  ~DnsSocketData() = default;

  void ClearWrites() { writes_.clear(); }
  // All responses must be added before GetProvider.

  // Adds pre-built DnsResponse. |tcp_length| will be used in TCP mode only.
  void AddResponseWithLength(std::unique_ptr<DnsResponse> response,
                             IoMode mode,
                             uint16_t tcp_length) {
    CHECK(!provider_.get());
    if (Transport::TCP == transport_) {
      auto length = std::make_unique<uint16_t>();
      *length = base::HostToNet16(tcp_length);
      reads_.emplace_back(mode, reinterpret_cast<const char*>(length.get()),
                          sizeof(uint16_t), num_reads_and_writes());
      lengths_.push_back(std::move(length));
    }
    reads_.emplace_back(mode, response->io_buffer()->data(),
                        response->io_buffer_size(), num_reads_and_writes());
    responses_.push_back(std::move(response));
  }

  // Adds pre-built DnsResponse.
  void AddResponse(std::unique_ptr<DnsResponse> response, IoMode mode) {
    uint16_t tcp_length = response->io_buffer_size();
    AddResponseWithLength(std::move(response), mode, tcp_length);
  }

  // Adds pre-built response from |data| buffer.
  void AddResponseData(base::span<const uint8_t> data, IoMode mode) {
    CHECK(!provider_.get());
    AddResponse(std::make_unique<DnsResponse>(data, 0), mode);
  }

  // Add no-answer (RCODE only) response matching the query.
  void AddRcode(int rcode, IoMode mode) {
    auto response =
        std::make_unique<DnsResponse>(query_->io_buffer()->span(), 0);
    dns_protocol::Header* header =
        reinterpret_cast<dns_protocol::Header*>(response->io_buffer()->data());
    header->flags |= base::HostToNet16(dns_protocol::kFlagResponse | rcode);
    AddResponse(std::move(response), mode);
  }

  // Add error response.
  void AddReadError(int error, IoMode mode) {
    reads_.emplace_back(mode, error, num_reads_and_writes());
  }

  // Build, if needed, and return the SocketDataProvider. No new responses
  // should be added afterwards.
  SequencedSocketData* GetProvider() {
    if (provider_.get())
      return provider_.get();
    // Terminate the reads with ERR_IO_PENDING to prevent overrun and default to
    // timeout.
    if (transport_ != Transport::HTTPS) {
      reads_.emplace_back(SYNCHRONOUS, ERR_IO_PENDING,
                          writes_.size() + reads_.size());
    }
    provider_ = std::make_unique<SequencedSocketData>(reads_, writes_);
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
};

class TestSocketFactory;

// A variant of MockUDPClientSocket which always fails to Connect.
class FailingUDPClientSocket : public MockUDPClientSocket {
 public:
  FailingUDPClientSocket(SocketDataProvider* data, net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log) {}

  FailingUDPClientSocket(const FailingUDPClientSocket&) = delete;
  FailingUDPClientSocket& operator=(const FailingUDPClientSocket&) = delete;

  ~FailingUDPClientSocket() override = default;
  int Connect(const IPEndPoint& endpoint) override {
    return ERR_CONNECTION_REFUSED;
  }
};

// A variant of MockUDPClientSocket which notifies the factory OnConnect.
class TestUDPClientSocket : public MockUDPClientSocket {
 public:
  TestUDPClientSocket(TestSocketFactory* factory,
                      SocketDataProvider* data,
                      net::NetLog* net_log)
      : MockUDPClientSocket(data, net_log), factory_(factory) {}

  TestUDPClientSocket(const TestUDPClientSocket&) = delete;
  TestUDPClientSocket& operator=(const TestUDPClientSocket&) = delete;

  ~TestUDPClientSocket() override = default;
  int Connect(const IPEndPoint& endpoint) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;

 private:
  raw_ptr<TestSocketFactory> factory_;
};

// Creates TestUDPClientSockets and keeps endpoints reported via OnConnect.
class TestSocketFactory : public MockClientSocketFactory {
 public:
  TestSocketFactory() = default;
  ~TestSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    if (fail_next_socket_) {
      fail_next_socket_ = false;
      return std::make_unique<FailingUDPClientSocket>(&empty_data_, net_log);
    }

    SocketDataProvider* data_provider = mock_data().GetNext();
    auto socket =
        std::make_unique<TestUDPClientSocket>(this, data_provider, net_log);

    // Even using DEFAULT_BIND, actual sockets have been measured to very rarely
    // repeat the same source port multiple times in a row. Need to mimic that
    // functionality here, so DnsUdpTracker doesn't misdiagnose repeated port
    // as low entropy.
    if (diverse_source_ports_)
      socket->set_source_port(next_source_port_++);

    return socket;
  }

  void OnConnect(const IPEndPoint& endpoint) {
    remote_endpoints_.emplace_back(endpoint);
  }

  struct RemoteNameserver {
    explicit RemoteNameserver(IPEndPoint insecure_nameserver)
        : insecure_nameserver(insecure_nameserver) {}
    explicit RemoteNameserver(DnsOverHttpsServerConfig secure_nameserver)
        : secure_nameserver(secure_nameserver) {}

    std::optional<IPEndPoint> insecure_nameserver;
    std::optional<DnsOverHttpsServerConfig> secure_nameserver;
  };

  std::vector<RemoteNameserver> remote_endpoints_;
  bool fail_next_socket_ = false;
  bool diverse_source_ports_ = true;

 private:
  StaticSocketDataProvider empty_data_;
  uint16_t next_source_port_ = 123;
};

int TestUDPClientSocket::Connect(const IPEndPoint& endpoint) {
  factory_->OnConnect(endpoint);
  return MockUDPClientSocket::Connect(endpoint);
}

int TestUDPClientSocket::ConnectAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) {
  factory_->OnConnect(address);
  return MockUDPClientSocket::ConnectAsync(address, std::move(callback));
}

// Helper class that holds a DnsTransaction and handles OnTransactionComplete.
class TransactionHelper {
 public:
  // If |expected_answer_count| < 0 then it is the expected net error.
  explicit TransactionHelper(int expected_answer_count)
      : expected_answer_count_(expected_answer_count) {}

  // Mark that the transaction shall be destroyed immediately upon callback.
  void set_cancel_in_callback() { cancel_in_callback_ = true; }

  void StartTransaction(DnsTransactionFactory* factory,
                        const char* hostname,
                        uint16_t qtype,
                        bool secure,
                        ResolveContext* context) {
    std::unique_ptr<DnsTransaction> transaction = factory->CreateTransaction(
        hostname, qtype,
        NetLogWithSource::Make(net::NetLog::Get(), net::NetLogSourceType::NONE),
        secure, factory->GetSecureDnsModeForTest(), context,
        true /* fast_timeout */);
    transaction->SetRequestPriority(DEFAULT_PRIORITY);
    EXPECT_EQ(qtype, transaction->GetType());
    StartTransaction(std::move(transaction));
  }

  void StartTransaction(std::unique_ptr<DnsTransaction> transaction) {
    EXPECT_FALSE(transaction_);
    transaction_ = std::move(transaction);
    qtype_ = transaction_->GetType();
    transaction_->Start(base::BindOnce(
        &TransactionHelper::OnTransactionComplete, base::Unretained(this)));
  }

  void Cancel() {
    ASSERT_TRUE(transaction_.get() != nullptr);
    transaction_.reset(nullptr);
  }

  void OnTransactionComplete(int rv, const DnsResponse* response) {
    EXPECT_FALSE(completed_);

    completed_ = true;
    response_ = response;

    transaction_complete_run_loop_.Quit();

    if (cancel_in_callback_) {
      Cancel();
      return;
    }

    if (response)
      EXPECT_TRUE(response->IsValid());

    if (expected_answer_count_ >= 0) {
      ASSERT_THAT(rv, IsOk());
      ASSERT_TRUE(response != nullptr);
      EXPECT_EQ(static_cast<unsigned>(expected_answer_count_),
                response->answer_count());
      EXPECT_EQ(qtype_, response->GetSingleQType());

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

  // Runs until the completion callback is called. Transaction must have already
  // been started or this will never complete.
  void RunUntilComplete() {
    DCHECK(transaction_);
    DCHECK(!transaction_complete_run_loop_.running());
    transaction_complete_run_loop_.Run();
    DCHECK(has_completed());
  }

 private:
  uint16_t qtype_ = 0;
  std::unique_ptr<DnsTransaction> transaction_;
  raw_ptr<const DnsResponse, AcrossTasksDanglingUntriaged> response_ = nullptr;
  int expected_answer_count_;
  bool cancel_in_callback_ = false;
  base::RunLoop transaction_complete_run_loop_;
  bool completed_ = false;
};

// Callback that allows a test to modify HttpResponseinfo
// before the response is sent to the requester. This allows
// response headers to be changed.
using ResponseModifierCallback =
    base::RepeatingCallback<void(URLRequest* request, HttpResponseInfo* info)>;

// Callback that allows the test to substitute its own implementation
// of URLRequestJob to handle the request.
using DohJobMakerCallback = base::RepeatingCallback<std::unique_ptr<
    URLRequestJob>(URLRequest* request, SocketDataProvider* data_provider)>;

// Callback to notify that URLRequestJob::Start has been called.
using UrlRequestStartedCallback = base::RepeatingCallback<void()>;

// Subclass of URLRequestJob which takes a SocketDataProvider with data
// representing both a DNS over HTTPS query and response.
class URLRequestMockDohJob : public URLRequestJob, public AsyncSocket {
 public:
  URLRequestMockDohJob(
      URLRequest* request,
      SocketDataProvider* data_provider,
      ResponseModifierCallback response_modifier = ResponseModifierCallback(),
      UrlRequestStartedCallback on_start = UrlRequestStartedCallback())
      : URLRequestJob(request),
        data_provider_(data_provider),
        response_modifier_(response_modifier),
        on_start_(on_start) {
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
      EXPECT_EQ(IDEMPOTENT, request->GetIdempotency());
      const UploadDataStream* stream = request->get_upload_for_testing();
      auto* readers = stream->GetElementReaders();
      EXPECT_TRUE(readers);
      EXPECT_FALSE(readers->empty());
      for (auto& reader : *readers) {
        const UploadBytesElementReader* byte_reader = reader->AsBytesReader();
        decoded_query +=
            std::string(base::as_string_view(byte_reader->bytes()));
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

  static std::string GetMockHttpUrl(const std::string& path) {
    return "http://" + (kMockHostname + ("/" + path));
  }

  // URLRequestJob implementation:
  void Start() override {
    if (on_start_)
      on_start_.Run();
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&URLRequestMockDohJob::StartAsync,
                                  weak_factory_.GetWeakPtr()));
  }

  URLRequestMockDohJob(const URLRequestMockDohJob&) = delete;
  URLRequestMockDohJob& operator=(const URLRequestMockDohJob&) = delete;

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
    info->headers = base::MakeRefCounted<HttpResponseHeaders>(
        HttpUtil::AssembleRawHeaders(raw_headers));
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
      std::copy(data, data + buf_size, buf->data());
      leftover_data_ = data + buf_size;
      leftover_data_len_ = data_len - buf_size;
      return buf_size;
    }
    std::copy(data, data + data_len, buf->data());
    return data_len;
  }

  const int content_length_ = 0;
  const char* leftover_data_;
  int leftover_data_len_ = 0;
  raw_ptr<SocketDataProvider> data_provider_;
  const ResponseModifierCallback response_modifier_;
  const UrlRequestStartedCallback on_start_;
  raw_ptr<IOBuffer> pending_buf_;
  int pending_buf_size_;

  base::WeakPtrFactory<URLRequestMockDohJob> weak_factory_{this};
};

class DnsTransactionTestBase : public testing::Test {
 public:
  DnsTransactionTestBase() = default;

  ~DnsTransactionTestBase() override {
    // All queued transaction IDs should be used by a transaction calling
    // GetNextId().
    CHECK(transaction_ids_.empty());
  }

  // Generates |nameservers| for DnsConfig.
  void ConfigureNumServers(size_t num_servers) {
    CHECK_LE(num_servers, 255u);
    config_.nameservers.clear();
    for (size_t i = 0; i < num_servers; ++i) {
      config_.nameservers.emplace_back(IPAddress(192, 168, 1, i),
                                       dns_protocol::kDefaultPort);
    }
  }

  // Configures the DnsConfig DNS-over-HTTPS server(s), which either
  // accept GET or POST requests based on use_post. If a
  // ResponseModifierCallback is provided it will be called to construct the
  // HTTPResponse.
  void ConfigureDohServers(bool use_post,
                           size_t num_doh_servers = 1,
                           bool make_available = true) {
    GURL url(URLRequestMockDohJob::GetMockHttpsUrl("doh_test"));
    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->AddHostnameInterceptor(url.scheme(), url.host(),
                                   std::make_unique<DohJobInterceptor>(this));
    CHECK_LE(num_doh_servers, 255u);
    std::vector<string> templates;
    templates.reserve(num_doh_servers);
    for (size_t i = 0; i < num_doh_servers; ++i) {
      templates.push_back(URLRequestMockDohJob::GetMockHttpsUrl(
                              base::StringPrintf("doh_test_%zu", i)) +
                          (use_post ? "" : "{?dns}"));
    }
    config_.doh_config =
        *DnsOverHttpsConfig::FromTemplatesForTesting(std::move(templates));
    ConfigureFactory();

    if (make_available) {
      for (size_t server_index = 0; server_index < num_doh_servers;
           ++server_index) {
        resolve_context_->RecordServerSuccess(
            server_index, true /* is_doh_server */, session_.get());
      }
    }
  }

  // Called after fully configuring |config|.
  void ConfigureFactory() {
    session_ = base::MakeRefCounted<DnsSession>(
        config_,
        base::BindRepeating(&DnsTransactionTestBase::GetNextId,
                            base::Unretained(this)),
        nullptr /* NetLog */);
    resolve_context_->InvalidateCachesAndPerSessionData(
        session_.get(), false /* network_change */);
    transaction_factory_ = DnsTransactionFactory::CreateFactory(session_.get());
  }

  void AddSocketData(std::unique_ptr<DnsSocketData> data,
                     bool enqueue_transaction_id = true) {
    CHECK(socket_factory_.get());
    if (enqueue_transaction_id)
      transaction_ids_.push_back(data->query_id());
    socket_factory_->AddSocketDataProvider(data->GetProvider());
    socket_data_.push_back(std::move(data));
  }

  void AddQueryAndResponseNoWrite(uint16_t id,
                                  const char* dotted_name,
                                  uint16_t qtype,
                                  IoMode mode,
                                  Transport transport,
                                  const OptRecordRdata* opt_rdata = nullptr,
                                  DnsQuery::PaddingStrategy padding_strategy =
                                      DnsQuery::PaddingStrategy::NONE) {
    CHECK(socket_factory_.get());
    auto data = std::make_unique<DnsSocketData>(
        id, dotted_name, qtype, mode, transport, opt_rdata, padding_strategy);
    data->ClearWrites();
    AddSocketData(std::move(data), true);
  }

  // Add expected query for |dotted_name| and |qtype| with |id| and response
  // taken verbatim from |data| of |data_length| bytes. The transaction id in
  // |data| should equal |id|, unless testing mismatched response.
  void AddQueryAndResponse(uint16_t id,
                           const char* dotted_name,
                           uint16_t qtype,
                           base::span<const uint8_t> response_data,
                           IoMode mode,
                           Transport transport,
                           const OptRecordRdata* opt_rdata = nullptr,
                           DnsQuery::PaddingStrategy padding_strategy =
                               DnsQuery::PaddingStrategy::NONE,
                           bool enqueue_transaction_id = true) {
    CHECK(socket_factory_.get());
    auto data = std::make_unique<DnsSocketData>(
        id, dotted_name, qtype, mode, transport, opt_rdata, padding_strategy);
    data->AddResponseData(response_data, mode);
    AddSocketData(std::move(data), enqueue_transaction_id);
  }

  void AddQueryAndErrorResponse(uint16_t id,
                                const char* dotted_name,
                                uint16_t qtype,
                                int error,
                                IoMode mode,
                                Transport transport,
                                const OptRecordRdata* opt_rdata = nullptr,
                                DnsQuery::PaddingStrategy padding_strategy =
                                    DnsQuery::PaddingStrategy::NONE,
                                bool enqueue_transaction_id = true) {
    CHECK(socket_factory_.get());
    auto data = std::make_unique<DnsSocketData>(
        id, dotted_name, qtype, mode, transport, opt_rdata, padding_strategy);
    data->AddReadError(error, mode);
    AddSocketData(std::move(data), enqueue_transaction_id);
  }

  void AddAsyncQueryAndResponse(uint16_t id,
                                const char* dotted_name,
                                uint16_t qtype,
                                base::span<const uint8_t> data,
                                const OptRecordRdata* opt_rdata = nullptr) {
    AddQueryAndResponse(id, dotted_name, qtype, data, ASYNC, Transport::UDP,
                        opt_rdata);
  }

  void AddSyncQueryAndResponse(uint16_t id,
                               const char* dotted_name,
                               uint16_t qtype,
                               base::span<const uint8_t> data,
                               const OptRecordRdata* opt_rdata = nullptr) {
    AddQueryAndResponse(id, dotted_name, qtype, data, SYNCHRONOUS,
                        Transport::UDP, opt_rdata);
  }

  // Add expected query of |dotted_name| and |qtype| and no response.
  void AddHangingQuery(
      const char* dotted_name,
      uint16_t qtype,
      DnsQuery::PaddingStrategy padding_strategy =
          DnsQuery::PaddingStrategy::NONE,
      uint16_t id = base::RandInt(0, std::numeric_limits<uint16_t>::max()),
      bool enqueue_transaction_id = true) {
    auto data = std::make_unique<DnsSocketData>(
        id, dotted_name, qtype, ASYNC, Transport::UDP, nullptr /* opt_rdata */,
        padding_strategy);
    AddSocketData(std::move(data), enqueue_transaction_id);
  }

  // Add expected query of |dotted_name| and |qtype| and matching response with
  // no answer and RCODE set to |rcode|. The id will be generated randomly.
  void AddQueryAndRcode(
      const char* dotted_name,
      uint16_t qtype,
      int rcode,
      IoMode mode,
      Transport trans,
      DnsQuery::PaddingStrategy padding_strategy =
          DnsQuery::PaddingStrategy::NONE,
      uint16_t id = base::RandInt(0, std::numeric_limits<uint16_t>::max()),
      bool enqueue_transaction_id = true) {
    CHECK_NE(dns_protocol::kRcodeNOERROR, rcode);
    auto data = std::make_unique<DnsSocketData>(id, dotted_name, qtype, mode,
                                                trans, nullptr /* opt_rdata */,
                                                padding_strategy);
    data->AddRcode(rcode, mode);
    AddSocketData(std::move(data), enqueue_transaction_id);
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
  void CheckServerOrder(const size_t* servers, size_t num_attempts) {
    ASSERT_EQ(num_attempts, socket_factory_->remote_endpoints_.size());
    auto num_insecure_nameservers = session_->config().nameservers.size();
    for (size_t i = 0; i < num_attempts; ++i) {
      if (servers[i] < num_insecure_nameservers) {
        // Check insecure server match.
        EXPECT_EQ(
            socket_factory_->remote_endpoints_[i].insecure_nameserver.value(),
            session_->config().nameservers[servers[i]]);
      } else {
        // Check secure server match.
        EXPECT_EQ(
            socket_factory_->remote_endpoints_[i].secure_nameserver.value(),
            session_->config()
                .doh_config.servers()[servers[i] - num_insecure_nameservers]);
      }
    }
  }

  std::unique_ptr<URLRequestJob> MaybeInterceptRequest(URLRequest* request) {
    // If the path indicates a redirect, skip checking the list of
    // configured servers, because it won't be there and we still want
    // to handle it.
    bool server_found = request->url().path() == "/redirect-destination";
    for (auto server : config_.doh_config.servers()) {
      if (server_found)
        break;
      std::string url_base =
          GetURLFromTemplateWithoutParameters(server.server_template());
      if (server.use_post() && request->method() == "POST") {
        if (url_base == request->url().spec()) {
          server_found = true;
          socket_factory_->remote_endpoints_.emplace_back(server);
        }
      } else if (!server.use_post() && request->method() == "GET") {
        std::string prefix = url_base + "?dns=";
        auto mispair = base::ranges::mismatch(prefix, request->url().spec());
        if (mispair.first == prefix.end()) {
          server_found = true;
          socket_factory_->remote_endpoints_.emplace_back(server);
        }
      }
    }
    EXPECT_TRUE(server_found);

    EXPECT_TRUE(
        request->isolation_info().network_isolation_key().IsTransient());

    // All DoH requests for the same ResolveContext should use the same
    // IsolationInfo, so network objects like sockets can be reused between
    // requests.
    if (!expect_multiple_isolation_infos_) {
      if (!isolation_info_) {
        isolation_info_ =
            std::make_unique<IsolationInfo>(request->isolation_info());
      } else {
        EXPECT_TRUE(
            isolation_info_->IsEqualForTesting(request->isolation_info()));
      }
    }

    EXPECT_FALSE(request->allow_credentials());
    EXPECT_EQ(SecureDnsPolicy::kBootstrap, request->secure_dns_policy());

    EXPECT_THAT(request->extra_request_headers().GetHeader("Accept"),
                testing::Optional(std::string("application/dns-message")));

    EXPECT_THAT(request->extra_request_headers().GetHeader("Accept-Language"),
                testing::Optional(std::string("*")));

    EXPECT_THAT(request->extra_request_headers().GetHeader("User-Agent"),
                testing::Optional(std::string("Chrome")));

    SocketDataProvider* provider = socket_factory_->mock_data().GetNext();

    if (doh_job_maker_)
      return doh_job_maker_.Run(request, provider);

    return std::make_unique<URLRequestMockDohJob>(
        request, provider, response_modifier_, on_start_);
  }

  class DohJobInterceptor : public URLRequestInterceptor {
   public:
    explicit DohJobInterceptor(DnsTransactionTestBase* test) : test_(test) {}

    DohJobInterceptor(const DohJobInterceptor&) = delete;
    DohJobInterceptor& operator=(const DohJobInterceptor&) = delete;

    ~DohJobInterceptor() override = default;

    // URLRequestInterceptor implementation:
    std::unique_ptr<URLRequestJob> MaybeInterceptRequest(
        URLRequest* request) const override {
      return test_->MaybeInterceptRequest(request);
    }

   private:
    raw_ptr<DnsTransactionTestBase> test_;
  };

  void SetResponseModifierCallback(ResponseModifierCallback response_modifier) {
    response_modifier_ = response_modifier;
  }

  void SetDohJobMakerCallback(DohJobMakerCallback doh_job_maker) {
    doh_job_maker_ = doh_job_maker;
  }

  void SetUrlRequestStartedCallback(UrlRequestStartedCallback on_start) {
    on_start_ = on_start;
  }

  void SetUp() override {
    // By default set one server,
    ConfigureNumServers(1);
    // and no retransmissions,
    config_.attempts = 1;
    // and an arbitrary fallback period.
    config_.fallback_period = kFallbackPeriod;
    auto context_builder = CreateTestURLRequestContextBuilder();
    socket_factory_ = std::make_unique<TestSocketFactory>();
    context_builder->set_client_socket_factory_for_testing(
        socket_factory_.get());
    request_context_ = context_builder->Build();
    resolve_context_ = std::make_unique<ResolveContext>(
        request_context_.get(), false /* enable_caching */);

    ConfigureFactory();
  }

  void TearDown() override {
    // Check that all socket data was at least written to.
    for (size_t i = 0; i < socket_data_.size(); ++i) {
      EXPECT_TRUE(socket_data_[i]->GetProvider()->AllWriteDataConsumed()) << i;
    }

    URLRequestFilter* filter = URLRequestFilter::GetInstance();
    filter->ClearHandlers();
  }

  void set_expect_multiple_isolation_infos(
      bool expect_multiple_isolation_infos) {
    expect_multiple_isolation_infos_ = expect_multiple_isolation_infos;
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
  std::unique_ptr<URLRequestContext> request_context_;
  std::unique_ptr<ResolveContext> resolve_context_;
  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> transaction_factory_;
  ResponseModifierCallback response_modifier_;
  UrlRequestStartedCallback on_start_;
  DohJobMakerCallback doh_job_maker_;

  // Whether multiple IsolationInfos should be expected (due to there being
  // multiple RequestContexts in use).
  bool expect_multiple_isolation_infos_ = false;

  // IsolationInfo used by DoH requests. Populated on first DoH request, and
  // compared to IsolationInfo used by all subsequent requests, unless
  // |expect_multiple_isolation_infos_| is true.
  std::unique_ptr<IsolationInfo> isolation_info_;
};

class DnsTransactionTest : public DnsTransactionTestBase,
                           public WithTaskEnvironment {
 public:
  DnsTransactionTest() = default;
  ~DnsTransactionTest() override = default;
};

class DnsTransactionTestWithMockTime : public DnsTransactionTestBase,
                                       public WithTaskEnvironment {
 protected:
  DnsTransactionTestWithMockTime()
      : WithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DnsTransactionTestWithMockTime() override = default;
};

TEST_F(DnsTransactionTest, Lookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, LookupWithLog) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  NetLogCountingObserver observer;
  NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kEverything);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  EXPECT_EQ(observer.count(), 7);
  EXPECT_EQ(observer.dict_count(), 5);
}

TEST_F(DnsTransactionTest, LookupWithEDNSOption) {
  OptRecordRdata expected_opt_rdata;

  transaction_factory_->AddEDNSOption(
      OptRecordRdata::UnknownOpt::CreateForTesting(123, "\xbe\xef"));
  expected_opt_rdata.AddOpt(
      OptRecordRdata::UnknownOpt::CreateForTesting(123, "\xbe\xef"));

  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, &expected_opt_rdata);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, LookupWithMultipleEDNSOptions) {
  OptRecordRdata expected_opt_rdata;

  std::vector<std::pair<uint16_t, std::string>> params = {
      // Two options with the same code, to check that both are included.
      std::pair<uint16_t, std::string>(1, "\xde\xad"),
      std::pair<uint16_t, std::string>(1, "\xbe\xef"),
      // Try a different code and different length of data.
      std::pair<uint16_t, std::string>(2, "\xff")};

  for (auto& param : params) {
    transaction_factory_->AddEDNSOption(
        OptRecordRdata::UnknownOpt::CreateForTesting(param.first,
                                                     param.second));
    expected_opt_rdata.AddOpt(OptRecordRdata::UnknownOpt::CreateForTesting(
        param.first, param.second));
  }

  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram, &expected_opt_rdata);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

// Concurrent lookup tests assume that DnsTransaction::Start immediately
// consumes a socket from ClientSocketFactory.
TEST_F(DnsTransactionTest, ConcurrentLookup) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);
  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  TransactionHelper helper1(kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get(), kT1HostName, kT1Qtype,
                           false /* secure */, resolve_context_.get());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, CancelLookup) {
  AddQueryAndResponseNoWrite(0 /* id */, kT0HostName, kT0Qtype, ASYNC,
                             Transport::UDP, nullptr);

  AddAsyncQueryAndResponse(1 /* id */, kT1HostName, kT1Qtype,
                           kT1ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  TransactionHelper helper1(kT1RecordCount);
  helper1.StartTransaction(transaction_factory_.get(), kT1HostName, kT1Qtype,
                           false /* secure */, resolve_context_.get());

  helper0.Cancel();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(helper0.has_completed());
  EXPECT_TRUE(helper1.has_completed());
}

TEST_F(DnsTransactionTest, DestroyFactory) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());

  // Destroying the client does not affect running requests.
  transaction_factory_.reset(nullptr);

  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, CancelFromCallback) {
  AddAsyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.set_cancel_in_callback();

  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, MismatchedResponseSync) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response synchronously.
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              SYNCHRONOUS, Transport::UDP);
  data->AddResponseData(kT1ResponseDatagram, SYNCHRONOUS);
  AddSocketData(std::move(data));

  // Second attempt receives valid response synchronously.
  auto data1 = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::UDP);
  data1->AddResponseData(kT0ResponseDatagram, SYNCHRONOUS);
  AddSocketData(std::move(data1));

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, MismatchedResponseAsync) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response asynchronously.
  auto data0 = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName,
                                               kT0Qtype, ASYNC, Transport::UDP);
  data0->AddResponseData(kT1ResponseDatagram, ASYNC);
  AddSocketData(std::move(data0));

  // Second attempt receives valid response asynchronously.
  auto data1 = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName,
                                               kT0Qtype, ASYNC, Transport::UDP);
  data1->AddResponseData(kT0ResponseDatagram, ASYNC);
  AddSocketData(std::move(data1));

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

// Test that responses are not accepted when only the response ID mismatches.
// Tests against incorrect transaction ID validation, which is anti-pattern #1
// from the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST_F(DnsTransactionTest, MismatchedResponseFail) {
  ConfigureFactory();

  // Attempt receives mismatched response and fails because only one attempt is
  // allowed.
  AddAsyncQueryAndResponse(1 /* id */, kT0HostName, kT0Qtype,
                           kT0ResponseDatagram);

  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, MismatchedResponseNxdomain) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives mismatched response followed by valid NXDOMAIN
  // response.
  // Second attempt receives valid NXDOMAIN response.
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              SYNCHRONOUS, Transport::UDP);
  data->AddResponseData(kT1ResponseDatagram, SYNCHRONOUS);
  data->AddRcode(dns_protocol::kRcodeNXDOMAIN, ASYNC);
  AddSocketData(std::move(data));
  AddSyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

// This is a regression test for https://crbug.com/1410442.
TEST_F(DnsTransactionTest, ZeroSizeResponseAsync) {
  config_.attempts = 2;
  ConfigureFactory();

  // First attempt receives zero size response asynchronously.
  auto data0 = std::make_unique<DnsSocketData>(/*id=*/0, kT0HostName, kT0Qtype,
                                               ASYNC, Transport::UDP);
  data0->AddReadError(0, ASYNC);
  AddSocketData(std::move(data0));

  // Second attempt receives valid response asynchronously.
  auto data1 = std::make_unique<DnsSocketData>(/*id=*/0, kT0HostName, kT0Qtype,
                                               ASYNC, Transport::UDP);
  data1->AddResponseData(kT0ResponseDatagram, ASYNC);
  AddSocketData(std::move(data1));

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           /*secure=*/false, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, ServerFail) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);

  TransactionHelper helper0(ERR_DNS_SERVER_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();

  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, NoDomain) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTestWithMockTime, Timeout_FastTimeout) {
  config_.attempts = 3;
  ConfigureFactory();

  AddHangingQuery(kT0HostName, kT0Qtype);
  AddHangingQuery(kT0HostName, kT0Qtype);
  AddHangingQuery(kT0HostName, kT0Qtype);

  TransactionHelper helper0(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), false /* secure */,
          SecureDnsMode::kOff, resolve_context_.get(), true /* fast_timeout */);

  helper0.StartTransaction(std::move(transaction));

  // Finish when the third attempt expires its fallback period.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());
  FastForwardBy(
      resolve_context_->NextClassicFallbackPeriod(0, 0, session_.get()));
  EXPECT_FALSE(helper0.has_completed());
  FastForwardBy(
      resolve_context_->NextClassicFallbackPeriod(0, 1, session_.get()));
  EXPECT_FALSE(helper0.has_completed());
  FastForwardBy(
      resolve_context_->NextClassicFallbackPeriod(0, 2, session_.get()));
  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTestWithMockTime, ServerFallbackAndRotate) {
  // Test that we fallback on both server failure and fallback period
  // expiration.
  config_.attempts = 2;
  // The next request should start from the next server.
  config_.rotate = true;
  ConfigureNumServers(3);
  ConfigureFactory();

  // Responses for first request.
  AddHangingQuery(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddHangingQuery(kT0HostName, kT0Qtype);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);
  // Responses for second request.
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeSERVFAIL);
  AddAsyncQueryAndRcode(kT1HostName, kT1Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  TransactionHelper helper1(ERR_NAME_NOT_RESOLVED);

  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(helper0.has_completed());

  helper1.StartTransaction(transaction_factory_.get(), kT1HostName, kT1Qtype,
                           false /* secure */, resolve_context_.get());
  helper1.RunUntilComplete();

  size_t kOrder[] = {
      // The first transaction.
      0,
      1,
      2,
      0,
      1,
      // The second transaction starts from the next server, and 0 is skipped
      // because it already has 2 consecutive failures.
      1,
      2,
      1,
  };
  CheckServerOrder(kOrder, std::size(kOrder));
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

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);

  helper0.StartTransaction(transaction_factory_.get(), "x.y.z",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  // Also check if suffix search causes server rotation.
  size_t kOrder0[] = {0, 1, 0, 1};
  CheckServerOrder(kOrder0, std::size(kOrder0));
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

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), "x.y",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  // A single-label name.
  TransactionHelper helper1(ERR_NAME_NOT_RESOLVED);
  helper1.StartTransaction(transaction_factory_.get(), "x",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper1.RunUntilComplete();

  // A fully-qualified name.
  TransactionHelper helper2(ERR_NAME_NOT_RESOLVED);
  helper2.StartTransaction(transaction_factory_.get(), "x.",
                           dns_protocol::kTypeAAAA, false /* secure */,
                           resolve_context_.get());
  helper2.RunUntilComplete();
}

TEST_F(DnsTransactionTest, EmptySuffixSearch) {
  // Responses for first transaction.
  AddAsyncQueryAndRcode("x", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);

  // A fully-qualified name.
  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), "x.",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  // A single label name is not even attempted.
  TransactionHelper helper1(ERR_DNS_SEARCH_EMPTY);
  helper1.StartTransaction(transaction_factory_.get(), "singlelabel",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper1.RunUntilComplete();
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

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), "x.y.z",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  TransactionHelper helper1(ERR_NAME_NOT_RESOLVED);
  helper1.StartTransaction(transaction_factory_.get(), "x.y",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper1.RunUntilComplete();

  TransactionHelper helper2(ERR_NAME_NOT_RESOLVED);
  helper2.StartTransaction(transaction_factory_.get(), "x",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper2.RunUntilComplete();
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
                           kResponseNoData);

  TransactionHelper helper0(0 /* answers */);

  helper0.StartTransaction(transaction_factory_.get(), "x.y.z",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, SyncFirstQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                          kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, SyncFirstQueryWithSearch) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddSyncQueryAndRcode("www.lab.ccs.neu.edu", kT2Qtype,
                       dns_protocol::kRcodeNXDOMAIN);
  // "www.ccs.neu.edu"
  AddAsyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                           kT2ResponseDatagram);

  TransactionHelper helper0(kT2RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), "www", kT2Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, SyncSearchQuery) {
  config_.search.push_back("lab.ccs.neu.edu");
  config_.search.push_back("ccs.neu.edu");
  ConfigureFactory();

  AddAsyncQueryAndRcode("www.lab.ccs.neu.edu", dns_protocol::kTypeA,
                        dns_protocol::kRcodeNXDOMAIN);
  AddSyncQueryAndResponse(2 /* id */, kT2HostName, kT2Qtype,
                          kT2ResponseDatagram);

  TransactionHelper helper0(kT2RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), "www", kT2Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, ConnectFailure) {
  // Prep socket factory for a single socket with connection failure.
  MockConnect connect_data;
  connect_data.result = ERR_FAILED;
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(connect_data);
  socket_factory_->AddSocketDataProvider(&data_provider);

  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  TransactionHelper helper0(ERR_CONNECTION_REFUSED);

  helper0.StartTransaction(transaction_factory_.get(), "www.chromium.org",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  EXPECT_FALSE(helper0.response());
  EXPECT_FALSE(session_->udp_tracker()->low_entropy());
}

TEST_F(DnsTransactionTest, ConnectFailure_SocketLimitReached) {
  // Prep socket factory for a single socket with connection failure.
  MockConnect connect_data;
  connect_data.result = ERR_INSUFFICIENT_RESOURCES;
  StaticSocketDataProvider data_provider;
  data_provider.set_connect_data(connect_data);
  socket_factory_->AddSocketDataProvider(&data_provider);

  transaction_ids_.push_back(0);  // Needed to make a DnsUDPAttempt.
  TransactionHelper helper0(ERR_CONNECTION_REFUSED);

  helper0.StartTransaction(transaction_factory_.get(), "www.chromium.org",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  EXPECT_FALSE(helper0.response());
  EXPECT_TRUE(session_->udp_tracker()->low_entropy());
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
                           kT0ResponseDatagram);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsGetLookup) {
  ConfigureDohServers(false /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsGetFailure) {
  ConfigureDohServers(false /* use_post */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper0(ERR_DNS_SERVER_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, HttpsGetMalformed) {
  ConfigureDohServers(false /* use_post */);
  // Use T1 response, which is malformed for a T0 request.
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT1ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookup) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostFailure) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper0(ERR_DNS_SERVER_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, HttpsPostMalformed) {
  ConfigureDohServers(true /* use_post */);
  // Use T1 response, which is malformed for a T0 request.
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT1ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsync) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram, ASYNC,
                      Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

std::unique_ptr<URLRequestJob> DohJobMakerCallbackFailLookup(
    URLRequest* request,
    SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return std::make_unique<URLRequestFailedJob>(
      request, URLRequestFailedJob::START, ERR_NAME_NOT_RESOLVED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailDohServerLookup) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_SECURE_RESOLVER_HOSTNAME_RESOLUTION_FAILED);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailLookup));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

std::unique_ptr<URLRequestJob> DohJobMakerCallbackFailStart(
    URLRequest* request,
    SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return std::make_unique<URLRequestFailedJob>(
      request, URLRequestFailedJob::START, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailStart) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

std::unique_ptr<URLRequestJob> DohJobMakerCallbackFailSync(
    URLRequest* request,
    SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return std::make_unique<URLRequestFailedJob>(
      request, URLRequestFailedJob::READ_SYNC, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailSync) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseWithLength(std::make_unique<DnsResponse>(), SYNCHRONOUS, 0);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailSync));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

std::unique_ptr<URLRequestJob> DohJobMakerCallbackFailAsync(
    URLRequest* request,
    SocketDataProvider* data) {
  URLRequestMockDohJob::MatchQueryData(request, data);
  return std::make_unique<URLRequestFailedJob>(
      request, URLRequestFailedJob::READ_ASYNC, ERR_FAILED);
}

TEST_F(DnsTransactionTest, HttpsPostLookupFailAsync) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailAsync));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookup2Sync) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  auto [first20bytes, rest] = base::span(kT0ResponseDatagram).split_at(20u);
  data->AddResponseData(first20bytes, SYNCHRONOUS);
  data->AddResponseData(rest, SYNCHRONOUS);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookup2Async) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u), ASYNC);
  data->AddResponseData(base::span(kT0ResponseDatagram).subspan(20u), ASYNC);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncWithAsyncZeroRead) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(kT0ResponseDatagram, ASYNC);
  data->AddResponseData({}, ASYNC);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncWithAsyncZeroRead) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(kT0ResponseDatagram, SYNCHRONOUS);
  data->AddResponseData({}, ASYNC);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenSync) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u), ASYNC);
  data->AddResponseData(base::span(kT0ResponseDatagram).subspan(20u),
                        SYNCHRONOUS);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenSyncError) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u), ASYNC);
  data->AddReadError(ERR_FAILED, SYNCHRONOUS);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupAsyncThenAsyncError) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u), ASYNC);
  data->AddReadError(ERR_FAILED, ASYNC);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncThenAsyncError) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u),
                        SYNCHRONOUS);
  data->AddReadError(ERR_FAILED, ASYNC);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupSyncThenSyncError) {
  ConfigureDohServers(true /* use_post */);
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, SYNCHRONOUS, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponseData(base::span(kT0ResponseDatagram).first(20u),
                        SYNCHRONOUS);
  data->AddReadError(ERR_FAILED, SYNCHRONOUS);
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsNotAvailable) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  ASSERT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  TransactionHelper helper0(ERR_BLOCKED_BY_CLIENT);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsMarkHttpsBad) {
  config_.attempts = 1;
  ConfigureDohServers(true /* use_post */, 3);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram, ASYNC,
                      Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT0HostName, kT0Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT0HostName, kT0Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  TransactionHelper helper0(kT0RecordCount);
  TransactionHelper helper1(kT0RecordCount);

  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();

  // UDP server 0 is our only UDP server, so it will be good. HTTPS
  // servers 0 and 1 failed and will be marked bad. HTTPS server 2 succeeded
  // so it will be good.
  // The expected order of the HTTPS servers is therefore 2, 0, then 1.
  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        resolve_context_->GetClassicDnsIterator(session_->config(),
                                                session_.get());
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());
    EXPECT_TRUE(classic_itr->AttemptAvailable());
    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 2u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
  }
  size_t kOrder0[] = {1, 2, 3};
  CheckServerOrder(kOrder0, std::size(kOrder0));

  helper1.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper1.RunUntilComplete();
  // UDP server 0 is still our only UDP server, so it will be good by
  // definition. HTTPS server 2 started out as good, so it was tried first and
  // failed. HTTPS server 0 then had the oldest failure so it would be the next
  // good server and then it failed so it's marked bad. Next attempt was HTTPS
  // server 1, which succeeded so it's good. The expected order of the HTTPS
  // servers is therefore 1, 2, then 0.

  {
    std::unique_ptr<DnsServerIterator> classic_itr =
        resolve_context_->GetClassicDnsIterator(session_->config(),
                                                session_.get());
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    EXPECT_EQ(classic_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 2u);
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  size_t kOrder1[] = {
      1, 2, 3, /* transaction0 */
      3, 1, 2  /* transaction1 */
  };
  CheckServerOrder(kOrder1, std::size(kOrder1));
}

TEST_F(DnsTransactionTest, HttpsPostFailThenHTTPFallback) {
  ConfigureDohServers(true /* use_post */, 2);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL, ASYNC,
                   Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  size_t kOrder0[] = {1, 2};
  CheckServerOrder(kOrder0, std::size(kOrder0));
}

TEST_F(DnsTransactionTest, HttpsPostFailTwice) {
  config_.attempts = 3;
  ConfigureDohServers(true /* use_post */, 2);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_FAILED);
  SetDohJobMakerCallback(base::BindRepeating(DohJobMakerCallbackFailStart));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  size_t kOrder0[] = {1, 2};
  CheckServerOrder(kOrder0, std::size(kOrder0));
}

TEST_F(DnsTransactionTest, HttpsNotAvailableThenHttpFallback) {
  ConfigureDohServers(true /* use_post */, 2 /* num_doh_servers */,
                      false /* make_available */);

  // Make just server 1 available.
  resolve_context_->RecordServerSuccess(
      1u /* server_index */, true /* is_doh_server*/, session_.get());

  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    EXPECT_FALSE(doh_itr->AttemptAvailable());
  }
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  size_t kOrder0[] = {2};
  CheckServerOrder(kOrder0, std::size(kOrder0));
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    EXPECT_FALSE(doh_itr->AttemptAvailable());
  }
}

// Fail first DoH server, then no fallbacks marked available in AUTOMATIC mode.
TEST_F(DnsTransactionTest, HttpsFailureThenNotAvailable_Automatic) {
  config_.secure_dns_mode = SecureDnsMode::kAutomatic;
  ConfigureDohServers(true /* use_post */, 3 /* num_doh_servers */,
                      false /* make_available */);

  // Make just server 0 available.
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server*/, session_.get());

  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
    EXPECT_FALSE(doh_itr->AttemptAvailable());
  }

  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_CONNECTION_REFUSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();

  // Expect fallback not attempted because other servers not available in
  // AUTOMATIC mode until they have recorded a success.
  size_t kOrder0[] = {1};
  CheckServerOrder(kOrder0, std::size(kOrder0));

  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
    EXPECT_FALSE(doh_itr->AttemptAvailable());
  }
}

// Test a secure transaction failure in SECURE mode when other DoH servers are
// only available for fallback because of
TEST_F(DnsTransactionTest, HttpsFailureThenNotAvailable_Secure) {
  config_.secure_dns_mode = SecureDnsMode::kSecure;
  ConfigureDohServers(true /* use_post */, 3 /* num_doh_servers */,
                      false /* make_available */);

  // Make just server 0 available.
  resolve_context_->RecordServerSuccess(
      0u /* server_index */, true /* is_doh_server*/, session_.get());

  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kSecure, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 1u);
    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 2u);
  }

  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_CONNECTION_REFUSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();

  // Expect fallback to attempt all servers because SECURE mode does not require
  // server availability.
  size_t kOrder0[] = {1, 2, 3};
  CheckServerOrder(kOrder0, std::size(kOrder0));

  // Expect server 0 to be preferred due to least recent failure.
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kSecure, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }
}

TEST_F(DnsTransactionTest, MaxHttpsFailures_NonConsecutive) {
  config_.attempts = 1;
  ConfigureDohServers(false /* use_post */);
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit - 1; i++) {
    AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                             SYNCHRONOUS, Transport::HTTPS,
                             nullptr /* opt_rdata */,
                             DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                             false /* enqueue_transaction_id */);
    TransactionHelper failure(ERR_CONNECTION_REFUSED);
    failure.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                             true /* secure */, resolve_context_.get());
    failure.RunUntilComplete();

    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  // A success should reset the failure counter for DoH.
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper success(kT0RecordCount);
  success.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  success.RunUntilComplete();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  // One more failure should not pass the threshold because failures were reset.
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  TransactionHelper last_failure(ERR_CONNECTION_REFUSED);
  last_failure.StartTransaction(transaction_factory_.get(), kT0HostName,
                                kT0Qtype, true /* secure */,
                                resolve_context_.get());
  last_failure.RunUntilComplete();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }
}

TEST_F(DnsTransactionTest, MaxHttpsFailures_Consecutive) {
  config_.attempts = 1;
  ConfigureDohServers(false /* use_post */);
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit - 1; i++) {
    AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                             SYNCHRONOUS, Transport::HTTPS,
                             nullptr /* opt_rdata */,
                             DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                             false /* enqueue_transaction_id */);
    TransactionHelper failure(ERR_CONNECTION_REFUSED);
    failure.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                             true /* secure */, resolve_context_.get());
    failure.RunUntilComplete();
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  // One more failure should pass the threshold.
  AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                           SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  TransactionHelper last_failure(ERR_CONNECTION_REFUSED);
  last_failure.StartTransaction(transaction_factory_.get(), kT0HostName,
                                kT0Qtype, true /* secure */,
                                resolve_context_.get());
  last_failure.RunUntilComplete();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    EXPECT_FALSE(doh_itr->AttemptAvailable());
  }
}

// Test that a secure transaction started before a DoH server becomes
// unavailable can complete and make the server available again.
TEST_F(DnsTransactionTest, SuccessfulTransactionStartedBeforeUnavailable) {
  ConfigureDohServers(false /* use_post */);
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  // Create a socket data to first return ERR_IO_PENDING. This will pause the
  // response and not return the second response until
  // SequencedSocketData::Resume() is called.
  auto data = std::make_unique<DnsSocketData>(
      0, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  TransactionHelper delayed_success(kT0RecordCount);
  delayed_success.StartTransaction(transaction_factory_.get(), kT0HostName,
                                   kT0Qtype, true /* secure */,
                                   resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(delayed_success.has_completed());

  // Trigger DoH server unavailability with a bunch of failures.
  for (size_t i = 0; i < ResolveContext::kAutomaticModeFailureLimit; i++) {
    AddQueryAndErrorResponse(0, kT0HostName, kT0Qtype, ERR_CONNECTION_REFUSED,
                             SYNCHRONOUS, Transport::HTTPS,
                             nullptr /* opt_rdata */,
                             DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                             false /* enqueue_transaction_id */);
    TransactionHelper failure(ERR_CONNECTION_REFUSED);
    failure.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                             true /* secure */, resolve_context_.get());
    failure.RunUntilComplete();
  }
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Resume first query.
  ASSERT_FALSE(delayed_success.has_completed());
  sequenced_socket_data->Resume();
  delayed_success.RunUntilComplete();

  // Expect DoH server is available again.
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
}

void MakeResponseWithCookie(URLRequest* request, HttpResponseInfo* info) {
  info->headers->AddHeader("Set-Cookie", "test-cookie=you-fail");
}

class CookieCallback {
 public:
  CookieCallback() : loop_to_quit_(std::make_unique<base::RunLoop>()) {}

  void SetCookieCallback(CookieAccessResult result) {
    result_ = result.status.IsInclude();
    loop_to_quit_->Quit();
  }

  CookieCallback(const CookieCallback&) = delete;
  CookieCallback& operator=(const CookieCallback&) = delete;

  void GetCookieListCallback(
      const net::CookieAccessResultList& list,
      const net::CookieAccessResultList& excluded_cookies) {
    list_ = cookie_util::StripAccessResults(list);
    loop_to_quit_->Quit();
  }

  void Reset() { loop_to_quit_ = std::make_unique<base::RunLoop>(); }

  void WaitUntilDone() { loop_to_quit_->Run(); }

  size_t cookie_list_size() { return list_.size(); }

 private:
  net::CookieList list_;
  bool result_ = false;
  std::unique_ptr<base::RunLoop> loop_to_quit_;
};

TEST_F(DnsTransactionTest, HttpsPostTestNoCookies) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  TransactionHelper helper1(kT0RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWithCookie));

  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();

  CookieCallback callback;
  request_context_->cookie_store()->GetCookieListWithOptionsAsync(
      GURL(GetURLFromTemplateWithoutParameters(
          config_.doh_config.servers()[0].server_template())),
      CookieOptions::MakeAllInclusive(), CookiePartitionKeyCollection(),
      base::BindOnce(&CookieCallback::GetCookieListCallback,
                     base::Unretained(&callback)));
  callback.WaitUntilDone();
  EXPECT_EQ(0u, callback.cookie_list_size());
  callback.Reset();
  GURL cookie_url(GetURLFromTemplateWithoutParameters(
      config_.doh_config.servers()[0].server_template()));
  auto cookie = CanonicalCookie::CreateForTesting(
      cookie_url, "test-cookie=you-still-fail", base::Time::Now());
  request_context_->cookie_store()->SetCanonicalCookieAsync(
      std::move(cookie), cookie_url, CookieOptions(),
      base::BindOnce(&CookieCallback::SetCookieCallback,
                     base::Unretained(&callback)));
  helper1.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper1.RunUntilComplete();
}

void MakeResponseWithoutLength(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Length");
}

TEST_F(DnsTransactionTest, HttpsPostNoContentLength) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWithoutLength));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

void MakeResponseWithBadRequestResponse(URLRequest* request,
                                        HttpResponseInfo* info) {
  info->headers->ReplaceStatusLine("HTTP/1.1 400 Bad Request");
}

TEST_F(DnsTransactionTest, HttpsPostWithBadRequestResponse) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(
      base::BindRepeating(MakeResponseWithBadRequestResponse));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

void MakeResponseWrongType(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Type");
  info->headers->AddHeader("Content-Type", "text/html");
}

TEST_F(DnsTransactionTest, HttpsPostWithWrongType) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseWrongType));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

void MakeResponseRedirect(URLRequest* request, HttpResponseInfo* info) {
  if (request->url_chain().size() < 2) {
    info->headers->ReplaceStatusLine("HTTP/1.1 302 Found");
    info->headers->AddHeader("Location",
                             "/redirect-destination?" + request->url().query());
  }
}

TEST_F(DnsTransactionTest, HttpsGetRedirect) {
  ConfigureDohServers(false /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseRedirect));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

void MakeResponseInsecureRedirect(URLRequest* request, HttpResponseInfo* info) {
  if (request->url_chain().size() < 2) {
    info->headers->ReplaceStatusLine("HTTP/1.1 302 Found");
    const std::string location = URLRequestMockDohJob::GetMockHttpUrl(
        "/redirect-destination?" + request->url().query());
    info->headers->AddHeader("Location", location);
  }
}

TEST_F(DnsTransactionTest, HttpsGetRedirectToInsecureProtocol) {
  ConfigureDohServers(/*use_post=*/false);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, /*opt_rdata=*/nullptr,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      /*enqueue_transaction_id=*/false);
  TransactionHelper helper0(ERR_ABORTED);
  SetResponseModifierCallback(
      base::BindRepeating(MakeResponseInsecureRedirect));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           /*secure=*/true, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_EQ(helper0.response(), nullptr);
}

TEST_F(DnsTransactionTest, HttpsGetContentLengthTooLarge) {
  ConfigureDohServers(/*use_post=*/false);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, /*opt_rdata=*/nullptr,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      /*enqueue_transaction_id=*/false);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(base::BindLambdaForTesting(
      [](URLRequest* request, HttpResponseInfo* info) {
        info->headers->AddHeader("Content-Length", "65536");
      }));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           /*secure=*/true, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_EQ(helper0.response(), nullptr);
}

TEST_F(DnsTransactionTest, HttpsGetResponseTooLargeWithoutContentLength) {
  ConfigureDohServers(/*use_post=*/false);
  std::vector<uint8_t> large_response(65536, 0);
  AddQueryAndResponse(
      0, kT0HostName, kT0Qtype, large_response, SYNCHRONOUS, Transport::HTTPS,
      /*opt_rdata=*/nullptr, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
      /*enqueue_transaction_id=*/false);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           /*secure=*/true, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_EQ(helper0.response(), nullptr);
}

void MakeResponseNoType(URLRequest* request, HttpResponseInfo* info) {
  info->headers->RemoveHeader("Content-Type");
}

TEST_F(DnsTransactionTest, HttpsPostWithNoType) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  SetResponseModifierCallback(base::BindRepeating(MakeResponseNoType));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, CanLookupDohServerName) {
  config_.search.push_back("http");
  ConfigureDohServers(true /* use_post */);
  AddQueryAndErrorResponse(0, kMockHostname, dns_protocol::kTypeA,
                           ERR_NAME_NOT_RESOLVED, SYNCHRONOUS, Transport::HTTPS,
                           nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), "mock",
                           dns_protocol::kTypeA, true /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, HttpsPostLookupWithLog) {
  ConfigureDohServers(true /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  NetLogCountingObserver observer;
  NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kEverything);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer.count(), 19);
  EXPECT_EQ(observer.dict_count(), 10);
}

// Test for when a slow DoH response is delayed until after the initial fallback
// period (but succeeds before the full timeout period).
TEST_F(DnsTransactionTestWithMockTime, SlowHttpsResponse_SingleAttempt) {
  config_.doh_attempts = 1;
  ConfigureDohServers(false /* use_post */);

  // Assume fallback period is less than timeout.
  ASSERT_LT(resolve_context_->NextDohFallbackPeriod(0 /* doh_server_index */,
                                                    session_.get()),
            resolve_context_->SecureTransactionTimeout(SecureDnsMode::kSecure,
                                                       session_.get()));

  // Simulate a slow response by using an ERR_IO_PENDING read error to delay
  // until SequencedSocketData::Resume() is called.
  auto data = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  TransactionHelper helper(kT0RecordCount);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);

  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_FALSE(helper.has_completed());

  sequenced_socket_data->Resume();
  helper.RunUntilComplete();
}

// Test for when a slow DoH response is delayed until after the initial fallback
// period but fast timeout is enabled, resulting in timeout failure.
TEST_F(DnsTransactionTestWithMockTime,
       SlowHttpsResponse_SingleAttempt_FastTimeout) {
  config_.doh_attempts = 1;
  ConfigureDohServers(false /* use_post */);

  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          true /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());

  // Only one attempt configured and fast timeout enabled, so expect immediate
  // failure after fallback period.
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_TRUE(helper.has_completed());
}

// Test for when a slow DoH response is delayed until after the initial fallback
// period but a retry is configured.
TEST_F(DnsTransactionTestWithMockTime, SlowHttpsResponse_TwoAttempts) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  // Simulate a slow response by using an ERR_IO_PENDING read error to delay
  // until SequencedSocketData::Resume() is called.
  auto data = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  TransactionHelper helper(kT0RecordCount);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);

  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());
  ASSERT_TRUE(sequenced_socket_data->IsPaused());

  // Another attempt configured, so transaction should not fail after initial
  // fallback period. Setup the second attempt to never receive a response.
  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_FALSE(helper.has_completed());

  // Expect first attempt to continue in parallel with retry, so expect the
  // transaction to complete when the first query is allowed to resume.
  sequenced_socket_data->Resume();
  helper.RunUntilComplete();
}

// Test for when a slow DoH response is delayed until after the full timeout
// period.
TEST_F(DnsTransactionTestWithMockTime, HttpsTimeout) {
  config_.doh_attempts = 1;
  ConfigureDohServers(false /* use_post */);

  // Assume fallback period is less than timeout.
  ASSERT_LT(resolve_context_->NextDohFallbackPeriod(0 /* doh_server_index */,
                                                    session_.get()),
            resolve_context_->SecureTransactionTimeout(SecureDnsMode::kSecure,
                                                       session_.get()));

  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());

  // Stop a tiny bit short to ensure transaction doesn't finish early.
  const base::TimeDelta kTimeHoldback = base::Milliseconds(5);
  base::TimeDelta timeout = resolve_context_->SecureTransactionTimeout(
      SecureDnsMode::kSecure, session_.get());
  ASSERT_LT(kTimeHoldback, timeout);
  FastForwardBy(timeout - kTimeHoldback);
  EXPECT_FALSE(helper.has_completed());

  FastForwardBy(kTimeHoldback);
  EXPECT_TRUE(helper.has_completed());
}

// Test for when two slow DoH responses are delayed until after the full timeout
// period.
TEST_F(DnsTransactionTestWithMockTime, HttpsTimeout2) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  // Assume fallback period is less than timeout.
  ASSERT_LT(resolve_context_->NextDohFallbackPeriod(0 /* doh_server_index */,
                                                    session_.get()),
            resolve_context_->SecureTransactionTimeout(SecureDnsMode::kSecure,
                                                       session_.get()));

  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);
  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());

  base::TimeDelta fallback_period = resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get());
  FastForwardBy(fallback_period);
  EXPECT_FALSE(helper.has_completed());

  // Timeout is from start of transaction, so need to keep track of the
  // remainder after other fast forwards.
  base::TimeDelta timeout = resolve_context_->SecureTransactionTimeout(
      SecureDnsMode::kSecure, session_.get());
  base::TimeDelta timeout_remainder = timeout - fallback_period;

  // Fallback period for second attempt.
  fallback_period = resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get());
  ASSERT_LT(fallback_period, timeout_remainder);
  FastForwardBy(fallback_period);
  EXPECT_FALSE(helper.has_completed());
  timeout_remainder -= fallback_period;

  // Stop a tiny bit short to ensure transaction doesn't finish early.
  const base::TimeDelta kTimeHoldback = base::Milliseconds(5);
  ASSERT_LT(kTimeHoldback, timeout_remainder);
  FastForwardBy(timeout_remainder - kTimeHoldback);
  EXPECT_FALSE(helper.has_completed());

  FastForwardBy(kTimeHoldback);
  EXPECT_TRUE(helper.has_completed());
}

// Test for when attempt fallback periods go beyond the full timeout period.
TEST_F(DnsTransactionTestWithMockTime, LongHttpsTimeouts) {
  const int kNumAttempts = 20;
  config_.doh_attempts = kNumAttempts;
  ConfigureDohServers(false /* use_post */);

  // Assume sum of fallback periods is greater than timeout.
  ASSERT_GT(kNumAttempts * resolve_context_->NextDohFallbackPeriod(
                               0 /* doh_server_index */, session_.get()),
            resolve_context_->SecureTransactionTimeout(SecureDnsMode::kSecure,
                                                       session_.get()));

  for (int i = 0; i < kNumAttempts; ++i) {
    AddHangingQuery(kT0HostName, kT0Qtype,
                    DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                    false /* enqueue_transaction_id */);
  }

  TransactionHelper helper(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(helper.has_completed());

  for (int i = 0; i < kNumAttempts - 1; ++i) {
    FastForwardBy(resolve_context_->NextDohFallbackPeriod(
        0 /* doh_server_index */, session_.get()));
    EXPECT_FALSE(helper.has_completed());
  }

  // Expect transaction to time out immediately after the last fallback period.
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_TRUE(helper.has_completed());
}

// Test for when the last of multiple HTTPS attempts fails (SERVFAIL) before
// a previous attempt succeeds.
TEST_F(DnsTransactionTestWithMockTime, LastHttpsAttemptFails) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  // Simulate a slow response by using an ERR_IO_PENDING read error to delay
  // until SequencedSocketData::Resume() is called.
  auto data = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper(kT0RecordCount);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));

  // Wait for one timeout period to start (and fail) the second attempt.
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_FALSE(helper.has_completed());

  // Complete the first attempt and expect immediate success.
  sequenced_socket_data->Resume();
  helper.RunUntilComplete();
}

// Test for when the last of multiple HTTPS attempts fails (SERVFAIL), and a
// previous attempt never completes.
TEST_F(DnsTransactionTestWithMockTime, LastHttpsAttemptFails_Timeout) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_TIMED_OUT);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);

  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper.has_completed());

  // Second attempt fails immediately after first fallback period, but because
  // fast timeout is disabled, the transaction will attempt to wait for the
  // first attempt.
  base::TimeDelta fallback_period = resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get());
  FastForwardBy(fallback_period);
  EXPECT_FALSE(helper.has_completed());

  // Timeout is from start of transaction, so need to keep track of the
  // remainder after other fast forwards.
  base::TimeDelta timeout = resolve_context_->SecureTransactionTimeout(
      SecureDnsMode::kSecure, session_.get());
  base::TimeDelta timeout_remainder = timeout - fallback_period;

  // Stop a tiny bit short to ensure transaction doesn't finish early.
  const base::TimeDelta kTimeHoldback = base::Milliseconds(5);
  ASSERT_LT(kTimeHoldback, timeout_remainder);
  FastForwardBy(timeout_remainder - kTimeHoldback);
  EXPECT_FALSE(helper.has_completed());

  FastForwardBy(kTimeHoldback);
  EXPECT_TRUE(helper.has_completed());
}

// Test for when the last of multiple HTTPS attempts fails (SERVFAIL) before
// a previous attempt can complete, but fast timeouts is enabled.
TEST_F(DnsTransactionTestWithMockTime, LastHttpsAttemptFails_FastTimeout) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  AddHangingQuery(kT0HostName, kT0Qtype,
                  DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                  false /* enqueue_transaction_id */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_SERVER_FAILED);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          true /* fast_timeout */);

  helper.StartTransaction(std::move(transaction));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper.has_completed());

  // With fast timeout enabled, expect the transaction to complete with failure
  // immediately on failure of the last transaction.
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_TRUE(helper.has_completed());
}

// Test for when the last of multiple HTTPS attempts fails (SERVFAIL) before
// a previous attempt later fails as well.
TEST_F(DnsTransactionTestWithMockTime, LastHttpsAttemptFailsFirst) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  // Simulate a slow response by using an ERR_IO_PENDING read error to delay
  // until SequencedSocketData::Resume() is called.
  auto data = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddRcode(dns_protocol::kRcodeSERVFAIL, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_SERVER_FAILED);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));

  // Wait for one timeout period to start (and fail) the second attempt.
  FastForwardBy(resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get()));
  EXPECT_FALSE(helper.has_completed());

  // Complete the first attempt and expect immediate completion.
  sequenced_socket_data->Resume();
  helper.RunUntilComplete();
}

// Test for when multiple HTTPS attempts fail (SERVFAIL) in order, making the
// last started attempt also the last attempt to be pending.
TEST_F(DnsTransactionTestWithMockTime, LastHttpsAttemptFailsLast) {
  config_.doh_attempts = 2;
  ConfigureDohServers(false /* use_post */);

  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, 0 /* id */,
                   false /* enqueue_transaction_id */);

  TransactionHelper helper(ERR_DNS_SERVER_FAILED);
  std::unique_ptr<DnsTransaction> transaction =
      transaction_factory_->CreateTransaction(
          kT0HostName, kT0Qtype, NetLogWithSource(), true /* secure */,
          SecureDnsMode::kSecure, resolve_context_.get(),
          false /* fast_timeout */);
  helper.StartTransaction(std::move(transaction));

  // Expect both attempts will run quickly without waiting for fallbacks or
  // transaction timeout.
  helper.RunUntilComplete();
}

TEST_F(DnsTransactionTest, TcpLookup_UdpRetry) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      ASYNC, Transport::TCP);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, TcpLookup_UdpRetry_WithLog) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      ASYNC, Transport::TCP);

  TransactionHelper helper0(kT0RecordCount);
  NetLogCountingObserver observer;
  NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kEverything);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  EXPECT_EQ(observer.count(), 9);
  EXPECT_EQ(observer.dict_count(), 7);
}

TEST_F(DnsTransactionTest, TcpLookup_LowEntropy) {
  socket_factory_->diverse_source_ports_ = false;

  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                        ASYNC, Transport::UDP);
  }

  AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      ASYNC, Transport::TCP);

  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    TransactionHelper udp_helper(kT0RecordCount);
    udp_helper.StartTransaction(transaction_factory_.get(), kT0HostName,
                                kT0Qtype, false /* secure */,
                                resolve_context_.get());
    udp_helper.RunUntilComplete();
  }

  ASSERT_TRUE(session_->udp_tracker()->low_entropy());

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  EXPECT_TRUE(session_->udp_tracker()->low_entropy());
}

TEST_F(DnsTransactionTest, TCPFailure) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeSERVFAIL, ASYNC,
                   Transport::TCP);

  TransactionHelper helper0(ERR_DNS_SERVER_FAILED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
  ASSERT_NE(helper0.response(), nullptr);
  EXPECT_EQ(helper0.response()->rcode(), dns_protocol::kRcodeSERVFAIL);
}

TEST_F(DnsTransactionTest, TCPMalformed) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              ASYNC, Transport::TCP);
  // Valid response but length too short.
  // This must be truncated in the question section. The DnsResponse doesn't
  // examine the answer section until asked to parse it, so truncating it in
  // the answer section would result in the DnsTransaction itself succeeding.
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(kT0ResponseDatagram, 0), ASYNC,
      static_cast<uint16_t>(kT0QuerySize - 1));
  AddSocketData(std::move(data));

  TransactionHelper helper0(ERR_DNS_MALFORMED_RESPONSE);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTestWithMockTime, TcpTimeout_UdpRetry) {
  ConfigureFactory();
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  AddSocketData(std::make_unique<DnsSocketData>(
      1 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));

  TransactionHelper helper0(ERR_DNS_TIMED_OUT);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTestWithMockTime, TcpTimeout_LowEntropy) {
  ConfigureFactory();
  socket_factory_->diverse_source_ports_ = false;

  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                        ASYNC, Transport::UDP);
  }

  AddSocketData(std::make_unique<DnsSocketData>(
      1 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::TCP));

  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    TransactionHelper udp_helper(kT0RecordCount);
    udp_helper.StartTransaction(transaction_factory_.get(), kT0HostName,
                                kT0Qtype, false /* secure */,
                                resolve_context_.get());
    udp_helper.RunUntilComplete();
  }

  ASSERT_TRUE(session_->udp_tracker()->low_entropy());

  TransactionHelper helper0(ERR_DNS_TIMED_OUT);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());
  FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(helper0.has_completed());
}

TEST_F(DnsTransactionTest, TCPReadReturnsZeroAsync) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              ASYNC, Transport::TCP);
  // Return all but the last byte of the response.
  auto response_data = base::span(kT0ResponseDatagram);
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(
          response_data.first(response_data.size() - 1), 0),
      ASYNC, static_cast<uint16_t>(response_data.size()));
  // Then return a 0-length read.
  data->AddReadError(0, ASYNC);
  AddSocketData(std::move(data));

  TransactionHelper helper0(ERR_CONNECTION_CLOSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, TCPReadReturnsZeroSynchronous) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              ASYNC, Transport::TCP);
  auto response_data = base::span(kT0ResponseDatagram);
  // Return all but the last byte of the response.
  data->AddResponseWithLength(
      std::make_unique<DnsResponse>(
          response_data.first(response_data.size() - 1), 0),
      SYNCHRONOUS, static_cast<uint16_t>(response_data.size()));
  // Then return a 0-length read.
  data->AddReadError(0, SYNCHRONOUS);
  AddSocketData(std::move(data));

  TransactionHelper helper0(ERR_CONNECTION_CLOSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, TCPConnectionClosedAsync) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              ASYNC, Transport::TCP);
  data->AddReadError(ERR_CONNECTION_CLOSED, ASYNC);
  AddSocketData(std::move(data));

  TransactionHelper helper0(ERR_CONNECTION_CLOSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, TCPConnectionClosedSynchronous) {
  AddAsyncQueryAndRcode(kT0HostName, kT0Qtype,
                        dns_protocol::kRcodeNOERROR | dns_protocol::kFlagTC);
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              ASYNC, Transport::TCP);
  data->AddReadError(ERR_CONNECTION_CLOSED, SYNCHRONOUS);
  AddSocketData(std::move(data));

  TransactionHelper helper0(ERR_CONNECTION_CLOSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, MismatchedThenNxdomainThenTCP) {
  config_.attempts = 2;
  ConfigureFactory();
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              SYNCHRONOUS, Transport::UDP);
  // First attempt gets a mismatched response.
  data->AddResponseData(kT1ResponseDatagram, SYNCHRONOUS);
  // Second read from first attempt gets TCP required.
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));
  // Second attempt gets NXDOMAIN, which happens before the TCP required.
  AddSyncQueryAndRcode(kT0HostName, kT0Qtype, dns_protocol::kRcodeNXDOMAIN);

  TransactionHelper helper0(ERR_NAME_NOT_RESOLVED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, MismatchedThenOkThenTCP) {
  config_.attempts = 2;
  ConfigureFactory();
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              SYNCHRONOUS, Transport::UDP);
  // First attempt gets a mismatched response.
  data->AddResponseData(kT1ResponseDatagram, SYNCHRONOUS);
  // Second read from first attempt gets TCP required.
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));
  // Second attempt gets a valid response, which happens before the TCP
  // required.
  AddSyncQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype,
                          kT0ResponseDatagram);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
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
  auto data = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName, kT0Qtype,
                                              SYNCHRONOUS, Transport::UDP);
  data->AddResponseData(kT1ResponseDatagram, SYNCHRONOUS);
  data->AddRcode(dns_protocol::kFlagTC, ASYNC);
  AddSocketData(std::move(data));

  // Attempt 2.
  AddQueryAndErrorResponse(0 /* id */, kT0HostName, kT0Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS, Transport::UDP);

  TransactionHelper helper0(ERR_CONNECTION_REFUSED);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  helper0.RunUntilComplete();
}

TEST_F(DnsTransactionTest, InvalidQuery) {
  ConfigureFactory();

  TransactionHelper helper0(ERR_INVALID_ARGUMENT);
  helper0.StartTransaction(transaction_factory_.get(), ".",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();

  TransactionHelper helper1(ERR_INVALID_ARGUMENT);
  helper1.StartTransaction(transaction_factory_.get(), "foo,bar.com",
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper1.RunUntilComplete();
}

TEST_F(DnsTransactionTest, CheckAsync) {
  ConfigureDohServers(false /* use_post */);
  AddQueryAndResponse(0, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  TransactionHelper helper0(kT0RecordCount);
  bool started = false;
  SetUrlRequestStartedCallback(
      base::BindLambdaForTesting([&] { started = true; }));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  EXPECT_FALSE(started);
  EXPECT_FALSE(helper0.has_completed());
  helper0.RunUntilComplete();
  EXPECT_TRUE(started);
}

TEST_F(DnsTransactionTest, EarlyCancel) {
  ConfigureDohServers(false /* use_post */);
  TransactionHelper helper0(0);
  SetUrlRequestStartedCallback(base::BindRepeating([] { FAIL(); }));
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  EXPECT_FALSE(helper0.has_completed());
  helper0.Cancel();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DnsTransactionTestWithMockTime, ProbeUntilSuccess) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  ASSERT_EQ(kDohProbeHostname, kT4HostName);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(false /* network_change */);

  // The first probe happens without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  // Expect the server to still be unavailable after the second probe.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  EXPECT_FALSE(doh_itr->AttemptAvailable());

  // Expect the server to be available after the successful third probe.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
}

TEST_F(DnsTransactionTestWithMockTime, ProbeCreationTriggersSuccessMetric) {
  config_.secure_dns_mode = SecureDnsMode::kAutomatic;
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  ASSERT_EQ(kDohProbeHostname, kT4HostName);
  AddQueryAndResponse(/*id=*/0, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS,
                      /*opt_rdata=*/nullptr,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      /*enqueue_transaction_id=*/false);

  // The metric timer should not have started yet.
  EXPECT_FALSE(
      resolve_context_->doh_autoupgrade_metrics_timer_is_running_for_testing());

  base::HistogramTester histogram_tester;
  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  // Ensure that calling `CreateDohProbeRunner()` causes metrics to be emitted
  // after the timeout.
  EXPECT_TRUE(
      resolve_context_->doh_autoupgrade_metrics_timer_is_running_for_testing());

  // Fast-forward by enough time for the timer to trigger. Add one millisecond
  // just to make it clear that afterwards the timeout should definitely have
  // occurred (although this may not be strictly necessary).
  FastForwardBy(ResolveContext::kDohAutoupgradeSuccessMetricTimeout +
                base::Milliseconds(1));

  EXPECT_FALSE(
      resolve_context_->doh_autoupgrade_metrics_timer_is_running_for_testing());

  histogram_tester.ExpectUniqueSample(
      "Net.DNS.ResolveContext.DohAutoupgrade.Other.Status",
      DohServerAutoupgradeStatus::kSuccessWithNoPriorFailures, 1);
}

TEST_F(DnsTransactionTestWithMockTime,
       ProbeAttemptConnectionFailureAffectsHistograms) {
  config_.secure_dns_mode = SecureDnsMode::kAutomatic;
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  ASSERT_EQ(kDohProbeHostname, kT4HostName);
  AddQueryAndErrorResponse(/*id=*/0, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, /*opt_rdata=*/nullptr,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           /*enqueue_transaction_id=*/false);

  base::HistogramTester histogram_tester;
  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  // Consume the one failure response and then destroy the probe so it doesn't
  // continue to make requests.
  RunUntilIdle();
  runner = nullptr;

  FastForwardBy(ResolveContext::kDohAutoupgradeSuccessMetricTimeout +
                base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.ResolveContext.DohAutoupgrade.Other.Status",
      DohServerAutoupgradeStatus::kFailureWithNoPriorSuccesses, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.DnsTransaction.SecureNotValidated.Other.FailureError",
      std::abs(Error::ERR_CONNECTION_REFUSED), 1);
}

TEST_F(DnsTransactionTestWithMockTime, ProbeAttemptServFailAffectsHistograms) {
  config_.secure_dns_mode = SecureDnsMode::kAutomatic;
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  ASSERT_EQ(kDohProbeHostname, kT4HostName);
  AddQueryAndRcode(kT4HostName, kT4Qtype, dns_protocol::kRcodeSERVFAIL,
                   SYNCHRONOUS, Transport::HTTPS,
                   DnsQuery::PaddingStrategy::BLOCK_LENGTH_128, /*id=*/0,
                   /*enqueue_transaction_id=*/false);

  base::HistogramTester histogram_tester;
  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  RunUntilIdle();
  runner = nullptr;

  FastForwardBy(ResolveContext::kDohAutoupgradeSuccessMetricTimeout +
                base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.ResolveContext.DohAutoupgrade.Other.Status",
      DohServerAutoupgradeStatus::kFailureWithNoPriorSuccesses, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.DnsTransaction.SecureNotValidated.Other.FailureError",
      std::abs(Error::ERR_DNS_SERVER_FAILED), 1);
}

TEST_F(DnsTransactionTestWithMockTime,
       ProbeAttemptEmptyResponseAffectsHistograms) {
  config_.secure_dns_mode = SecureDnsMode::kAutomatic;
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  ASSERT_EQ(kDohProbeHostname, kT4HostName);
  auto response = std::make_unique<DnsResponse>(
      BuildTestDnsResponse(kT4HostName, dns_protocol::kTypeA,
                           /*answers=*/{}));
  auto data = std::make_unique<DnsSocketData>(
      /*id=*/0, kT4HostName, dns_protocol::kTypeA, SYNCHRONOUS,
      Transport::HTTPS, /*opt_rdata=*/nullptr,
      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddResponse(std::move(response), SYNCHRONOUS);
  AddSocketData(std::move(data), /*enqueue_transaction_id=*/false);

  base::HistogramTester histogram_tester;
  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  RunUntilIdle();
  runner = nullptr;

  FastForwardBy(ResolveContext::kDohAutoupgradeSuccessMetricTimeout +
                base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.ResolveContext.DohAutoupgrade.Other.Status",
      DohServerAutoupgradeStatus::kFailureWithNoPriorSuccesses, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.DNS.DnsTransaction.SecureNotValidated.Other.FailureError",
      std::abs(Error::ERR_DNS_SECURE_PROBE_RECORD_INVALID), 1);
}

// Test that if a probe attempt hangs, additional probes will still run on
// schedule
TEST_F(DnsTransactionTestWithMockTime, HungProbe) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);

  // Create a socket data to first return ERR_IO_PENDING. This will pause the
  // probe and not return the error until SequencedSocketData::Resume() is
  // called.
  auto data = std::make_unique<DnsSocketData>(
      0 /* id */, kT4HostName, kT4Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data->AddReadError(ERR_IO_PENDING, ASYNC);
  data->AddReadError(ERR_CONNECTION_REFUSED, ASYNC);
  data->AddResponseData(kT4ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data = data->GetProvider();
  AddSocketData(std::move(data), false /* enqueue_transaction_id */);

  // Add success for second probe.
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(false /* network_change */);

  // The first probe starts without any delay, but doesn't finish.
  RunUntilIdle();
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Second probe succeeds.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Probe runner self-cancels on next cycle.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0), base::TimeDelta());

  // Expect no effect when the hung probe wakes up and fails.
  sequenced_socket_data->Resume();
  RunUntilIdle();
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, ProbeMultipleServers) {
  ConfigureDohServers(true /* use_post */, 2 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  ASSERT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
  ASSERT_FALSE(resolve_context_->GetDohServerAvailability(
      1u /* doh_server_index */, session_.get()));

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(true /* network_change */);

  // The first probes happens without any delay and succeeds for only one server
  RunUntilIdle();
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      1u /* doh_server_index */, session_.get()));

  // On second round of probing, probes for first server should self-cancel and
  // second server should become available.
  FastForwardBy(
      runner->GetDelayUntilNextProbeForTest(0u /* doh_server_index */));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0u /* doh_server_index */),
            base::TimeDelta());
  FastForwardBy(
      runner->GetDelayUntilNextProbeForTest(1u /* doh_server_index */));
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      1u /* doh_server_index */, session_.get()));

  // Expect server 2 probes to self-cancel on next cycle.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(1u));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(1u), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, MultipleProbeRunners) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner1 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  std::unique_ptr<DnsProbeRunner> runner2 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner1->Start(true /* network_change */);
  runner2->Start(true /* network_change */);

  // The first two probes (one for each runner) happen without any delay
  // and mark the first server good.
  RunUntilIdle();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  // Both probes expected to self-cancel on next scheduled run.
  FastForwardBy(runner1->GetDelayUntilNextProbeForTest(0));
  FastForwardBy(runner2->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner1->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
  EXPECT_EQ(runner2->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, MultipleProbeRunners_SeparateContexts) {
  // Each RequestContext uses its own transient IsolationInfo. Since there's
  // typically only one RequestContext per URLRequestContext, there's no
  // advantage in using the same IsolationInfo across RequestContexts.
  set_expect_multiple_isolation_infos(true);

  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  auto request_context2 = CreateTestURLRequestContextBuilder()->Build();
  ResolveContext context2(request_context2.get(), false /* enable_caching */);
  context2.InvalidateCachesAndPerSessionData(session_.get(),
                                             false /* network_change */);

  std::unique_ptr<DnsProbeRunner> runner1 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  std::unique_ptr<DnsProbeRunner> runner2 =
      transaction_factory_->CreateDohProbeRunner(&context2);
  runner1->Start(false /* network_change */);
  runner2->Start(false /* network_change */);

  // The first two probes (one for each runner) happen without any delay.
  // Probe for first context succeeds and second fails.
  RunUntilIdle();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }
  {
    std::unique_ptr<DnsServerIterator> doh_itr2 = context2.GetDohIterator(
        session_->config(), SecureDnsMode::kAutomatic, session_.get());

    EXPECT_FALSE(doh_itr2->AttemptAvailable());
  }

  // First probe runner expected to be compete and self-cancel on next run.
  FastForwardBy(runner1->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner1->GetDelayUntilNextProbeForTest(0), base::TimeDelta());

  // Expect second runner to succeed on its second probe.
  FastForwardBy(runner2->GetDelayUntilNextProbeForTest(0));
  {
    std::unique_ptr<DnsServerIterator> doh_itr2 = context2.GetDohIterator(
        session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr2->AttemptAvailable());
    EXPECT_EQ(doh_itr2->GetNextAttemptIndex(), 0u);
  }
  FastForwardBy(runner2->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner2->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, CancelDohProbeOnDestruction) {
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  AddQueryAndErrorResponse(/*id=*/0, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, /*opt_rdata=*/nullptr,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           /*enqueue_transaction_id=*/false);
  AddQueryAndErrorResponse(/*id=*/0, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, /*opt_rdata=*/nullptr,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           /* enqueue_transaction_id=*/false);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  // The first probe happens without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  // Expect the server to still be unavailable after the second probe.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  base::TimeDelta next_delay = runner->GetDelayUntilNextProbeForTest(0);
  runner.reset();

  // Server stays unavailable because probe canceled before (non-existent)
  // success. No success result is added, so this FastForward will cause a
  // failure if probes attempt to run.
  FastForwardBy(next_delay);

  EXPECT_FALSE(doh_itr->AttemptAvailable());
}

TEST_F(DnsTransactionTestWithMockTime, CancelDohProbeOnContextDestruction) {
  ConfigureDohServers(/*use_post=*/true, /*num_doh_servers=*/1,
                      /*make_available=*/false);
  AddQueryAndErrorResponse(/*id=*/0, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, /*opt_rdata=*/nullptr,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           /*enqueue_transaction_id=*/false);
  AddQueryAndErrorResponse(/*id=*/0, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, /*opt_rdata=*/nullptr,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           /* enqueue_transaction_id=*/false);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(/*network_change=*/false);

  // The first probe happens without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  // Expect the server to still be unavailable after the second probe.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  base::TimeDelta next_delay = runner->GetDelayUntilNextProbeForTest(0);
  resolve_context_.reset();

  // The probe detects that the context no longer exists and stops running.
  FastForwardBy(next_delay);

  // There are no more probes to run.
  EXPECT_EQ(base::TimeDelta(), runner->GetDelayUntilNextProbeForTest(0));
}

TEST_F(DnsTransactionTestWithMockTime, CancelOneOfMultipleProbeRunners) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner1 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  std::unique_ptr<DnsProbeRunner> runner2 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner1->Start(true /* network_change */);
  runner2->Start(true /* network_change */);

  // The first two probes (one for each runner) happen without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
  EXPECT_GT(runner1->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
  EXPECT_GT(runner2->GetDelayUntilNextProbeForTest(0), base::TimeDelta());

  // Cancel only one probe runner.
  runner1.reset();

  // Expect the server to be available after the successful third probe.
  FastForwardBy(runner2->GetDelayUntilNextProbeForTest(0));

  ASSERT_TRUE(doh_itr->AttemptAvailable());
  EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  FastForwardBy(runner2->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner2->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, CancelAllOfMultipleProbeRunners) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner1 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  std::unique_ptr<DnsProbeRunner> runner2 =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner1->Start(false /* network_change */);
  runner2->Start(false /* network_change */);

  // The first two probes (one for each runner) happen without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());
  EXPECT_GT(runner1->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
  EXPECT_GT(runner2->GetDelayUntilNextProbeForTest(0), base::TimeDelta());

  base::TimeDelta next_delay = runner1->GetDelayUntilNextProbeForTest(0);
  runner1.reset();
  runner2.reset();

  // Server stays unavailable because probe canceled before (non-existent)
  // success. No success result is added, so this FastForward will cause a
  // failure if probes attempt to run.
  FastForwardBy(next_delay);
  EXPECT_FALSE(doh_itr->AttemptAvailable());
}

TEST_F(DnsTransactionTestWithMockTime, CancelDohProbe_AfterSuccess) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      SYNCHRONOUS, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(true /* network_change */);

  // The first probe happens without any delay, and immediately succeeds.
  RunUntilIdle();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }

  runner.reset();

  // No change expected after cancellation.
  RunUntilIdle();
  {
    std::unique_ptr<DnsServerIterator> doh_itr =
        resolve_context_->GetDohIterator(
            session_->config(), SecureDnsMode::kAutomatic, session_.get());

    ASSERT_TRUE(doh_itr->AttemptAvailable());
    EXPECT_EQ(doh_itr->GetNextAttemptIndex(), 0u);
  }
}

TEST_F(DnsTransactionTestWithMockTime, DestroyFactoryAfterStartingDohProbe) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(false /* network_change */);

  // The first probe happens without any delay.
  RunUntilIdle();
  std::unique_ptr<DnsServerIterator> doh_itr = resolve_context_->GetDohIterator(
      session_->config(), SecureDnsMode::kAutomatic, session_.get());

  EXPECT_FALSE(doh_itr->AttemptAvailable());

  // Destroy factory and session.
  transaction_factory_.reset();
  ASSERT_TRUE(session_->HasOneRef());
  session_.reset();

  // Probe should not encounter issues and should stop running.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0), base::TimeDelta());
}

TEST_F(DnsTransactionTestWithMockTime, StartWhileRunning) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndErrorResponse(0 /* id */, kT4HostName, kT4Qtype,
                           ERR_CONNECTION_REFUSED, SYNCHRONOUS,
                           Transport::HTTPS, nullptr /* opt_rdata */,
                           DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                           false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(false /* network_change */);

  // The first probe happens without any delay.
  RunUntilIdle();
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Extra Start() call should have no effect because runner is already running.
  runner->Start(true /* network_change */);
  RunUntilIdle();
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Expect the server to be available after the successful second probe.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0));
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
}

TEST_F(DnsTransactionTestWithMockTime, RestartFinishedProbe) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(true /* network_change */);

  // The first probe happens without any delay and succeeds.
  RunUntilIdle();
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Expect runner to self-cancel on next cycle.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0u));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0u), base::TimeDelta());

  // Mark server unavailabe and restart runner.
  for (int i = 0; i < ResolveContext::kAutomaticModeFailureLimit; ++i) {
    resolve_context_->RecordServerFailure(0u /* server_index */,
                                          true /* is_doh_server */, ERR_FAILED,
                                          session_.get());
  }
  ASSERT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
  runner->Start(false /* network_change */);

  // Expect the server to be available again after a successful immediately-run
  // probe.
  RunUntilIdle();
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Expect self-cancel again.
  FastForwardBy(runner->GetDelayUntilNextProbeForTest(0u));
  EXPECT_EQ(runner->GetDelayUntilNextProbeForTest(0u), base::TimeDelta());
}

// Test that a probe runner keeps running on the same schedule if it completes
// but the server is marked unavailable again before the next scheduled probe.
TEST_F(DnsTransactionTestWithMockTime, FastProbeRestart) {
  ConfigureDohServers(true /* use_post */, 1 /* num_doh_servers */,
                      false /* make_available */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);
  AddQueryAndResponse(0 /* id */, kT4HostName, kT4Qtype, kT4ResponseDatagram,
                      ASYNC, Transport::HTTPS, nullptr /* opt_rdata */,
                      DnsQuery::PaddingStrategy::BLOCK_LENGTH_128,
                      false /* enqueue_transaction_id */);

  std::unique_ptr<DnsProbeRunner> runner =
      transaction_factory_->CreateDohProbeRunner(resolve_context_.get());
  runner->Start(true /* network_change */);

  // The first probe happens without any delay and succeeds.
  RunUntilIdle();
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  base::TimeDelta scheduled_delay = runner->GetDelayUntilNextProbeForTest(0);
  EXPECT_GT(scheduled_delay, base::TimeDelta());

  // Mark server unavailabe and restart runner. Note that restarting the runner
  // is unnecessary, but a Start() call should always happen on a server
  // becoming unavailable and might as well replecate real behavior for the
  // test.
  for (int i = 0; i < ResolveContext::kAutomaticModeFailureLimit; ++i) {
    resolve_context_->RecordServerFailure(0u /* server_index */,
                                          true /* is_doh_server */, ERR_FAILED,
                                          session_.get());
  }
  ASSERT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
  runner->Start(false /* network_change */);

  // Probe should not run until scheduled delay.
  RunUntilIdle();
  EXPECT_FALSE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));

  // Expect the probe to run again and succeed after scheduled delay.
  FastForwardBy(scheduled_delay);
  EXPECT_TRUE(resolve_context_->GetDohServerAvailability(
      0u /* doh_server_index */, session_.get()));
}

// Test that queries cannot be sent when they contain a too-long name.
// Tests against incorrect name length validation, which is anti-pattern #3 from
// the "NAME:WRECK" report:
// https://www.forescout.com/company/resources/namewreck-breaking-and-fixing-dns-implementations/
TEST_F(DnsTransactionTestWithMockTime, RejectsQueryingLongNames) {
  std::string long_dotted_name;
  while (long_dotted_name.size() <= dns_protocol::kMaxNameLength) {
    long_dotted_name.append("naaaaaamelabel.");
  }
  long_dotted_name.append("test");

  TransactionHelper helper0(ERR_INVALID_ARGUMENT);
  helper0.StartTransaction(transaction_factory_.get(), long_dotted_name.c_str(),
                           dns_protocol::kTypeA, false /* secure */,
                           resolve_context_.get());
  helper0.RunUntilComplete();
}

// Test that ERR_CONNECTION_REFUSED error after fallback of DnsTCPAttempt
// should not cause DCHECK failure (https://crbug.com/1334250).
TEST_F(DnsTransactionTestWithMockTime, TcpConnectionRefusedAfterFallback) {
  ConfigureNumServers(2);
  ConfigureFactory();
  socket_factory_->diverse_source_ports_ = false;

  // Data for UDP attempts to set `low_entropy` flag.
  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    AddQueryAndResponse(0 /* id */, kT0HostName, kT0Qtype, kT0ResponseDatagram,
                        ASYNC, Transport::UDP);
  }

  // Data for TCP attempt.
  auto data1 = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName,
                                               kT0Qtype, ASYNC, Transport::TCP);
  data1->AddReadError(ERR_IO_PENDING, ASYNC);
  data1->AddReadError(ERR_CONNECTION_REFUSED, ASYNC);
  SequencedSocketData* sequenced_socket_data1 = data1->GetProvider();
  AddSocketData(std::move(data1));

  auto data2 = std::make_unique<DnsSocketData>(0 /* id */, kT0HostName,
                                               kT0Qtype, ASYNC, Transport::TCP);
  data2->AddReadError(ERR_IO_PENDING, ASYNC);
  data2->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data2 = data2->GetProvider();
  AddSocketData(std::move(data2));

  // DNS transactions for UDP attempts to set `low_entropy` flag.
  for (int i = 0; i <= DnsUdpTracker::kPortReuseThreshold; ++i) {
    TransactionHelper udp_helper(kT0RecordCount);
    udp_helper.StartTransaction(transaction_factory_.get(), kT0HostName,
                                kT0Qtype, false /* secure */,
                                resolve_context_.get());
    udp_helper.RunUntilComplete();
  }

  ASSERT_TRUE(session_->udp_tracker()->low_entropy());

  // DNS transactions for TCP attempt.
  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           false /* secure */, resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());

  base::TimeDelta timeout = resolve_context_->NextClassicFallbackPeriod(
      0 /* classic_server_index */, 0 /* attempt */, session_.get());
  FastForwardBy(timeout);

  // Resume the first query.
  sequenced_socket_data1->Resume();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());

  // Resume the second query.
  sequenced_socket_data2->Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(helper0.has_completed());
}

// Test that ERR_CONNECTION_REFUSED error after fallback of DnsHTTPAttempt
// should not cause DCHECK failure (https://crbug.com/1334250).
TEST_F(DnsTransactionTestWithMockTime, HttpsConnectionRefusedAfterFallback) {
  ConfigureDohServers(false /* use_post */, 2 /* num_doh_servers */,
                      true /* make_available */);

  auto data1 = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data1->AddReadError(ERR_IO_PENDING, ASYNC);
  data1->AddReadError(ERR_CONNECTION_REFUSED, ASYNC);
  SequencedSocketData* sequenced_socket_data1 = data1->GetProvider();
  AddSocketData(std::move(data1), false /* enqueue_transaction_id */);

  auto data2 = std::make_unique<DnsSocketData>(
      0 /* id */, kT0HostName, kT0Qtype, ASYNC, Transport::HTTPS,
      nullptr /* opt_rdata */, DnsQuery::PaddingStrategy::BLOCK_LENGTH_128);
  data2->AddReadError(ERR_IO_PENDING, ASYNC);
  data2->AddResponseData(kT0ResponseDatagram, ASYNC);
  SequencedSocketData* sequenced_socket_data2 = data2->GetProvider();
  AddSocketData(std::move(data2), false /* enqueue_transaction_id */);

  TransactionHelper helper0(kT0RecordCount);
  helper0.StartTransaction(transaction_factory_.get(), kT0HostName, kT0Qtype,
                           true /* secure */, resolve_context_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());

  base::TimeDelta timeout = resolve_context_->NextDohFallbackPeriod(
      0 /* doh_server_index */, session_.get());
  FastForwardBy(timeout);

  // Resume the first query.
  sequenced_socket_data1->Resume();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper0.has_completed());

  // Resume the second query.
  sequenced_socket_data2->Resume();

  EXPECT_TRUE(helper0.has_completed());
}

}  // namespace

}  // namespace net
