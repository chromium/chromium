// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
#define NET_SPDY_SPDY_TEST_UTIL_COMMON_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "net/base/completion_once_callback.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/transport_security_state.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_service.h"
#endif

class GURL;

namespace net {

class CTVerifier;
class CTPolicyEnforcer;
class HashValue;
class HostPortPair;
class HostResolver;
class QuicContext;
class HttpUserAgentSettings;
class NetLogWithSource;
class SpdySessionKey;
class SpdyStream;
class SpdyStreamRequest;
class TransportSecurityState;

// Default upload data used by both, mock objects and framer when creating
// data frames.
const char kDefaultUrl[] = "https://www.example.org/";
const char kUploadData[] = "hello!";
const int kUploadDataSize = base::size(kUploadData) - 1;

// While HTTP/2 protocol defines default SETTINGS_MAX_HEADER_LIST_SIZE_FOR_TEST
// to be unlimited, BufferedSpdyFramer constructor requires a value.
const uint32_t kMaxHeaderListSizeForTest = 1024;

// Chop a spdy::SpdySerializedFrame into an array of MockWrites.
// |frame| is the frame to chop.
// |num_chunks| is the number of chunks to create.
std::unique_ptr<MockWrite[]> ChopWriteFrame(
    const spdy::SpdySerializedFrame& frame,
    int num_chunks);

// Adds headers and values to a map.
// |extra_headers| is an array of { name, value } pairs, arranged as strings
// where the even entries are the header names, and the odd entries are the
// header values.
// |headers| gets filled in from |extra_headers|.
void AppendToHeaderBlock(const char* const extra_headers[],
                         int extra_header_count,
                         spdy::SpdyHeaderBlock* headers);

// Create an async MockWrite from the given spdy::SpdySerializedFrame.
MockWrite CreateMockWrite(const spdy::SpdySerializedFrame& req);

// Create an async MockWrite from the given spdy::SpdySerializedFrame and
// sequence number.
MockWrite CreateMockWrite(const spdy::SpdySerializedFrame& req, int seq);

MockWrite CreateMockWrite(const spdy::SpdySerializedFrame& req,
                          int seq,
                          IoMode mode);

// Create a MockRead from the given spdy::SpdySerializedFrame.
MockRead CreateMockRead(const spdy::SpdySerializedFrame& resp);

// Create a MockRead from the given spdy::SpdySerializedFrame and sequence
// number.
MockRead CreateMockRead(const spdy::SpdySerializedFrame& resp, int seq);

MockRead CreateMockRead(const spdy::SpdySerializedFrame& resp,
                        int seq,
                        IoMode mode);

// Combines the given vector of spdy::SpdySerializedFrame into a single frame.
spdy::SpdySerializedFrame CombineFrames(
    std::vector<const spdy::SpdySerializedFrame*> frames);

// Returns the spdy::SpdyPriority embedded in the given frame.  Returns true
// and fills in |priority| on success.
bool GetSpdyPriority(const spdy::SpdySerializedFrame& frame,
                     spdy::SpdyPriority* priority);

// Tries to create a stream in |session| synchronously. Returns NULL
// on failure.
base::WeakPtr<SpdyStream> CreateStreamSynchronously(
    SpdyStreamType type,
    const base::WeakPtr<SpdySession>& session,
    const GURL& url,
    RequestPriority priority,
    const NetLogWithSource& net_log);

// Helper class used by some tests to release a stream as soon as it's
// created.
class StreamReleaserCallback : public TestCompletionCallbackBase {
 public:
  StreamReleaserCallback();

  ~StreamReleaserCallback() override;

  // Returns a callback that releases |request|'s stream.
  CompletionOnceCallback MakeCallback(SpdyStreamRequest* request);

 private:
  void OnComplete(SpdyStreamRequest* request, int result);
};

// An ECSignatureCreator that returns deterministic signatures.
class MockECSignatureCreator : public crypto::ECSignatureCreator {
 public:
  explicit MockECSignatureCreator(crypto::ECPrivateKey* key);

  // crypto::ECSignatureCreator
  bool Sign(const uint8_t* data,
            int data_len,
            std::vector<uint8_t>* signature) override;
  bool DecodeSignature(const std::vector<uint8_t>& signature,
                       std::vector<uint8_t>* out_raw_sig) override;

 private:
  crypto::ECPrivateKey* key_;

  DISALLOW_COPY_AND_ASSIGN(MockECSignatureCreator);
};

// An ECSignatureCreatorFactory creates MockECSignatureCreator.
class MockECSignatureCreatorFactory : public crypto::ECSignatureCreatorFactory {
 public:
  MockECSignatureCreatorFactory();
  ~MockECSignatureCreatorFactory() override;

  // crypto::ECSignatureCreatorFactory
  std::unique_ptr<crypto::ECSignatureCreator> Create(
      crypto::ECPrivateKey* key) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockECSignatureCreatorFactory);
};

// Helper to manage the lifetimes of the dependencies for a
// HttpNetworkTransaction.
struct SpdySessionDependencies {
  // Default set of dependencies -- "null" proxy service.
  SpdySessionDependencies();

  // Custom proxy service dependency.
  explicit SpdySessionDependencies(
      std::unique_ptr<ProxyResolutionService> proxy_resolution_service);

  ~SpdySessionDependencies();

  HostResolver* GetHostResolver() {
    return alternate_host_resolver ? alternate_host_resolver.get()
                                   : host_resolver.get();
  }

  static std::unique_ptr<HttpNetworkSession> SpdyCreateSession(
      SpdySessionDependencies* session_deps);

  // Variant that ignores session_deps->socket_factory, and uses the passed in
  // |factory| instead.
  static std::unique_ptr<HttpNetworkSession> SpdyCreateSessionWithSocketFactory(
      SpdySessionDependencies* session_deps,
      ClientSocketFactory* factory);
  static HttpNetworkSession::Params CreateSessionParams(
      SpdySessionDependencies* session_deps);
  static HttpNetworkSession::Context CreateSessionContext(
      SpdySessionDependencies* session_deps);

  // NOTE: host_resolver must be ordered before http_auth_handler_factory.
  std::unique_ptr<MockHostResolverBase> host_resolver;
  // For using a HostResolver not derived from MockHostResolverBase.
  std::unique_ptr<HostResolver> alternate_host_resolver;
  std::unique_ptr<CertVerifier> cert_verifier;
  std::unique_ptr<TransportSecurityState> transport_security_state;
  std::unique_ptr<CTVerifier> cert_transparency_verifier;
  std::unique_ptr<CTPolicyEnforcer> ct_policy_enforcer;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service;
  std::unique_ptr<HttpUserAgentSettings> http_user_agent_settings;
  std::unique_ptr<SSLConfigService> ssl_config_service;
  std::unique_ptr<MockClientSocketFactory> socket_factory;
  std::unique_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  std::unique_ptr<HttpServerProperties> http_server_properties;
  std::unique_ptr<QuicContext> quic_context;
#if BUILDFLAG(ENABLE_REPORTING)
  std::unique_ptr<ReportingService> reporting_service;
  std::unique_ptr<NetworkErrorLoggingService> network_error_logging_service;
#endif
  bool enable_ip_pooling;
  bool enable_ping;
  bool enable_user_alternate_protocol_ports;
  bool enable_quic;
  bool enable_server_push_cancellation;
  size_t session_max_recv_window_size;
  int session_max_queued_capped_frames;
  spdy::SettingsMap http2_settings;
  SpdySession::TimeFunc time_func;
  bool enable_http2_alternative_service;
  bool enable_websocket_over_http2;
  base::Optional<SpdySessionPool::GreasedHttp2Frame> greased_http2_frame;
  NetLog* net_log;
  bool disable_idle_sockets_close_on_memory_pressure;
  bool enable_early_data;
  bool key_auth_cache_server_entries_by_network_isolation_key;
};

class SpdyURLRequestContext : public URLRequestContext {
 public:
  SpdyURLRequestContext();
  ~SpdyURLRequestContext() override;

  MockClientSocketFactory& socket_factory() { return socket_factory_; }

 private:
  MockClientSocketFactory socket_factory_;
  URLRequestContextStorage storage_;
};

// Equivalent to pool->GetIfExists(spdy_session_key, NetLogWithSource()) !=
// NULL.
bool HasSpdySession(SpdySessionPool* pool, const SpdySessionKey& key);

// Creates a SPDY session for the given key and puts it in the SPDY
// session pool in |http_session|. A SPDY session for |key| must not
// already exist.
base::WeakPtr<SpdySession> CreateSpdySession(HttpNetworkSession* http_session,
                                             const SpdySessionKey& key,
                                             const NetLogWithSource& net_log);

// Like CreateSpdySession(), but the host is considered a trusted proxy and
// allowed to push cross-origin resources.
base::WeakPtr<SpdySession> CreateTrustedSpdySession(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const NetLogWithSource& net_log);

// Like CreateSpdySession(), but does not fail if there is already an IP
// pooled session for |key|.
base::WeakPtr<SpdySession> CreateSpdySessionWithIpBasedPoolingDisabled(
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const NetLogWithSource& net_log);

// Creates an insecure SPDY session for the given key and puts it in
// |pool|. The returned session will neither receive nor send any
// data. A SPDY session for |key| must not already exist.
base::WeakPtr<SpdySession> CreateFakeSpdySession(SpdySessionPool* pool,
                                                 const SpdySessionKey& key);

// Tries to create an insecure SPDY session for the given key but
// expects the attempt to fail with the given error. The session will
// neither receive nor send any data. A SPDY session for |key| must
// not already exist. The session will be created but close in the
// next event loop iteration.
base::WeakPtr<SpdySession> TryCreateFakeSpdySessionExpectingFailure(
    SpdySessionPool* pool,
    const SpdySessionKey& key,
    Error expected_status);

class SpdySessionPoolPeer {
 public:
  explicit SpdySessionPoolPeer(SpdySessionPool* pool);

  void RemoveAliases(const SpdySessionKey& key);
  void SetEnableSendingInitialData(bool enabled);

 private:
  SpdySessionPool* const pool_;

  DISALLOW_COPY_AND_ASSIGN(SpdySessionPoolPeer);
};

class SpdyTestUtil {
 public:
  SpdyTestUtil();
  ~SpdyTestUtil();

  // Add the appropriate headers to put |url| into |block|.
  void AddUrlToHeaderBlock(base::StringPiece url,
                           spdy::SpdyHeaderBlock* headers) const;

  static spdy::SpdyHeaderBlock ConstructGetHeaderBlock(base::StringPiece url);
  static spdy::SpdyHeaderBlock ConstructGetHeaderBlockForProxy(
      base::StringPiece url);
  static spdy::SpdyHeaderBlock ConstructHeadHeaderBlock(base::StringPiece url,
                                                        int64_t content_length);
  static spdy::SpdyHeaderBlock ConstructPostHeaderBlock(base::StringPiece url,
                                                        int64_t content_length);
  static spdy::SpdyHeaderBlock ConstructPutHeaderBlock(base::StringPiece url,
                                                       int64_t content_length);

  // Construct an expected SPDY reply string from the given headers.
  std::string ConstructSpdyReplyString(
      const spdy::SpdyHeaderBlock& headers) const;

  // Construct an expected SPDY SETTINGS frame.
  // |settings| are the settings to set.
  // Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdySettings(
      const spdy::SettingsMap& settings);

  // Constructs an expected SPDY SETTINGS acknowledgement frame.
  spdy::SpdySerializedFrame ConstructSpdySettingsAck();

  // Construct a SPDY PING frame.  Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyPing(uint32_t ping_id, bool is_ack);

  // Construct a SPDY GOAWAY frame with the specified last_good_stream_id.
  // Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyGoAway(
      spdy::SpdyStreamId last_good_stream_id);

  // Construct a SPDY GOAWAY frame with the specified last_good_stream_id,
  // status, and description. Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyGoAway(
      spdy::SpdyStreamId last_good_stream_id,
      spdy::SpdyErrorCode error_code,
      const std::string& desc);

  // Construct a SPDY WINDOW_UPDATE frame.  Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyWindowUpdate(
      spdy::SpdyStreamId stream_id,
      uint32_t delta_window_size);

  // Construct a SPDY RST_STREAM frame.  Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyRstStream(
      spdy::SpdyStreamId stream_id,
      spdy::SpdyErrorCode error_code);

  // Construct a PRIORITY frame. The weight is derived from |request_priority|.
  // Returns the constructed frame.
  spdy::SpdySerializedFrame ConstructSpdyPriority(
      spdy::SpdyStreamId stream_id,
      spdy::SpdyStreamId parent_stream_id,
      RequestPriority request_priority,
      bool exclusive);

  // Constructs a standard SPDY GET HEADERS frame for |url| with header
  // compression.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  spdy::SpdySerializedFrame ConstructSpdyGet(const char* const url,
                                             spdy::SpdyStreamId stream_id,
                                             RequestPriority request_priority);

  // Constructs a standard SPDY GET HEADERS frame with header compression.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.  If |direct| is false, the
  // the full url will be used instead of simply the path.
  spdy::SpdySerializedFrame ConstructSpdyGet(const char* const extra_headers[],
                                             int extra_header_count,
                                             int stream_id,
                                             RequestPriority request_priority);

  // Constructs a SPDY HEADERS frame for a CONNECT request.
  spdy::SpdySerializedFrame ConstructSpdyConnect(
      const char* const extra_headers[],
      int extra_header_count,
      int stream_id,
      RequestPriority priority,
      const HostPortPair& host_port_pair);

  // Constructs a PUSH_PROMISE frame and a HEADERS frame on the pushed stream.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  // Returns a spdy::SpdySerializedFrame object with the two frames
  // concatenated.
  spdy::SpdySerializedFrame ConstructSpdyPush(const char* const extra_headers[],
                                              int extra_header_count,
                                              int stream_id,
                                              int associated_stream_id,
                                              const char* url);
  spdy::SpdySerializedFrame ConstructSpdyPush(const char* const extra_headers[],
                                              int extra_header_count,
                                              int stream_id,
                                              int associated_stream_id,
                                              const char* url,
                                              const char* status,
                                              const char* location);

  // Constructs a PUSH_PROMISE frame.
  spdy::SpdySerializedFrame ConstructSpdyPushPromise(
      spdy::SpdyStreamId associated_stream_id,
      spdy::SpdyStreamId stream_id,
      spdy::SpdyHeaderBlock headers);

  spdy::SpdySerializedFrame ConstructSpdyPushHeaders(
      int stream_id,
      const char* const extra_headers[],
      int extra_header_count);

  // Constructs a HEADERS frame with the request header compression context with
  // END_STREAM flag set to |fin|.
  spdy::SpdySerializedFrame ConstructSpdyResponseHeaders(
      int stream_id,
      spdy::SpdyHeaderBlock headers,
      bool fin);

  // Construct a HEADERS frame carrying exactly the given headers and priority.
  spdy::SpdySerializedFrame ConstructSpdyHeaders(int stream_id,
                                                 spdy::SpdyHeaderBlock headers,
                                                 RequestPriority priority,
                                                 bool fin);

  // Construct a reply HEADERS frame carrying exactly the given headers and the
  // default priority.
  spdy::SpdySerializedFrame ConstructSpdyReply(int stream_id,
                                               spdy::SpdyHeaderBlock headers);

  // Constructs a standard SPDY HEADERS frame to match the SPDY GET.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  spdy::SpdySerializedFrame ConstructSpdyGetReply(
      const char* const extra_headers[],
      int extra_header_count,
      int stream_id);

  // Constructs a standard SPDY HEADERS frame with an Internal Server
  // Error status code.
  spdy::SpdySerializedFrame ConstructSpdyReplyError(int stream_id);

  // Constructs a standard SPDY HEADERS frame with the specified status code.
  spdy::SpdySerializedFrame ConstructSpdyReplyError(
      const char* const status,
      const char* const* const extra_headers,
      int extra_header_count,
      int stream_id);

  // Constructs a standard SPDY POST HEADERS frame.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  spdy::SpdySerializedFrame ConstructSpdyPost(const char* url,
                                              spdy::SpdyStreamId stream_id,
                                              int64_t content_length,
                                              RequestPriority priority,
                                              const char* const extra_headers[],
                                              int extra_header_count);

  // Constructs a chunked transfer SPDY POST HEADERS frame.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  spdy::SpdySerializedFrame ConstructChunkedSpdyPost(
      const char* const extra_headers[],
      int extra_header_count);

  // Constructs a standard SPDY HEADERS frame to match the SPDY POST.
  // |extra_headers| are the extra header-value pairs, which typically
  // will vary the most between calls.
  spdy::SpdySerializedFrame ConstructSpdyPostReply(
      const char* const extra_headers[],
      int extra_header_count);

  // Constructs a single SPDY data frame with the contents "hello!"
  spdy::SpdySerializedFrame ConstructSpdyDataFrame(int stream_id, bool fin);

  // Constructs a single SPDY data frame with the given content.
  spdy::SpdySerializedFrame ConstructSpdyDataFrame(int stream_id,
                                                   base::StringPiece data,
                                                   bool fin);

  // Constructs a single SPDY data frame with the given content and padding.
  spdy::SpdySerializedFrame ConstructSpdyDataFrame(int stream_id,
                                                   base::StringPiece data,
                                                   bool fin,
                                                   int padding_length);

  // Wraps |frame| in the payload of a data frame in stream |stream_id|.
  spdy::SpdySerializedFrame ConstructWrappedSpdyFrame(
      const spdy::SpdySerializedFrame& frame,
      int stream_id);

  // Serialize a spdy::SpdyFrameIR with |headerless_spdy_framer_|.
  spdy::SpdySerializedFrame SerializeFrame(const spdy::SpdyFrameIR& frame_ir);

  // Called when necessary (when it will affect stream dependency specification
  // when setting dependencies based on priorioties) to notify the utility
  // class of stream destruction.
  void UpdateWithStreamDestruction(int stream_id);

  void set_default_url(const GURL& url) { default_url_ = url; }

 private:
  // |content_length| may be NULL, in which case the content-length
  // header will be omitted.
  static spdy::SpdyHeaderBlock ConstructHeaderBlock(base::StringPiece method,
                                                    base::StringPiece url,
                                                    int64_t* content_length);

  // Multiple SpdyFramers are required to keep track of header compression
  // state.
  // Use to serialize frames (request or response) without headers.
  spdy::SpdyFramer headerless_spdy_framer_;
  // Use to serialize request frames with headers.
  spdy::SpdyFramer request_spdy_framer_;
  // Use to serialize response frames with headers.
  spdy::SpdyFramer response_spdy_framer_;

  GURL default_url_;

  // Track a FIFO list of the stream_id of all created requests by priority.
  std::map<int, std::vector<int>> priority_to_stream_id_list_;
};

namespace test {

// Returns a SHA1 HashValue in which each byte has the value |label|.
HashValue GetTestHashValue(uint8_t label);

// A test implementation of ServerPushDelegate that caches all the pushed
// request and provides a interface to cancel the push given url.
class TestServerPushDelegate : public ServerPushDelegate {
 public:
  TestServerPushDelegate();
  ~TestServerPushDelegate() override;

  void OnPush(std::unique_ptr<ServerPushHelper> push_helper,
              const NetLogWithSource& session_net_log) override;

  bool CancelPush(GURL url);

 private:
  std::map<GURL, std::unique_ptr<ServerPushHelper>> push_helpers;
};

}  // namespace test
}  // namespace net

#endif  // NET_SPDY_SPDY_TEST_UTIL_COMMON_H_
