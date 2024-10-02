// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_session.h"

#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/base/tracing.h"
#include "net/base/url_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_util.h"
#include "net/http/http_vary_data.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/quic/quic_http_utils.h"
#include "net/socket/socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/alps_decoder.h"
#include "net/spdy/header_coalescer.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_log_util.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_frame_builder.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kSpdySessionCommandsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("spdy_session_control", R"(
        semantics {
          sender: "Spdy Session"
          description:
            "Sends commands to control an HTTP/2 session."
          trigger:
            "Required control commands like initiating stream, requesting "
            "stream reset, changing priorities, etc."
          data: "No user data."
          destination: OTHER
          destination_other:
            "Any destination the HTTP/2 session is connected to."
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Essential for network access."
        }
    )");

const int kReadBufferSize = 8 * 1024;
const int kDefaultConnectionAtRiskOfLossSeconds = 10;
const int kHungIntervalSeconds = 10;

// Default initial value for HTTP/2 SETTINGS.
const uint32_t kDefaultInitialHeaderTableSize = 4096;
const uint32_t kDefaultInitialEnablePush = 1;
const uint32_t kDefaultInitialInitialWindowSize = 65535;
const uint32_t kDefaultInitialMaxFrameSize = 16384;

// These values are persisted to logs. Entries should not be renumbered, and
// numeric values should never be reused.
enum class SpdyAcceptChEntries {
  kNoEntries = 0,
  kOnlyValidEntries = 1,
  kOnlyInvalidEntries = 2,
  kBothValidAndInvalidEntries = 3,
  kMaxValue = kBothValidAndInvalidEntries,
};

// A SpdyBufferProducer implementation that creates an HTTP/2 frame by adding
// stream ID to greased frame parameters.
class GreasedBufferProducer : public SpdyBufferProducer {
 public:
  GreasedBufferProducer() = delete;
  GreasedBufferProducer(
      base::WeakPtr<SpdyStream> stream,
      const SpdySessionPool::GreasedHttp2Frame* greased_http2_frame,
      BufferedSpdyFramer* buffered_spdy_framer)
      : stream_(stream),
        greased_http2_frame_(greased_http2_frame),
        buffered_spdy_framer_(buffered_spdy_framer) {}

  ~GreasedBufferProducer() override = default;

  std::unique_ptr<SpdyBuffer> ProduceBuffer() override {
    const spdy::SpdyStreamId stream_id = stream_ ? stream_->stream_id() : 0;
    spdy::SpdyUnknownIR frame(stream_id, greased_http2_frame_->type,
                              greased_http2_frame_->flags,
                              greased_http2_frame_->payload);
    auto serialized_frame = std::make_unique<spdy::SpdySerializedFrame>(
        buffered_spdy_framer_->SerializeFrame(frame));
    return std::make_unique<SpdyBuffer>(std::move(serialized_frame));
  }

 private:
  base::WeakPtr<SpdyStream> stream_;
  const raw_ptr<const SpdySessionPool::GreasedHttp2Frame> greased_http2_frame_;
  raw_ptr<BufferedSpdyFramer> buffered_spdy_framer_;
};

bool IsSpdySettingAtDefaultInitialValue(spdy::SpdySettingsId setting_id,
                                        uint32_t value) {
  switch (setting_id) {
    case spdy::SETTINGS_HEADER_TABLE_SIZE:
      return value == kDefaultInitialHeaderTableSize;
    case spdy::SETTINGS_ENABLE_PUSH:
      return value == kDefaultInitialEnablePush;
    case spdy::SETTINGS_MAX_CONCURRENT_STREAMS:
      // There is no initial limit on the number of concurrent streams.
      return false;
    case spdy::SETTINGS_INITIAL_WINDOW_SIZE:
      return value == kDefaultInitialInitialWindowSize;
    case spdy::SETTINGS_MAX_FRAME_SIZE:
      return value == kDefaultInitialMaxFrameSize;
    case spdy::SETTINGS_MAX_HEADER_LIST_SIZE:
      // There is no initial limit on the size of the header list.
      return false;
    case spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL:
      return value == 0;
    default:
      // Undefined parameters have no initial value.
      return false;
  }
}

void LogSpdyAcceptChForOriginHistogram(bool value) {
  base::UmaHistogramBoolean("Net.SpdySession.AcceptChForOrigin", value);
}

base::Value::Dict NetLogSpdyHeadersSentParams(
    const quiche::HttpHeaderBlock* headers,
    bool fin,
    spdy::SpdyStreamId stream_id,
    bool has_priority,
    int weight,
    spdy::SpdyStreamId parent_stream_id,
    bool exclusive,
    NetLogSource source_dependency,
    NetLogCaptureMode capture_mode) {
  auto dict =
      base::Value::Dict()
          .Set("headers", ElideHttpHeaderBlockForNetLog(*headers, capture_mode))
          .Set("fin", fin)
          .Set("stream_id", static_cast<int>(stream_id))
          .Set("has_priority", has_priority);
  if (has_priority) {
    dict.Set("parent_stream_id", static_cast<int>(parent_stream_id));
    dict.Set("weight", weight);
    dict.Set("exclusive", exclusive);
  }
  if (source_dependency.IsValid()) {
    source_dependency.AddToEventParameters(dict);
  }
  return dict;
}

base::Value::Dict NetLogSpdyHeadersReceivedParams(
    const quiche::HttpHeaderBlock* headers,
    bool fin,
    spdy::SpdyStreamId stream_id,
    NetLogCaptureMode capture_mode) {
  return base::Value::Dict()
      .Set("headers", ElideHttpHeaderBlockForNetLog(*headers, capture_mode))
      .Set("fin", fin)
      .Set("stream_id", static_cast<int>(stream_id));
}

base::Value::Dict NetLogSpdySessionCloseParams(int net_error,
                                               const std::string& description) {
  return base::Value::Dict()
      .Set("net_error", net_error)
      .Set("description", description);
}

base::Value::Dict NetLogSpdySessionParams(const HostPortProxyPair& host_pair) {
  return base::Value::Dict()
      .Set("host", host_pair.first.ToString())
      .Set("proxy", host_pair.second.ToDebugString());
}

base::Value::Dict NetLogSpdyInitializedParams(NetLogSource source) {
  base::Value::Dict dict;
  if (source.IsValid()) {
    source.AddToEventParameters(dict);
  }
  dict.Set("protocol", NextProtoToString(kProtoHTTP2));
  return dict;
}

base::Value::Dict NetLogSpdySendSettingsParams(
    const spdy::SettingsMap* settings) {
  base::Value::List settings_list;
  for (const auto& setting : *settings) {
    const spdy::SpdySettingsId id = setting.first;
    const uint32_t value = setting.second;
    settings_list.Append(
        base::StringPrintf("[id:%u (%s) value:%u]", id,
                           spdy::SettingsIdToString(id).c_str(), value));
  }

  return base::Value::Dict().Set("settings", std::move(settings_list));
}

base::Value::Dict NetLogSpdyRecvAcceptChParams(
    spdy::AcceptChOriginValuePair entry) {
  return base::Value::Dict()
      .Set("origin", entry.origin)
      .Set("accept_ch", entry.value);
}

base::Value::Dict NetLogSpdyRecvSettingParams(spdy::SpdySettingsId id,
                                              uint32_t value) {
  return base::Value::Dict()
      .Set("id", base::StringPrintf("%u (%s)", id,
                                    spdy::SettingsIdToString(id).c_str()))
      .Set("value", static_cast<int>(value));
}

base::Value::Dict NetLogSpdyWindowUpdateFrameParams(
    spdy::SpdyStreamId stream_id,
    uint32_t delta) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("delta", static_cast<int>(delta));
}

base::Value::Dict NetLogSpdySessionWindowUpdateParams(int32_t delta,
                                                      int32_t window_size) {
  return base::Value::Dict()
      .Set("delta", delta)
      .Set("window_size", window_size);
}

base::Value::Dict NetLogSpdyDataParams(spdy::SpdyStreamId stream_id,
                                       int size,
                                       bool fin) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("size", size)
      .Set("fin", fin);
}

base::Value::Dict NetLogSpdyRecvRstStreamParams(
    spdy::SpdyStreamId stream_id,
    spdy::SpdyErrorCode error_code) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("error_code", base::StringPrintf("%u (%s)", error_code,
                                            ErrorCodeToString(error_code)));
}

base::Value::Dict NetLogSpdySendRstStreamParams(
    spdy::SpdyStreamId stream_id,
    spdy::SpdyErrorCode error_code,
    const std::string& description) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("error_code", base::StringPrintf("%u (%s)", error_code,
                                            ErrorCodeToString(error_code)))
      .Set("description", description);
}

base::Value::Dict NetLogSpdyPingParams(spdy::SpdyPingId unique_id,
                                       bool is_ack,
                                       const char* type) {
  return base::Value::Dict()
      .Set("unique_id", static_cast<int>(unique_id))
      .Set("type", type)
      .Set("is_ack", is_ack);
}

base::Value::Dict NetLogSpdyRecvGoAwayParams(spdy::SpdyStreamId last_stream_id,
                                             int active_streams,
                                             spdy::SpdyErrorCode error_code,
                                             std::string_view debug_data,
                                             NetLogCaptureMode capture_mode) {
  return base::Value::Dict()
      .Set("last_accepted_stream_id", static_cast<int>(last_stream_id))
      .Set("active_streams", active_streams)
      .Set("error_code", base::StringPrintf("%u (%s)", error_code,
                                            ErrorCodeToString(error_code)))
      .Set("debug_data",
           ElideGoAwayDebugDataForNetLog(capture_mode, debug_data));
}

base::Value::Dict NetLogSpdySessionStalledParams(size_t num_active_streams,
                                                 size_t num_created_streams,
                                                 size_t max_concurrent_streams,
                                                 const std::string& url) {
  return base::Value::Dict()
      .Set("num_active_streams", static_cast<int>(num_active_streams))
      .Set("num_created_streams", static_cast<int>(num_created_streams))
      .Set("max_concurrent_streams", static_cast<int>(max_concurrent_streams))
      .Set("url", url);
}

base::Value::Dict NetLogSpdyPriorityParams(spdy::SpdyStreamId stream_id,
                                           spdy::SpdyStreamId parent_stream_id,
                                           int weight,
                                           bool exclusive) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("parent_stream_id", static_cast<int>(parent_stream_id))
      .Set("weight", weight)
      .Set("exclusive", exclusive);
}

base::Value::Dict NetLogSpdyGreasedFrameParams(spdy::SpdyStreamId stream_id,
                                               uint8_t type,
                                               uint8_t flags,
                                               size_t length,
                                               RequestPriority priority) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("type", type)
      .Set("flags", flags)
      .Set("length", static_cast<int>(length))
      .Set("priority", RequestPriorityToString(priority));
}

// Helper function to return the total size of an array of objects
// with .size() member functions.
template <typename T, size_t N>
size_t GetTotalSize(const T (&arr)[N]) {
  size_t total_size = 0;
  for (size_t i = 0; i < N; ++i) {
    total_size += arr[i].size();
  }
  return total_size;
}

// The maximum number of concurrent streams we will ever create.  Even if
// the server permits more, we will never exceed this limit.
const size_t kMaxConcurrentStreamLimit = 256;

}  // namespace

SpdyProtocolErrorDetails MapFramerErrorToProtocolError(
    http2::Http2DecoderAdapter::SpdyFramerError err) {
  switch (err) {
    case http2::Http2DecoderAdapter::SPDY_NO_ERROR:
      return SPDY_ERROR_NO_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_STREAM_ID:
      return SPDY_ERROR_INVALID_STREAM_ID;
    case http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME:
      return SPDY_ERROR_INVALID_CONTROL_FRAME;
    case http2::Http2DecoderAdapter::SPDY_CONTROL_PAYLOAD_TOO_LARGE:
      return SPDY_ERROR_CONTROL_PAYLOAD_TOO_LARGE;
    case http2::Http2DecoderAdapter::SPDY_DECOMPRESS_FAILURE:
      return SPDY_ERROR_DECOMPRESS_FAILURE;
    case http2::Http2DecoderAdapter::SPDY_INVALID_PADDING:
      return SPDY_ERROR_INVALID_PADDING;
    case http2::Http2DecoderAdapter::SPDY_INVALID_DATA_FRAME_FLAGS:
      return SPDY_ERROR_INVALID_DATA_FRAME_FLAGS;
    case http2::Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME:
      return SPDY_ERROR_UNEXPECTED_FRAME;
    case http2::Http2DecoderAdapter::SPDY_INTERNAL_FRAMER_ERROR:
      return SPDY_ERROR_INTERNAL_FRAMER_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE:
      return SPDY_ERROR_INVALID_CONTROL_FRAME_SIZE;
    case http2::Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD:
      return SPDY_ERROR_OVERSIZED_PAYLOAD;
    case http2::Http2DecoderAdapter::SPDY_HPACK_INDEX_VARINT_ERROR:
      return SPDY_ERROR_HPACK_INDEX_VARINT_ERROR;
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_LENGTH_VARINT_ERROR:
      return SPDY_ERROR_HPACK_NAME_LENGTH_VARINT_ERROR;
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_LENGTH_VARINT_ERROR:
      return SPDY_ERROR_HPACK_VALUE_LENGTH_VARINT_ERROR;
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_TOO_LONG:
      return SPDY_ERROR_HPACK_NAME_TOO_LONG;
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_TOO_LONG:
      return SPDY_ERROR_HPACK_VALUE_TOO_LONG;
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_HUFFMAN_ERROR:
      return SPDY_ERROR_HPACK_NAME_HUFFMAN_ERROR;
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_HUFFMAN_ERROR:
      return SPDY_ERROR_HPACK_VALUE_HUFFMAN_ERROR;
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_MISSING_DYNAMIC_TABLE_SIZE_UPDATE:
      return SPDY_ERROR_HPACK_MISSING_DYNAMIC_TABLE_SIZE_UPDATE;
    case http2::Http2DecoderAdapter::SPDY_HPACK_INVALID_INDEX:
      return SPDY_ERROR_HPACK_INVALID_INDEX;
    case http2::Http2DecoderAdapter::SPDY_HPACK_INVALID_NAME_INDEX:
      return SPDY_ERROR_HPACK_INVALID_NAME_INDEX;
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_NOT_ALLOWED:
      return SPDY_ERROR_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_NOT_ALLOWED;
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_INITIAL_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_LOW_WATER_MARK:
      return SPDY_ERROR_HPACK_INITIAL_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_LOW_WATER_MARK;
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_ACKNOWLEDGED_SETTING:
      return SPDY_ERROR_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_ACKNOWLEDGED_SETTING;
    case http2::Http2DecoderAdapter::SPDY_HPACK_TRUNCATED_BLOCK:
      return SPDY_ERROR_HPACK_TRUNCATED_BLOCK;
    case http2::Http2DecoderAdapter::SPDY_HPACK_FRAGMENT_TOO_LONG:
      return SPDY_ERROR_HPACK_FRAGMENT_TOO_LONG;
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_COMPRESSED_HEADER_SIZE_EXCEEDS_LIMIT:
      return SPDY_ERROR_HPACK_COMPRESSED_HEADER_SIZE_EXCEEDS_LIMIT;
    case http2::Http2DecoderAdapter::SPDY_STOP_PROCESSING:
      return SPDY_ERROR_STOP_PROCESSING;

    case http2::Http2DecoderAdapter::LAST_ERROR:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return static_cast<SpdyProtocolErrorDetails>(-1);
}

Error MapFramerErrorToNetError(
    http2::Http2DecoderAdapter::SpdyFramerError err) {
  switch (err) {
    case http2::Http2DecoderAdapter::SPDY_NO_ERROR:
      return OK;
    case http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_CONTROL_PAYLOAD_TOO_LARGE:
      return ERR_HTTP2_FRAME_SIZE_ERROR;
    case http2::Http2DecoderAdapter::SPDY_DECOMPRESS_FAILURE:
    case http2::Http2DecoderAdapter::SPDY_HPACK_INDEX_VARINT_ERROR:
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_LENGTH_VARINT_ERROR:
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_LENGTH_VARINT_ERROR:
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_TOO_LONG:
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_TOO_LONG:
    case http2::Http2DecoderAdapter::SPDY_HPACK_NAME_HUFFMAN_ERROR:
    case http2::Http2DecoderAdapter::SPDY_HPACK_VALUE_HUFFMAN_ERROR:
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_MISSING_DYNAMIC_TABLE_SIZE_UPDATE:
    case http2::Http2DecoderAdapter::SPDY_HPACK_INVALID_INDEX:
    case http2::Http2DecoderAdapter::SPDY_HPACK_INVALID_NAME_INDEX:
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_NOT_ALLOWED:
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_INITIAL_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_LOW_WATER_MARK:
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_DYNAMIC_TABLE_SIZE_UPDATE_IS_ABOVE_ACKNOWLEDGED_SETTING:
    case http2::Http2DecoderAdapter::SPDY_HPACK_TRUNCATED_BLOCK:
    case http2::Http2DecoderAdapter::SPDY_HPACK_FRAGMENT_TOO_LONG:
    case http2::Http2DecoderAdapter::
        SPDY_HPACK_COMPRESSED_HEADER_SIZE_EXCEEDS_LIMIT:
      return ERR_HTTP2_COMPRESSION_ERROR;
    case http2::Http2DecoderAdapter::SPDY_STOP_PROCESSING:
      return ERR_HTTP2_COMPRESSION_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_PADDING:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_DATA_FRAME_FLAGS:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_UNEXPECTED_FRAME:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INTERNAL_FRAMER_ERROR:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_CONTROL_FRAME_SIZE:
      return ERR_HTTP2_FRAME_SIZE_ERROR;
    case http2::Http2DecoderAdapter::SPDY_INVALID_STREAM_ID:
      return ERR_HTTP2_PROTOCOL_ERROR;
    case http2::Http2DecoderAdapter::SPDY_OVERSIZED_PAYLOAD:
      return ERR_HTTP2_FRAME_SIZE_ERROR;
    case http2::Http2DecoderAdapter::LAST_ERROR:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return ERR_HTTP2_PROTOCOL_ERROR;
}

SpdyProtocolErrorDetails MapRstStreamStatusToProtocolError(
    spdy::SpdyErrorCode error_code) {
  switch (error_code) {
    case spdy::ERROR_CODE_NO_ERROR:
      return STATUS_CODE_NO_ERROR;
    case spdy::ERROR_CODE_PROTOCOL_ERROR:
      return STATUS_CODE_PROTOCOL_ERROR;
    case spdy::ERROR_CODE_INTERNAL_ERROR:
      return STATUS_CODE_INTERNAL_ERROR;
    case spdy::ERROR_CODE_FLOW_CONTROL_ERROR:
      return STATUS_CODE_FLOW_CONTROL_ERROR;
    case spdy::ERROR_CODE_SETTINGS_TIMEOUT:
      return STATUS_CODE_SETTINGS_TIMEOUT;
    case spdy::ERROR_CODE_STREAM_CLOSED:
      return STATUS_CODE_STREAM_CLOSED;
    case spdy::ERROR_CODE_FRAME_SIZE_ERROR:
      return STATUS_CODE_FRAME_SIZE_ERROR;
    case spdy::ERROR_CODE_REFUSED_STREAM:
      return STATUS_CODE_REFUSED_STREAM;
    case spdy::ERROR_CODE_CANCEL:
      return STATUS_CODE_CANCEL;
    case spdy::ERROR_CODE_COMPRESSION_ERROR:
      return STATUS_CODE_COMPRESSION_ERROR;
    case spdy::ERROR_CODE_CONNECT_ERROR:
      return STATUS_CODE_CONNECT_ERROR;
    case spdy::ERROR_CODE_ENHANCE_YOUR_CALM:
      return STATUS_CODE_ENHANCE_YOUR_CALM;
    case spdy::ERROR_CODE_INADEQUATE_SECURITY:
      return STATUS_CODE_INADEQUATE_SECURITY;
    case spdy::ERROR_CODE_HTTP_1_1_REQUIRED:
      return STATUS_CODE_HTTP_1_1_REQUIRED;
  }
  NOTREACHED_IN_MIGRATION();
  return static_cast<SpdyProtocolErrorDetails>(-1);
}

spdy::SpdyErrorCode MapNetErrorToGoAwayStatus(Error err) {
  switch (err) {
    case OK:
      return spdy::ERROR_CODE_NO_ERROR;
    case ERR_HTTP2_PROTOCOL_ERROR:
      return spdy::ERROR_CODE_PROTOCOL_ERROR;
    case ERR_HTTP2_FLOW_CONTROL_ERROR:
      return spdy::ERROR_CODE_FLOW_CONTROL_ERROR;
    case ERR_HTTP2_FRAME_SIZE_ERROR:
      return spdy::ERROR_CODE_FRAME_SIZE_ERROR;
    case ERR_HTTP2_COMPRESSION_ERROR:
      return spdy::ERROR_CODE_COMPRESSION_ERROR;
    case ERR_HTTP2_INADEQUATE_TRANSPORT_SECURITY:
      return spdy::ERROR_CODE_INADEQUATE_SECURITY;
    default:
      return spdy::ERROR_CODE_PROTOCOL_ERROR;
  }
}

SpdyStreamRequest::SpdyStreamRequest() {
  Reset();
}

SpdyStreamRequest::~SpdyStreamRequest() {
  CancelRequest();
}

int SpdyStreamRequest::StartRequest(
    SpdyStreamType type,
    const base::WeakPtr<SpdySession>& session,
    const GURL& url,
    bool can_send_early,
    RequestPriority priority,
    const SocketTag& socket_tag,
    const NetLogWithSource& net_log,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation,
    bool detect_broken_connection,
    base::TimeDelta heartbeat_interval) {
  DCHECK(session);
  DCHECK(!session_);
  DCHECK(!stream_);
  DCHECK(callback_.is_null());
  DCHECK(url.is_valid()) << url.possibly_invalid_spec();

  type_ = type;
  session_ = session;
  url_ = SimplifyUrlForRequest(url);
  priority_ = priority;
  socket_tag_ = socket_tag;
  net_log_ = net_log;
  callback_ = std::move(callback);
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(traffic_annotation);
  detect_broken_connection_ = detect_broken_connection;
  heartbeat_interval_ = heartbeat_interval;

  // If early data is not allowed, confirm the handshake first.
  int rv = OK;
  if (!can_send_early) {
    rv = session_->ConfirmHandshake(
        base::BindOnce(&SpdyStreamRequest::OnConfirmHandshakeComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  if (rv != OK) {
    // If rv is ERR_IO_PENDING, OnConfirmHandshakeComplete() will call
    // TryCreateStream() later.
    return rv;
  }

  base::WeakPtr<SpdyStream> stream;
  rv = session->TryCreateStream(weak_ptr_factory_.GetWeakPtr(), &stream);
  if (rv != OK) {
    // If rv is ERR_IO_PENDING, the SpdySession will call
    // OnRequestCompleteSuccess() or OnRequestCompleteFailure() later.
    return rv;
  }

  Reset();
  stream_ = stream;
  return OK;
}

void SpdyStreamRequest::CancelRequest() {
  if (session_)
    session_->CancelStreamRequest(weak_ptr_factory_.GetWeakPtr());
  Reset();
  // Do this to cancel any pending CompleteStreamRequest() and
  // OnConfirmHandshakeComplete() tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

base::WeakPtr<SpdyStream> SpdyStreamRequest::ReleaseStream() {
  DCHECK(!session_);
  base::WeakPtr<SpdyStream> stream = stream_;
  DCHECK(stream);
  Reset();
  return stream;
}

void SpdyStreamRequest::SetPriority(RequestPriority priority) {
  if (priority_ == priority)
    return;

  if (stream_)
    stream_->SetPriority(priority);
  if (session_)
    session_->ChangeStreamRequestPriority(weak_ptr_factory_.GetWeakPtr(),
                                          priority);
  priority_ = priority;
}

void SpdyStreamRequest::OnRequestCompleteSuccess(
    const base::WeakPtr<SpdyStream>& stream) {
  DCHECK(session_);
  DCHECK(!stream_);
  DCHECK(!callback_.is_null());
  CompletionOnceCallback callback = std::move(callback_);
  Reset();
  DCHECK(stream);
  stream_ = stream;
  std::move(callback).Run(OK);
}

void SpdyStreamRequest::OnRequestCompleteFailure(int rv) {
  DCHECK(session_);
  DCHECK(!stream_);
  DCHECK(!callback_.is_null());
  CompletionOnceCallback callback = std::move(callback_);
  Reset();
  DCHECK_NE(rv, OK);
  std::move(callback).Run(rv);
}

void SpdyStreamRequest::Reset() {
  type_ = SPDY_BIDIRECTIONAL_STREAM;
  session_.reset();
  stream_.reset();
  url_ = GURL();
  priority_ = MINIMUM_PRIORITY;
  socket_tag_ = SocketTag();
  net_log_ = NetLogWithSource();
  callback_.Reset();
  traffic_annotation_.reset();
}

void SpdyStreamRequest::OnConfirmHandshakeComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (!session_)
    return;

  if (rv != OK) {
    OnRequestCompleteFailure(rv);
    return;
  }

  // ConfirmHandshake() completed asynchronously. Record the time so the caller
  // can adjust LoadTimingInfo.
  confirm_handshake_end_ = base::TimeTicks::Now();

  if (!session_) {
    OnRequestCompleteFailure(ERR_CONNECTION_CLOSED);
    return;
  }

  base::WeakPtr<SpdyStream> stream;
  rv = session_->TryCreateStream(weak_ptr_factory_.GetWeakPtr(), &stream);
  if (rv == OK) {
    OnRequestCompleteSuccess(stream);
  } else if (rv != ERR_IO_PENDING) {
    // If rv is ERR_IO_PENDING, the SpdySession will call
    // OnRequestCompleteSuccess() or OnRequestCompleteFailure() later.
    OnRequestCompleteFailure(rv);
  }
}

// static
bool SpdySession::CanPool(TransportSecurityState* transport_security_state,
                          const SSLInfo& ssl_info,
                          const SSLConfigService& ssl_config_service,
                          std::string_view old_hostname,
                          std::string_view new_hostname) {
  // Pooling is prohibited if the server cert is not valid for the new domain,
  // and for connections on which client certs were sent. It is also prohibited
  // when channel ID was sent if the hosts are from different eTLDs+1.
  if (IsCertStatusError(ssl_info.cert_status))
    return false;

  if (ssl_info.client_cert_sent &&
      !(ssl_config_service.CanShareConnectionWithClientCerts(old_hostname) &&
        ssl_config_service.CanShareConnectionWithClientCerts(new_hostname))) {
    return false;
  }

  if (!ssl_info.cert->VerifyNameMatch(new_hostname))
    return false;

  // Port is left at 0 as it is never used.
  if (transport_security_state->CheckPublicKeyPins(
          HostPortPair(new_hostname, 0), ssl_info.is_issued_by_known_root,
          ssl_info.public_key_hashes) ==
      TransportSecurityState::PKPStatus::VIOLATED) {
    return false;
  }

  switch (transport_security_state->CheckCTRequirements(
      HostPortPair(new_hostname, 0), ssl_info.is_issued_by_known_root,
      ssl_info.public_key_hashes, ssl_info.cert.get(),
      ssl_info.ct_policy_compliance)) {
    case TransportSecurityState::CT_REQUIREMENTS_NOT_MET:
      return false;
    case TransportSecurityState::CT_REQUIREMENTS_MET:
    case TransportSecurityState::CT_NOT_REQUIRED:
      // Intentional fallthrough; this case is just here to make sure that all
      // possible values of CheckCTRequirements() are handled.
      break;
  }

  return true;
}

SpdySession::SpdySession(
    const SpdySessionKey& spdy_session_key,
    HttpServerProperties* http_server_properties,
    TransportSecurityState* transport_security_state,
    SSLConfigService* ssl_config_service,
    const quic::ParsedQuicVersionVector& quic_supported_versions,
    bool enable_sending_initial_data,
    bool enable_ping_based_connection_checking,
    bool is_http2_enabled,
    bool is_quic_enabled,
    size_t session_max_recv_window_size,
    int session_max_queued_capped_frames,
    const spdy::SettingsMap& initial_settings,
    bool enable_http2_settings_grease,
    const std::optional<SpdySessionPool::GreasedHttp2Frame>&
        greased_http2_frame,
    bool http2_end_stream_with_data_frame,
    bool enable_priority_update,
    TimeFunc time_func,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log)
    : spdy_session_key_(spdy_session_key),
      http_server_properties_(http_server_properties),
      transport_security_state_(transport_security_state),
      ssl_config_service_(ssl_config_service),
      stream_hi_water_mark_(kFirstStreamId),
      initial_settings_(initial_settings),
      enable_http2_settings_grease_(enable_http2_settings_grease),
      greased_http2_frame_(greased_http2_frame),
      http2_end_stream_with_data_frame_(http2_end_stream_with_data_frame),
      enable_priority_update_(enable_priority_update),
      max_concurrent_streams_(kInitialMaxConcurrentStreams),
      last_read_time_(time_func()),
      session_max_recv_window_size_(session_max_recv_window_size),
      session_max_queued_capped_frames_(session_max_queued_capped_frames),
      last_recv_window_update_(base::TimeTicks::Now()),
      time_to_buffer_small_window_updates_(
          kDefaultTimeToBufferSmallWindowUpdates),
      stream_initial_send_window_size_(kDefaultInitialWindowSize),
      max_header_table_size_(
          initial_settings.at(spdy::SETTINGS_HEADER_TABLE_SIZE)),
      stream_max_recv_window_size_(
          initial_settings.at(spdy::SETTINGS_INITIAL_WINDOW_SIZE)),
      net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::HTTP2_SESSION)),
      quic_supported_versions_(quic_supported_versions),
      enable_sending_initial_data_(enable_sending_initial_data),
      enable_ping_based_connection_checking_(
          enable_ping_based_connection_checking),
      is_http2_enabled_(is_http2_enabled),
      is_quic_enabled_(is_quic_enabled),
      connection_at_risk_of_loss_time_(
          base::Seconds(kDefaultConnectionAtRiskOfLossSeconds)),
      hung_interval_(base::Seconds(kHungIntervalSeconds)),
      time_func_(time_func),
      network_quality_estimator_(network_quality_estimator) {
  net_log_.BeginEvent(NetLogEventType::HTTP2_SESSION, [&] {
    return NetLogSpdySessionParams(host_port_proxy_pair());
  });

  DCHECK(base::Contains(initial_settings_, spdy::SETTINGS_HEADER_TABLE_SIZE));
  DCHECK(base::Contains(initial_settings_, spdy::SETTINGS_INITIAL_WINDOW_SIZE));

  if (greased_http2_frame_) {
    // See https://tools.ietf.org/html/draft-bishop-httpbis-grease-00
    // for reserved frame types.
    DCHECK_EQ(0x0b, greased_http2_frame_.value().type % 0x1f);
  }

  // TODO(mbelshe): consider randomization of the stream_hi_water_mark.
}

SpdySession::~SpdySession() {
  CHECK(!in_io_loop_);
  DcheckDraining();

  DCHECK(waiting_for_confirmation_callbacks_.empty());

  DCHECK_EQ(broken_connection_detection_requests_, 0);

  // TODO(akalin): Check connection->is_initialized().
  DCHECK(socket_);
  // With SPDY we can't recycle sockets.
  socket_->Disconnect();

  RecordHistograms();

  net_log_.EndEvent(NetLogEventType::HTTP2_SESSION);
}

void SpdySession::InitializeWithSocketHandle(
    std::unique_ptr<StreamSocketHandle> stream_socket_handle,
    SpdySessionPool* pool) {
  DCHECK(!stream_socket_handle_);
  DCHECK(!owned_stream_socket_);
  DCHECK(!socket_);

  // TODO(akalin): Check connection->is_initialized() instead. This
  // requires re-working CreateFakeSpdySession(), though.
  DCHECK(stream_socket_handle->socket());

  stream_socket_handle_ = std::move(stream_socket_handle);
  socket_ = stream_socket_handle_->socket();
  stream_socket_handle_->AddHigherLayeredPool(this);

  InitializeInternal(pool);
}

void SpdySession::InitializeWithSocket(
    std::unique_ptr<StreamSocket> stream_socket,
    const LoadTimingInfo::ConnectTiming& connect_timing,
    SpdySessionPool* pool) {
  DCHECK(!stream_socket_handle_);
  DCHECK(!owned_stream_socket_);
  DCHECK(!socket_);

  DCHECK(stream_socket);

  owned_stream_socket_ = std::move(stream_socket);
  socket_ = owned_stream_socket_.get();
  connect_timing_ =
      std::make_unique<LoadTimingInfo::ConnectTiming>(connect_timing);

  InitializeInternal(pool);
}

int SpdySession::ParseAlps() {
  auto alps_data = socket_->GetPeerApplicationSettings();
  if (!alps_data) {
    return OK;
  }

  AlpsDecoder alps_decoder;
  AlpsDecoder::Error error = alps_decoder.Decode(alps_data.value());
  base::UmaHistogramEnumeration("Net.SpdySession.AlpsDecoderStatus", error);
  if (error != AlpsDecoder::Error::kNoError) {
    DoDrainSession(
        ERR_HTTP2_PROTOCOL_ERROR,
        base::StrCat({"Error parsing ALPS: ",
                      base::NumberToString(static_cast<int>(error))}));
    return ERR_HTTP2_PROTOCOL_ERROR;
  }

  base::UmaHistogramCounts100("Net.SpdySession.AlpsSettingParameterCount",
                              alps_decoder.GetSettings().size());
  for (const auto& setting : alps_decoder.GetSettings()) {
    spdy::SpdySettingsId identifier = setting.first;
    uint32_t value = setting.second;
    net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_SETTING, [&] {
      return NetLogSpdyRecvSettingParams(identifier, value);
    });
    HandleSetting(identifier, value);
  }

  bool has_valid_entry = false;
  bool has_invalid_entry = false;
  for (const auto& entry : alps_decoder.GetAcceptCh()) {
    const url::SchemeHostPort scheme_host_port(GURL(entry.origin));
    // |entry.origin| must be a valid SchemeHostPort.
    std::string serialized = scheme_host_port.Serialize();
    if (serialized.empty() || entry.origin != serialized) {
      has_invalid_entry = true;
      continue;
    }
    has_valid_entry = true;
    accept_ch_entries_received_via_alps_.emplace(std::move(scheme_host_port),
                                                 entry.value);

    net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_ACCEPT_CH,
                      [&] { return NetLogSpdyRecvAcceptChParams(entry); });
  }

  SpdyAcceptChEntries value;
  if (has_valid_entry) {
    if (has_invalid_entry) {
      value = SpdyAcceptChEntries::kBothValidAndInvalidEntries;
    } else {
      value = SpdyAcceptChEntries::kOnlyValidEntries;
    }
  } else {
    if (has_invalid_entry) {
      value = SpdyAcceptChEntries::kOnlyInvalidEntries;
    } else {
      value = SpdyAcceptChEntries::kNoEntries;
    }
  }
  base::UmaHistogramEnumeration("Net.SpdySession.AlpsAcceptChEntries", value);

  return OK;
}

bool SpdySession::VerifyDomainAuthentication(std::string_view domain) const {
  if (availability_state_ == STATE_DRAINING)
    return false;

  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info))
    return true;  // This is not a secure session, so all domains are okay.

  return CanPool(transport_security_state_, ssl_info, *ssl_config_service_,
                 host_port_pair().host(), domain);
}

void SpdySession::EnqueueStreamWrite(
    const base::WeakPtr<SpdyStream>& stream,
    spdy::SpdyFrameType frame_type,
    std::unique_ptr<SpdyBufferProducer> producer) {
  DCHECK(frame_type == spdy::SpdyFrameType::HEADERS ||
         frame_type == spdy::SpdyFrameType::DATA);
  EnqueueWrite(stream->priority(), frame_type, std::move(producer), stream,
               stream->traffic_annotation());
}

bool SpdySession::GreasedFramesEnabled() const {
  return greased_http2_frame_.has_value();
}

void SpdySession::EnqueueGreasedFrame(const base::WeakPtr<SpdyStream>& stream) {
  if (availability_state_ == STATE_DRAINING)
    return;

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_GREASED_FRAME, [&] {
    return NetLogSpdyGreasedFrameParams(
        stream->stream_id(), greased_http2_frame_.value().type,
        greased_http2_frame_.value().flags,
        greased_http2_frame_.value().payload.length(), stream->priority());
  });

  EnqueueWrite(
      stream->priority(),
      static_cast<spdy::SpdyFrameType>(greased_http2_frame_.value().type),
      std::make_unique<GreasedBufferProducer>(
          stream, &greased_http2_frame_.value(), buffered_spdy_framer_.get()),
      stream, stream->traffic_annotation());
}

bool SpdySession::ShouldSendHttp2Priority() const {
  return !enable_priority_update_ || !deprecate_http2_priorities_;
}

bool SpdySession::ShouldSendPriorityUpdate() const {
  if (!enable_priority_update_) {
    return false;
  }

  return settings_frame_received_ ? deprecate_http2_priorities_ : true;
}

int SpdySession::ConfirmHandshake(CompletionOnceCallback callback) {
  if (availability_state_ == STATE_GOING_AWAY)
    return ERR_FAILED;

  if (availability_state_ == STATE_DRAINING)
    return ERR_CONNECTION_CLOSED;

  int rv = ERR_IO_PENDING;
  if (!in_confirm_handshake_) {
    rv = socket_->ConfirmHandshake(
        base::BindOnce(&SpdySession::NotifyRequestsOfConfirmation,
                       weak_factory_.GetWeakPtr()));
  }
  if (rv == ERR_IO_PENDING) {
    in_confirm_handshake_ = true;
    waiting_for_confirmation_callbacks_.push_back(std::move(callback));
  }
  return rv;
}

std::unique_ptr<spdy::SpdySerializedFrame> SpdySession::CreateHeaders(
    spdy::SpdyStreamId stream_id,
    RequestPriority priority,
    spdy::SpdyControlFlags flags,
    quiche::HttpHeaderBlock block,
    NetLogSource source_dependency) {
  ActiveStreamMap::const_iterator it = active_streams_.find(stream_id);
  CHECK(it != active_streams_.end());
  CHECK_EQ(it->second->stream_id(), stream_id);

  MaybeSendPrefacePing();

  DCHECK(buffered_spdy_framer_.get());
  spdy::SpdyPriority spdy_priority =
      ConvertRequestPriorityToSpdyPriority(priority);

  bool has_priority = true;
  int weight = 0;
  spdy::SpdyStreamId parent_stream_id = 0;
  bool exclusive = false;

  priority_dependency_state_.OnStreamCreation(
      stream_id, spdy_priority, &parent_stream_id, &weight, &exclusive);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_HEADERS,
                    [&](NetLogCaptureMode capture_mode) {
                      return NetLogSpdyHeadersSentParams(
                          &block, (flags & spdy::CONTROL_FLAG_FIN) != 0,
                          stream_id, has_priority, weight, parent_stream_id,
                          exclusive, source_dependency, capture_mode);
                    });

  spdy::SpdyHeadersIR headers(stream_id, std::move(block));
  headers.set_has_priority(has_priority);
  headers.set_weight(weight);
  headers.set_parent_stream_id(parent_stream_id);
  headers.set_exclusive(exclusive);
  headers.set_fin((flags & spdy::CONTROL_FLAG_FIN) != 0);

  streams_initiated_count_++;

  return std::make_unique<spdy::SpdySerializedFrame>(
      buffered_spdy_framer_->SerializeFrame(headers));
}

std::unique_ptr<SpdyBuffer> SpdySession::CreateDataBuffer(
    spdy::SpdyStreamId stream_id,
    IOBuffer* data,
    int len,
    spdy::SpdyDataFlags flags,
    int* effective_len,
    bool* end_stream) {
  if (availability_state_ == STATE_DRAINING) {
    return nullptr;
  }

  ActiveStreamMap::const_iterator it = active_streams_.find(stream_id);
  CHECK(it != active_streams_.end());
  SpdyStream* stream = it->second;
  CHECK_EQ(stream->stream_id(), stream_id);

  if (len < 0) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  *effective_len = std::min(len, kMaxSpdyFrameChunkSize);

  bool send_stalled_by_stream = (stream->send_window_size() <= 0);
  bool send_stalled_by_session = IsSendStalled();

  // NOTE: There's an enum of the same name in histograms.xml.
  enum SpdyFrameFlowControlState {
    SEND_NOT_STALLED,
    SEND_STALLED_BY_STREAM,
    SEND_STALLED_BY_SESSION,
    SEND_STALLED_BY_STREAM_AND_SESSION,
  };

  SpdyFrameFlowControlState frame_flow_control_state = SEND_NOT_STALLED;
  if (send_stalled_by_stream) {
    if (send_stalled_by_session) {
      frame_flow_control_state = SEND_STALLED_BY_STREAM_AND_SESSION;
    } else {
      frame_flow_control_state = SEND_STALLED_BY_STREAM;
    }
  } else if (send_stalled_by_session) {
    frame_flow_control_state = SEND_STALLED_BY_SESSION;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.SpdyFrameStreamAndSessionFlowControlState",
                            frame_flow_control_state,
                            SEND_STALLED_BY_STREAM_AND_SESSION + 1);

  // Obey send window size of the stream.
  if (send_stalled_by_stream) {
    stream->set_send_stalled_by_flow_control(true);
    // Even though we're currently stalled only by the stream, we
    // might end up being stalled by the session also.
    QueueSendStalledStream(*stream);
    net_log_.AddEventWithIntParams(
        NetLogEventType::HTTP2_SESSION_STREAM_STALLED_BY_STREAM_SEND_WINDOW,
        "stream_id", stream_id);
    return nullptr;
  }

  *effective_len = std::min(*effective_len, stream->send_window_size());

  // Obey send window size of the session.
  if (send_stalled_by_session) {
    stream->set_send_stalled_by_flow_control(true);
    QueueSendStalledStream(*stream);
    net_log_.AddEventWithIntParams(
        NetLogEventType::HTTP2_SESSION_STREAM_STALLED_BY_SESSION_SEND_WINDOW,
        "stream_id", stream_id);
    return nullptr;
  }

  *effective_len = std::min(*effective_len, session_send_window_size_);

  DCHECK_GE(*effective_len, 0);

  // Clear FIN flag if only some of the data will be in the data
  // frame.
  if (*effective_len < len)
    flags = static_cast<spdy::SpdyDataFlags>(flags & ~spdy::DATA_FLAG_FIN);


  // Send PrefacePing for DATA_FRAMEs with nonzero payload size.
  if (*effective_len > 0)
    MaybeSendPrefacePing();

  // TODO(mbelshe): reduce memory copies here.
  DCHECK(buffered_spdy_framer_.get());
  std::unique_ptr<spdy::SpdySerializedFrame> frame(
      buffered_spdy_framer_->CreateDataFrame(
          stream_id, data->data(), static_cast<uint32_t>(*effective_len),
          flags));

  auto data_buffer = std::make_unique<SpdyBuffer>(std::move(frame));

  // Send window size is based on payload size, so nothing to do if this is
  // just a FIN with no payload.
  if (*effective_len != 0) {
    DecreaseSendWindowSize(static_cast<int32_t>(*effective_len));
    data_buffer->AddConsumeCallback(base::BindRepeating(
        &SpdySession::OnWriteBufferConsumed, weak_factory_.GetWeakPtr(),
        static_cast<size_t>(*effective_len)));
  }

  *end_stream = (flags & spdy::DATA_FLAG_FIN) == spdy::DATA_FLAG_FIN;
  return data_buffer;
}

void SpdySession::UpdateStreamPriority(SpdyStream* stream,
                                       RequestPriority old_priority,
                                       RequestPriority new_priority) {
  // There might be write frames enqueued for |stream| regardless of whether it
  // is active (stream_id != 0) or inactive (no HEADERS frame has been sent out
  // yet and stream_id == 0).
  write_queue_.ChangePriorityOfWritesForStream(stream, old_priority,
                                               new_priority);

  // PRIORITY frames only need to be sent if |stream| is active.
  const spdy::SpdyStreamId stream_id = stream->stream_id();
  if (stream_id == 0)
    return;

  DCHECK(IsStreamActive(stream_id));

  if (base::FeatureList::IsEnabled(features::kAvoidH2Reprioritization))
    return;

  auto updates = priority_dependency_state_.OnStreamUpdate(
      stream_id, ConvertRequestPriorityToSpdyPriority(new_priority));
  for (auto u : updates) {
    DCHECK(IsStreamActive(u.id));
    EnqueuePriorityFrame(u.id, u.parent_stream_id, u.weight, u.exclusive);
  }
}

void SpdySession::CloseActiveStream(spdy::SpdyStreamId stream_id, int status) {
  DCHECK_NE(stream_id, 0u);

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  CloseActiveStreamIterator(it, status);
}

void SpdySession::CloseCreatedStream(const base::WeakPtr<SpdyStream>& stream,
                                     int status) {
  DCHECK_EQ(stream->stream_id(), 0u);

  auto it = created_streams_.find(stream.get());
  if (it == created_streams_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  CloseCreatedStreamIterator(it, status);
}

void SpdySession::ResetStream(spdy::SpdyStreamId stream_id,
                              int error,
                              const std::string& description) {
  DCHECK_NE(stream_id, 0u);

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  ResetStreamIterator(it, error, description);
}

bool SpdySession::IsStreamActive(spdy::SpdyStreamId stream_id) const {
  return base::Contains(active_streams_, stream_id);
}

LoadState SpdySession::GetLoadState() const {
  // Just report that we're idle since the session could be doing
  // many things concurrently.
  return LOAD_STATE_IDLE;
}

int SpdySession::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return GetPeerAddress(endpoint);
}

bool SpdySession::GetSSLInfo(SSLInfo* ssl_info) const {
  return socket_->GetSSLInfo(ssl_info);
}

std::string_view SpdySession::GetAcceptChViaAlps(
    const url::SchemeHostPort& scheme_host_port) const {
  auto it = accept_ch_entries_received_via_alps_.find(scheme_host_port);
  if (it == accept_ch_entries_received_via_alps_.end()) {
    LogSpdyAcceptChForOriginHistogram(false);
    return {};
  }

  LogSpdyAcceptChForOriginHistogram(true);
  return it->second;
}

NextProto SpdySession::GetNegotiatedProtocol() const {
  return socket_->GetNegotiatedProtocol();
}

void SpdySession::SendStreamWindowUpdate(spdy::SpdyStreamId stream_id,
                                         uint32_t delta_window_size) {
  ActiveStreamMap::const_iterator it = active_streams_.find(stream_id);
  CHECK(it != active_streams_.end());
  CHECK_EQ(it->second->stream_id(), stream_id);
  SendWindowUpdateFrame(stream_id, delta_window_size, it->second->priority());
}

void SpdySession::CloseSessionOnError(Error err,
                                      const std::string& description) {
  DCHECK_LT(err, ERR_IO_PENDING);
  DoDrainSession(err, description);
}

void SpdySession::MakeUnavailable() {
  if (availability_state_ == STATE_AVAILABLE) {
    availability_state_ = STATE_GOING_AWAY;
    pool_->MakeSessionUnavailable(GetWeakPtr());
  }
}

void SpdySession::StartGoingAway(spdy::SpdyStreamId last_good_stream_id,
                                 Error status) {
  DCHECK_GE(availability_state_, STATE_GOING_AWAY);
  DCHECK_NE(OK, status);
  DCHECK_NE(ERR_IO_PENDING, status);

  // The loops below are carefully written to avoid reentrancy problems.

  NotifyRequestsOfConfirmation(status);

  while (true) {
    size_t old_size = GetTotalSize(pending_create_stream_queues_);
    base::WeakPtr<SpdyStreamRequest> pending_request =
        GetNextPendingStreamRequest();
    if (!pending_request)
      break;
    // No new stream requests should be added while the session is
    // going away.
    DCHECK_GT(old_size, GetTotalSize(pending_create_stream_queues_));
    pending_request->OnRequestCompleteFailure(status);
  }

  while (true) {
    size_t old_size = active_streams_.size();
    auto it = active_streams_.lower_bound(last_good_stream_id + 1);
    if (it == active_streams_.end())
      break;
    LogAbandonedActiveStream(it, status);
    CloseActiveStreamIterator(it, status);
    // No new streams should be activated while the session is going
    // away.
    DCHECK_GT(old_size, active_streams_.size());
  }

  while (!created_streams_.empty()) {
    size_t old_size = created_streams_.size();
    auto it = created_streams_.begin();
    LogAbandonedStream(*it, status);
    CloseCreatedStreamIterator(it, status);
    // No new streams should be created while the session is going
    // away.
    DCHECK_GT(old_size, created_streams_.size());
  }

  write_queue_.RemovePendingWritesForStreamsAfter(last_good_stream_id);

  DcheckGoingAway();
  MaybeFinishGoingAway();
}

void SpdySession::MaybeFinishGoingAway() {
  if (active_streams_.empty() && created_streams_.empty() &&
      availability_state_ == STATE_GOING_AWAY) {
    DoDrainSession(OK, "Finished going away");
  }
}

base::Value::Dict SpdySession::GetInfoAsValue() const {
  DCHECK(buffered_spdy_framer_.get());

  auto dict =
      base::Value::Dict()
          .Set("source_id", static_cast<int>(net_log_.source().id))
          .Set("host_port_pair", host_port_pair().ToString())
          .Set("proxy", host_port_proxy_pair().second.ToDebugString())
          .Set("network_anonymization_key",
               spdy_session_key_.network_anonymization_key().ToDebugString())
          .Set("active_streams", static_cast<int>(active_streams_.size()))
          .Set("negotiated_protocol",
               NextProtoToString(socket_->GetNegotiatedProtocol()))
          .Set("error", error_on_close_)
          .Set("max_concurrent_streams",
               static_cast<int>(max_concurrent_streams_))
          .Set("streams_initiated_count", streams_initiated_count_)
          .Set("streams_abandoned_count", streams_abandoned_count_)
          .Set("frames_received", buffered_spdy_framer_->frames_received())
          .Set("send_window_size", session_send_window_size_)
          .Set("recv_window_size", session_recv_window_size_)
          .Set("unacked_recv_window_bytes", session_unacked_recv_window_bytes_);

  if (!pooled_aliases_.empty()) {
    base::Value::List alias_list;
    for (const auto& alias : pooled_aliases_) {
      alias_list.Append(alias.host_port_pair().ToString());
    }
    dict.Set("aliases", std::move(alias_list));
  }
  return dict;
}

bool SpdySession::IsReused() const {
  if (buffered_spdy_framer_->frames_received() > 0)
    return true;

  // If there's no socket pool in use (i.e., |owned_stream_socket_| is
  // non-null), then the SpdySession could only have been created with freshly
  // connected socket, since canceling the H2 session request would have
  // destroyed the socket.
  return owned_stream_socket_ ||
         stream_socket_handle_->reuse_type() ==
             StreamSocketHandle::SocketReuseType::kUnusedIdle;
}

bool SpdySession::GetLoadTimingInfo(spdy::SpdyStreamId stream_id,
                                    LoadTimingInfo* load_timing_info) const {
  if (stream_socket_handle_) {
    DCHECK(!connect_timing_);
    return stream_socket_handle_->GetLoadTimingInfo(stream_id != kFirstStreamId,
                                                    load_timing_info);
  }

  DCHECK(connect_timing_);
  DCHECK(socket_);

  // The socket is considered "fresh" (not reused) only for the first stream on
  // a SPDY session. All others consider it reused, and don't return connection
  // establishment timing information.
  load_timing_info->socket_reused = (stream_id != kFirstStreamId);
  if (!load_timing_info->socket_reused)
    load_timing_info->connect_timing = *connect_timing_;

  load_timing_info->socket_log_id = socket_->NetLog().source().id;

  return true;
}

int SpdySession::GetPeerAddress(IPEndPoint* address) const {
  if (socket_)
    return socket_->GetPeerAddress(address);

  return ERR_SOCKET_NOT_CONNECTED;
}

int SpdySession::GetLocalAddress(IPEndPoint* address) const {
  if (socket_)
    return socket_->GetLocalAddress(address);

  return ERR_SOCKET_NOT_CONNECTED;
}

void SpdySession::AddPooledAlias(const SpdySessionKey& alias_key) {
  pooled_aliases_.insert(alias_key);
}

void SpdySession::RemovePooledAlias(const SpdySessionKey& alias_key) {
  pooled_aliases_.erase(alias_key);
}

bool SpdySession::HasAcceptableTransportSecurity() const {
  SSLInfo ssl_info;
  CHECK(GetSSLInfo(&ssl_info));

  // HTTP/2 requires TLS 1.2+
  if (SSLConnectionStatusToVersion(ssl_info.connection_status) <
      SSL_CONNECTION_VERSION_TLS1_2) {
    return false;
  }

  if (!IsTLSCipherSuiteAllowedByHTTP2(
          SSLConnectionStatusToCipherSuite(ssl_info.connection_status))) {
    return false;
  }

  return true;
}

base::WeakPtr<SpdySession> SpdySession::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool SpdySession::CloseOneIdleConnection() {
  CHECK(!in_io_loop_);
  DCHECK(pool_);
  if (active_streams_.empty()) {
    DoDrainSession(ERR_CONNECTION_CLOSED, "Closing idle connection.");
  }
  // Return false as the socket wasn't immediately closed.
  return false;
}

bool SpdySession::ChangeSocketTag(const SocketTag& new_tag) {
  if (!IsAvailable() || !socket_)
    return false;

  // Changing the tag on the underlying socket will affect all streams,
  // so only allow changing the tag when there are no active streams.
  if (is_active())
    return false;

  socket_->ApplySocketTag(new_tag);

  SpdySessionKey new_key(
      spdy_session_key_.host_port_pair(), spdy_session_key_.privacy_mode(),
      spdy_session_key_.proxy_chain(), spdy_session_key_.session_usage(),
      new_tag, spdy_session_key_.network_anonymization_key(),
      spdy_session_key_.secure_dns_policy(),
      spdy_session_key_.disable_cert_verification_network_fetches());
  spdy_session_key_ = new_key;

  return true;
}

void SpdySession::EnableBrokenConnectionDetection(
    base::TimeDelta heartbeat_interval) {
  DCHECK_GE(broken_connection_detection_requests_, 0);
  if (broken_connection_detection_requests_++ > 0)
    return;

  DCHECK(!IsBrokenConnectionDetectionEnabled());
  NetworkChangeNotifier::AddDefaultNetworkActiveObserver(this);
  heartbeat_interval_ = heartbeat_interval;
  heartbeat_timer_.Start(
      FROM_HERE, heartbeat_interval_,
      base::BindOnce(&SpdySession::MaybeCheckConnectionStatus,
                     weak_factory_.GetWeakPtr()));
}

bool SpdySession::IsBrokenConnectionDetectionEnabled() const {
  return heartbeat_timer_.IsRunning();
}

void SpdySession::InitializeInternal(SpdySessionPool* pool) {
  CHECK(!in_io_loop_);
  DCHECK_EQ(availability_state_, STATE_AVAILABLE);
  DCHECK_EQ(read_state_, READ_STATE_DO_READ);
  DCHECK_EQ(write_state_, WRITE_STATE_IDLE);

  session_send_window_size_ = kDefaultInitialWindowSize;
  session_recv_window_size_ = kDefaultInitialWindowSize;

  buffered_spdy_framer_ = std::make_unique<BufferedSpdyFramer>(
      initial_settings_.find(spdy::SETTINGS_MAX_HEADER_LIST_SIZE)->second,
      net_log_, time_func_);
  buffered_spdy_framer_->set_visitor(this);
  buffered_spdy_framer_->set_debug_visitor(this);
  buffered_spdy_framer_->UpdateHeaderDecoderTableSize(max_header_table_size_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_INITIALIZED, [&] {
    return NetLogSpdyInitializedParams(socket_->NetLog().source());
  });

  DCHECK_EQ(availability_state_, STATE_AVAILABLE);
  if (enable_sending_initial_data_)
    SendInitialData();
  pool_ = pool;

  // Bootstrap the read loop.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SpdySession::PumpReadLoop, weak_factory_.GetWeakPtr(),
                     READ_STATE_DO_READ, OK));
}

// {,Try}CreateStream() can be called with |in_io_loop_| set if a stream is
// being created in response to another being closed due to received data.

int SpdySession::TryCreateStream(
    const base::WeakPtr<SpdyStreamRequest>& request,
    base::WeakPtr<SpdyStream>* stream) {
  DCHECK(request);

  if (availability_state_ == STATE_GOING_AWAY)
    return ERR_FAILED;

  if (availability_state_ == STATE_DRAINING)
    return ERR_CONNECTION_CLOSED;

  // Fail if ChangeSocketTag() has been called.
  if (request->socket_tag_ != spdy_session_key_.socket_tag())
    return ERR_FAILED;

  if ((active_streams_.size() + created_streams_.size() <
       max_concurrent_streams_)) {
    return CreateStream(*request, stream);
  }

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_STALLED_MAX_STREAMS, [&] {
    return NetLogSpdySessionStalledParams(
        active_streams_.size(), created_streams_.size(),
        max_concurrent_streams_, request->url().spec());
  });

  RequestPriority priority = request->priority();
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);
  pending_create_stream_queues_[priority].push_back(request);
  return ERR_IO_PENDING;
}

int SpdySession::CreateStream(const SpdyStreamRequest& request,
                              base::WeakPtr<SpdyStream>* stream) {
  DCHECK_GE(request.priority(), MINIMUM_PRIORITY);
  DCHECK_LE(request.priority(), MAXIMUM_PRIORITY);

  if (availability_state_ == STATE_GOING_AWAY)
    return ERR_FAILED;

  if (availability_state_ == STATE_DRAINING)
    return ERR_CONNECTION_CLOSED;

  DCHECK(socket_);
  UMA_HISTOGRAM_BOOLEAN("Net.SpdySession.CreateStreamWithSocketConnected",
                        socket_->IsConnected());
  if (!socket_->IsConnected()) {
    DoDrainSession(
        ERR_CONNECTION_CLOSED,
        "Tried to create SPDY stream for a closed socket connection.");
    return ERR_CONNECTION_CLOSED;
  }

  auto new_stream = std::make_unique<SpdyStream>(
      request.type(), GetWeakPtr(), request.url(), request.priority(),
      stream_initial_send_window_size_, stream_max_recv_window_size_,
      request.net_log(), request.traffic_annotation(),
      request.detect_broken_connection_);
  *stream = new_stream->GetWeakPtr();
  InsertCreatedStream(std::move(new_stream));
  if (request.detect_broken_connection_)
    EnableBrokenConnectionDetection(request.heartbeat_interval_);

  return OK;
}

bool SpdySession::CancelStreamRequest(
    const base::WeakPtr<SpdyStreamRequest>& request) {
  DCHECK(request);
  RequestPriority priority = request->priority();
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);

#if DCHECK_IS_ON()
  // |request| should not be in a queue not matching its priority.
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    if (priority == i)
      continue;
    DCHECK(!base::Contains(pending_create_stream_queues_[i], request.get(),
                           &base::WeakPtr<SpdyStreamRequest>::get));
  }
#endif

  PendingStreamRequestQueue* queue = &pending_create_stream_queues_[priority];
  // Remove |request| from |queue| while preserving the order of the
  // other elements.
  PendingStreamRequestQueue::iterator it = base::ranges::find(
      *queue, request.get(), &base::WeakPtr<SpdyStreamRequest>::get);
  // The request may already be removed if there's a
  // CompleteStreamRequest() in flight.
  if (it != queue->end()) {
    it = queue->erase(it);
    // |request| should be in the queue at most once, and if it is
    // present, should not be pending completion.
    DCHECK(base::ranges::find(it, queue->end(), request.get(),
                              &base::WeakPtr<SpdyStreamRequest>::get) ==
           queue->end());
    return true;
  }
  return false;
}

void SpdySession::ChangeStreamRequestPriority(
    const base::WeakPtr<SpdyStreamRequest>& request,
    RequestPriority priority) {
  // |request->priority()| is updated by the caller after this returns.
  // |request| needs to still have its old priority in order for
  // CancelStreamRequest() to find it in the correct queue.
  DCHECK_NE(priority, request->priority());
  if (CancelStreamRequest(request)) {
    pending_create_stream_queues_[priority].push_back(request);
  }
}

base::WeakPtr<SpdyStreamRequest> SpdySession::GetNextPendingStreamRequest() {
  for (int j = MAXIMUM_PRIORITY; j >= MINIMUM_PRIORITY; --j) {
    if (pending_create_stream_queues_[j].empty())
      continue;

    base::WeakPtr<SpdyStreamRequest> pending_request =
        pending_create_stream_queues_[j].front();
    DCHECK(pending_request);
    pending_create_stream_queues_[j].pop_front();
    return pending_request;
  }
  return base::WeakPtr<SpdyStreamRequest>();
}

void SpdySession::ProcessPendingStreamRequests() {
  size_t max_requests_to_process =
      max_concurrent_streams_ -
      (active_streams_.size() + created_streams_.size());
  for (size_t i = 0; i < max_requests_to_process; ++i) {
    base::WeakPtr<SpdyStreamRequest> pending_request =
        GetNextPendingStreamRequest();
    if (!pending_request)
      break;

    // Note that this post can race with other stream creations, and it's
    // possible that the un-stalled stream will be stalled again if it loses.
    // TODO(jgraettinger): Provide stronger ordering guarantees.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpdySession::CompleteStreamRequest,
                                  weak_factory_.GetWeakPtr(), pending_request));
  }
}

void SpdySession::CloseActiveStreamIterator(ActiveStreamMap::iterator it,
                                            int status) {
  // TODO(mbelshe): We should send a RST_STREAM control frame here
  //                so that the server can cancel a large send.

  std::unique_ptr<SpdyStream> owned_stream(it->second);
  active_streams_.erase(it);
  priority_dependency_state_.OnStreamDestruction(owned_stream->stream_id());

  DeleteStream(std::move(owned_stream), status);

  if (active_streams_.empty() && created_streams_.empty()) {
    // If the socket belongs to a socket pool, and there are no active streams,
    // and the socket pool is stalled, then close the session to free up a
    // socket slot.
    if (stream_socket_handle_ && stream_socket_handle_->IsPoolStalled()) {
      DoDrainSession(ERR_CONNECTION_CLOSED, "Closing idle connection.");
    } else {
      MaybeFinishGoingAway();
    }
  }
}

void SpdySession::CloseCreatedStreamIterator(CreatedStreamSet::iterator it,
                                             int status) {
  std::unique_ptr<SpdyStream> owned_stream(*it);
  created_streams_.erase(it);
  DeleteStream(std::move(owned_stream), status);
}

void SpdySession::ResetStreamIterator(ActiveStreamMap::iterator it,
                                      int error,
                                      const std::string& description) {
  // Send the RST_STREAM frame first as CloseActiveStreamIterator()
  // may close us.
  spdy::SpdyErrorCode error_code = spdy::ERROR_CODE_PROTOCOL_ERROR;
  if (error == ERR_FAILED) {
    error_code = spdy::ERROR_CODE_INTERNAL_ERROR;
  } else if (error == ERR_ABORTED) {
    error_code = spdy::ERROR_CODE_CANCEL;
  } else if (error == ERR_HTTP2_FLOW_CONTROL_ERROR) {
    error_code = spdy::ERROR_CODE_FLOW_CONTROL_ERROR;
  } else if (error == ERR_TIMED_OUT) {
    error_code = spdy::ERROR_CODE_REFUSED_STREAM;
  } else if (error == ERR_HTTP2_STREAM_CLOSED) {
    error_code = spdy::ERROR_CODE_STREAM_CLOSED;
  }
  spdy::SpdyStreamId stream_id = it->first;
  RequestPriority priority = it->second->priority();
  EnqueueResetStreamFrame(stream_id, priority, error_code, description);

  // Removes any pending writes for the stream except for possibly an
  // in-flight one.
  CloseActiveStreamIterator(it, error);
}

void SpdySession::EnqueueResetStreamFrame(spdy::SpdyStreamId stream_id,
                                          RequestPriority priority,
                                          spdy::SpdyErrorCode error_code,
                                          const std::string& description) {
  DCHECK_NE(stream_id, 0u);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_RST_STREAM, [&] {
    return NetLogSpdySendRstStreamParams(stream_id, error_code, description);
  });

  DCHECK(buffered_spdy_framer_.get());
  std::unique_ptr<spdy::SpdySerializedFrame> rst_frame(
      buffered_spdy_framer_->CreateRstStream(stream_id, error_code));

  EnqueueSessionWrite(priority, spdy::SpdyFrameType::RST_STREAM,
                      std::move(rst_frame));
  RecordProtocolErrorHistogram(MapRstStreamStatusToProtocolError(error_code));
}

void SpdySession::EnqueuePriorityFrame(spdy::SpdyStreamId stream_id,
                                       spdy::SpdyStreamId dependency_id,
                                       int weight,
                                       bool exclusive) {
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_SEND_PRIORITY, [&] {
    return NetLogSpdyPriorityParams(stream_id, dependency_id, weight,
                                    exclusive);
  });

  DCHECK(buffered_spdy_framer_.get());
  std::unique_ptr<spdy::SpdySerializedFrame> frame(
      buffered_spdy_framer_->CreatePriority(stream_id, dependency_id, weight,
                                            exclusive));

  // PRIORITY frames describe sequenced updates to the tree, so they must
  // be serialized. We do this by queueing all PRIORITY frames at HIGHEST
  // priority.
  EnqueueWrite(HIGHEST, spdy::SpdyFrameType::PRIORITY,
               std::make_unique<SimpleBufferProducer>(
                   std::make_unique<SpdyBuffer>(std::move(frame))),
               base::WeakPtr<SpdyStream>(),
               kSpdySessionCommandsTrafficAnnotation);
}

void SpdySession::PumpReadLoop(ReadState expected_read_state, int result) {
  CHECK(!in_io_loop_);
  if (availability_state_ == STATE_DRAINING) {
    return;
  }
  std::ignore = DoReadLoop(expected_read_state, result);
}

int SpdySession::DoReadLoop(ReadState expected_read_state, int result) {
  CHECK(!in_io_loop_);
  CHECK_EQ(read_state_, expected_read_state);

  in_io_loop_ = true;

  int bytes_read_without_yielding = 0;
  const base::TimeTicks yield_after_time =
      time_func_() + base::Milliseconds(kYieldAfterDurationMilliseconds);

  // Loop until the session is draining, the read becomes blocked, or
  // the read limit is exceeded.
  while (true) {
    switch (read_state_) {
      case READ_STATE_DO_READ:
        CHECK_EQ(result, OK);
        result = DoRead();
        break;
      case READ_STATE_DO_READ_COMPLETE:
        if (result > 0)
          bytes_read_without_yielding += result;
        result = DoReadComplete(result);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "read_state_: " << read_state_;
        break;
    }

    if (availability_state_ == STATE_DRAINING)
      break;

    if (result == ERR_IO_PENDING)
      break;

    if (read_state_ == READ_STATE_DO_READ &&
        (bytes_read_without_yielding > kYieldAfterBytesRead ||
         time_func_() > yield_after_time)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&SpdySession::PumpReadLoop, weak_factory_.GetWeakPtr(),
                         READ_STATE_DO_READ, OK));
      result = ERR_IO_PENDING;
      break;
    }
  }

  CHECK(in_io_loop_);
  in_io_loop_ = false;

  return result;
}

int SpdySession::DoRead() {
  DCHECK(!read_buffer_);
  CHECK(in_io_loop_);

  CHECK(socket_);
  read_state_ = READ_STATE_DO_READ_COMPLETE;
  read_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
  int rv = socket_->ReadIfReady(
      read_buffer_.get(), kReadBufferSize,
      base::BindOnce(&SpdySession::PumpReadLoop, weak_factory_.GetWeakPtr(),
                     READ_STATE_DO_READ));
  if (rv == ERR_IO_PENDING) {
    read_buffer_ = nullptr;
    read_state_ = READ_STATE_DO_READ;
    return rv;
  }
  if (rv == ERR_READ_IF_READY_NOT_IMPLEMENTED) {
    // Fallback to regular Read().
    return socket_->Read(
        read_buffer_.get(), kReadBufferSize,
        base::BindOnce(&SpdySession::PumpReadLoop, weak_factory_.GetWeakPtr(),
                       READ_STATE_DO_READ_COMPLETE));
  }
  return rv;
}

int SpdySession::DoReadComplete(int result) {
  DCHECK(read_buffer_);
  CHECK(in_io_loop_);

  // Parse a frame.  For now this code requires that the frame fit into our
  // buffer (kReadBufferSize).
  // TODO(mbelshe): support arbitrarily large frames!

  if (result == 0) {
    DoDrainSession(ERR_CONNECTION_CLOSED, "Connection closed");
    return ERR_CONNECTION_CLOSED;
  }

  if (result < 0) {
    DoDrainSession(
        static_cast<Error>(result),
        base::StringPrintf("Error %d reading from socket.", -result));
    return result;
  }
  CHECK_LE(result, kReadBufferSize);

  last_read_time_ = time_func_();

  DCHECK(buffered_spdy_framer_.get());
  char* data = read_buffer_->data();
  while (result > 0) {
    uint32_t bytes_processed =
        buffered_spdy_framer_->ProcessInput(data, result);
    result -= bytes_processed;
    data += bytes_processed;

    if (availability_state_ == STATE_DRAINING) {
      return ERR_CONNECTION_CLOSED;
    }

    DCHECK_EQ(buffered_spdy_framer_->spdy_framer_error(),
              http2::Http2DecoderAdapter::SPDY_NO_ERROR);
  }

  read_buffer_ = nullptr;
  read_state_ = READ_STATE_DO_READ;
  return OK;
}

void SpdySession::PumpWriteLoop(WriteState expected_write_state, int result) {
  CHECK(!in_io_loop_);
  DCHECK_EQ(write_state_, expected_write_state);

  DoWriteLoop(expected_write_state, result);

  if (availability_state_ == STATE_DRAINING && !in_flight_write_ &&
      write_queue_.IsEmpty()) {
    pool_->RemoveUnavailableSession(GetWeakPtr());  // Destroys |this|.
    return;
  }
}

void SpdySession::MaybePostWriteLoop() {
  if (write_state_ == WRITE_STATE_IDLE) {
    CHECK(!in_flight_write_);
    write_state_ = WRITE_STATE_DO_WRITE;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SpdySession::PumpWriteLoop, weak_factory_.GetWeakPtr(),
                       WRITE_STATE_DO_WRITE, OK));
  }
}

int SpdySession::DoWriteLoop(WriteState expected_write_state, int result) {
  CHECK(!in_io_loop_);
  DCHECK_NE(write_state_, WRITE_STATE_IDLE);
  DCHECK_EQ(write_state_, expected_write_state);

  in_io_loop_ = true;

  // Loop until the session is closed or the write becomes blocked.
  while (true) {
    switch (write_state_) {
      case WRITE_STATE_DO_WRITE:
        DCHECK_EQ(result, OK);
        result = DoWrite();
        break;
      case WRITE_STATE_DO_WRITE_COMPLETE:
        result = DoWriteComplete(result);
        break;
      case WRITE_STATE_IDLE:
      default:
        NOTREACHED_IN_MIGRATION() << "write_state_: " << write_state_;
        break;
    }

    if (write_state_ == WRITE_STATE_IDLE) {
      DCHECK_EQ(result, ERR_IO_PENDING);
      break;
    }

    if (result == ERR_IO_PENDING)
      break;
  }

  CHECK(in_io_loop_);
  in_io_loop_ = false;

  return result;
}

int SpdySession::DoWrite() {
  CHECK(in_io_loop_);

  DCHECK(buffered_spdy_framer_);
  if (in_flight_write_) {
    DCHECK_GT(in_flight_write_->GetRemainingSize(), 0u);
  } else {
    // Grab the next frame to send.
    spdy::SpdyFrameType frame_type = spdy::SpdyFrameType::DATA;
    std::unique_ptr<SpdyBufferProducer> producer;
    base::WeakPtr<SpdyStream> stream;
    if (!write_queue_.Dequeue(&frame_type, &producer, &stream,
                              &in_flight_write_traffic_annotation_)) {
      write_state_ = WRITE_STATE_IDLE;
      return ERR_IO_PENDING;
    }

    if (stream.get())
      CHECK(!stream->IsClosed());

    // Activate the stream only when sending the HEADERS frame to
    // guarantee monotonically-increasing stream IDs.
    if (frame_type == spdy::SpdyFrameType::HEADERS) {
      CHECK(stream.get());
      CHECK_EQ(stream->stream_id(), 0u);
      std::unique_ptr<SpdyStream> owned_stream =
          ActivateCreatedStream(stream.get());
      InsertActivatedStream(std::move(owned_stream));

      if (stream_hi_water_mark_ > kLastStreamId) {
        CHECK_EQ(stream->stream_id(), kLastStreamId);
        // We've exhausted the stream ID space, and no new streams may be
        // created after this one.
        MakeUnavailable();
        StartGoingAway(kLastStreamId, ERR_HTTP2_PROTOCOL_ERROR);
      }
    }

    in_flight_write_ = producer->ProduceBuffer();
    if (!in_flight_write_) {
      NOTREACHED_IN_MIGRATION();
      return ERR_UNEXPECTED;
    }
    in_flight_write_frame_type_ = frame_type;
    in_flight_write_frame_size_ = in_flight_write_->GetRemainingSize();
    DCHECK_GE(in_flight_write_frame_size_, spdy::kFrameMinimumSize);
    in_flight_write_stream_ = stream;
  }

  write_state_ = WRITE_STATE_DO_WRITE_COMPLETE;

  scoped_refptr<IOBuffer> write_io_buffer =
      in_flight_write_->GetIOBufferForRemainingData();
  return socket_->Write(
      write_io_buffer.get(), in_flight_write_->GetRemainingSize(),
      base::BindOnce(&SpdySession::PumpWriteLoop, weak_factory_.GetWeakPtr(),
                     WRITE_STATE_DO_WRITE_COMPLETE),
      NetworkTrafficAnnotationTag(in_flight_write_traffic_annotation_));
}

int SpdySession::DoWriteComplete(int result) {
  CHECK(in_io_loop_);
  DCHECK_NE(result, ERR_IO_PENDING);
  DCHECK_GT(in_flight_write_->GetRemainingSize(), 0u);

  if (result < 0) {
    DCHECK_NE(result, ERR_IO_PENDING);
    in_flight_write_.reset();
    in_flight_write_frame_type_ = spdy::SpdyFrameType::DATA;
    in_flight_write_frame_size_ = 0;
    in_flight_write_stream_.reset();
    in_flight_write_traffic_annotation_.reset();
    write_state_ = WRITE_STATE_DO_WRITE;
    DoDrainSession(static_cast<Error>(result), "Write error");
    return OK;
  }

  // It should not be possible to have written more bytes than our
  // in_flight_write_.
  DCHECK_LE(static_cast<size_t>(result), in_flight_write_->GetRemainingSize());

  if (result > 0) {
    in_flight_write_->Consume(static_cast<size_t>(result));
    if (in_flight_write_stream_.get())
      in_flight_write_stream_->AddRawSentBytes(static_cast<size_t>(result));

    // We only notify the stream when we've fully written the pending frame.
    if (in_flight_write_->GetRemainingSize() == 0) {
      // It is possible that the stream was cancelled while we were
      // writing to the socket.
      if (in_flight_write_stream_.get()) {
        DCHECK_GT(in_flight_write_frame_size_, 0u);
        in_flight_write_stream_->OnFrameWriteComplete(
            in_flight_write_frame_type_, in_flight_write_frame_size_);
      }

      // Cleanup the write which just completed.
      in_flight_write_.reset();
      in_flight_write_frame_type_ = spdy::SpdyFrameType::DATA;
      in_flight_write_frame_size_ = 0;
      in_flight_write_stream_.reset();
    }
  }

  write_state_ = WRITE_STATE_DO_WRITE;
  return OK;
}

void SpdySession::NotifyRequestsOfConfirmation(int rv) {
  for (auto& callback : waiting_for_confirmation_callbacks_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), rv));
  }
  waiting_for_confirmation_callbacks_.clear();
  in_confirm_handshake_ = false;
}

void SpdySession::SendInitialData() {
  DCHECK(enable_sending_initial_data_);
  DCHECK(buffered_spdy_framer_.get());

  // Prepare initial SETTINGS frame.  Only send settings that have a value
  // different from the protocol default value.
  spdy::SettingsMap settings_map;
  for (auto setting : initial_settings_) {
    if (!IsSpdySettingAtDefaultInitialValue(setting.first, setting.second)) {
      settings_map.insert(setting);
    }
  }
  if (enable_http2_settings_grease_) {
    spdy::SpdySettingsId greased_id = 0x0a0a +
                                      0x1000 * base::RandGenerator(0xf + 1) +
                                      0x0010 * base::RandGenerator(0xf + 1);
    uint32_t greased_value = base::RandGenerator(
        static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1);
    // Let insertion silently fail if `settings_map` already contains
    // `greased_id`.
    settings_map.emplace(greased_id, greased_value);
  }
  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_SETTINGS, [&] {
    return NetLogSpdySendSettingsParams(&settings_map);
  });
  std::unique_ptr<spdy::SpdySerializedFrame> settings_frame(
      buffered_spdy_framer_->CreateSettings(settings_map));

  // Prepare initial WINDOW_UPDATE frame.
  // Make sure |session_max_recv_window_size_ - session_recv_window_size_|
  // does not underflow.
  DCHECK_GE(session_max_recv_window_size_, session_recv_window_size_);
  DCHECK_GE(session_recv_window_size_, 0);
  DCHECK_EQ(0, session_unacked_recv_window_bytes_);
  std::unique_ptr<spdy::SpdySerializedFrame> window_update_frame;
  const bool send_window_update =
      session_max_recv_window_size_ > session_recv_window_size_;
  if (send_window_update) {
    const int32_t delta_window_size =
        session_max_recv_window_size_ - session_recv_window_size_;
    session_recv_window_size_ += delta_window_size;
    net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_UPDATE_RECV_WINDOW, [&] {
      return NetLogSpdySessionWindowUpdateParams(delta_window_size,
                                                 session_recv_window_size_);
    });

    last_recv_window_update_ = base::TimeTicks::Now();
    session_unacked_recv_window_bytes_ += delta_window_size;
    net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_WINDOW_UPDATE, [&] {
      return NetLogSpdyWindowUpdateFrameParams(
          spdy::kSessionFlowControlStreamId,
          session_unacked_recv_window_bytes_);
    });
    window_update_frame = buffered_spdy_framer_->CreateWindowUpdate(
        spdy::kSessionFlowControlStreamId, session_unacked_recv_window_bytes_);
    session_unacked_recv_window_bytes_ = 0;
  }

  // Create a single frame to hold connection prefix, initial SETTINGS frame,
  // and optional initial WINDOW_UPDATE frame, so that they are sent on the wire
  // in a single packet.
  size_t initial_frame_size =
      spdy::kHttp2ConnectionHeaderPrefixSize + settings_frame->size();
  if (send_window_update)
    initial_frame_size += window_update_frame->size();
  auto initial_frame_data = std::make_unique<char[]>(initial_frame_size);
  size_t offset = 0;

  memcpy(initial_frame_data.get() + offset, spdy::kHttp2ConnectionHeaderPrefix,
         spdy::kHttp2ConnectionHeaderPrefixSize);
  offset += spdy::kHttp2ConnectionHeaderPrefixSize;

  memcpy(initial_frame_data.get() + offset, settings_frame->data(),
         settings_frame->size());
  offset += settings_frame->size();

  if (send_window_update) {
    memcpy(initial_frame_data.get() + offset, window_update_frame->data(),
           window_update_frame->size());
  }

  auto initial_frame = std::make_unique<spdy::SpdySerializedFrame>(
      std::move(initial_frame_data), initial_frame_size);
  EnqueueSessionWrite(HIGHEST, spdy::SpdyFrameType::SETTINGS,
                      std::move(initial_frame));
}

void SpdySession::HandleSetting(uint32_t id, uint32_t value) {
  switch (id) {
    case spdy::SETTINGS_HEADER_TABLE_SIZE:
      buffered_spdy_framer_->UpdateHeaderEncoderTableSize(value);
      break;
    case spdy::SETTINGS_MAX_CONCURRENT_STREAMS:
      max_concurrent_streams_ =
          std::min(static_cast<size_t>(value), kMaxConcurrentStreamLimit);
      ProcessPendingStreamRequests();
      break;
    case spdy::SETTINGS_INITIAL_WINDOW_SIZE: {
      if (value > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
        net_log_.AddEventWithIntParams(
            NetLogEventType::HTTP2_SESSION_INITIAL_WINDOW_SIZE_OUT_OF_RANGE,
            "initial_window_size", value);
        return;
      }

      // spdy::SETTINGS_INITIAL_WINDOW_SIZE updates initial_send_window_size_
      // only.
      int32_t delta_window_size =
          static_cast<int32_t>(value) - stream_initial_send_window_size_;
      stream_initial_send_window_size_ = static_cast<int32_t>(value);
      UpdateStreamsSendWindowSize(delta_window_size);
      net_log_.AddEventWithIntParams(
          NetLogEventType::HTTP2_SESSION_UPDATE_STREAMS_SEND_WINDOW_SIZE,
          "delta_window_size", delta_window_size);
      break;
    }
    case spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL:
      if ((value != 0 && value != 1) || (support_websocket_ && value == 0)) {
        DoDrainSession(
            ERR_HTTP2_PROTOCOL_ERROR,
            "Invalid value for spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL.");
        return;
      }
      if (value == 1) {
        support_websocket_ = true;
      }
      break;
    case spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES:
      if (value != 0 && value != 1) {
        DoDrainSession(
            ERR_HTTP2_PROTOCOL_ERROR,
            "Invalid value for spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES.");
        return;
      }
      if (settings_frame_received_) {
        if (value != (deprecate_http2_priorities_ ? 1 : 0)) {
          DoDrainSession(ERR_HTTP2_PROTOCOL_ERROR,
                         "spdy::SETTINGS_DEPRECATE_HTTP2_PRIORITIES value "
                         "changed after first SETTINGS frame.");
          return;
        }
      } else {
        if (value == 1) {
          deprecate_http2_priorities_ = true;
        }
      }
      break;
  }
}

void SpdySession::UpdateStreamsSendWindowSize(int32_t delta_window_size) {
  for (const auto& value : active_streams_) {
    if (!value.second->AdjustSendWindowSize(delta_window_size)) {
      DoDrainSession(
          ERR_HTTP2_FLOW_CONTROL_ERROR,
          base::StringPrintf(
              "New spdy::SETTINGS_INITIAL_WINDOW_SIZE value overflows "
              "flow control window of stream %d.",
              value.second->stream_id()));
      return;
    }
  }

  for (SpdyStream* const stream : created_streams_) {
    if (!stream->AdjustSendWindowSize(delta_window_size)) {
      DoDrainSession(
          ERR_HTTP2_FLOW_CONTROL_ERROR,
          base::StringPrintf(
              "New spdy::SETTINGS_INITIAL_WINDOW_SIZE value overflows "
              "flow control window of stream %d.",
              stream->stream_id()));
      return;
    }
  }
}

void SpdySession::MaybeCheckConnectionStatus() {
  if (NetworkChangeNotifier::IsDefaultNetworkActive())
    CheckConnectionStatus();
  else
    check_connection_on_radio_wakeup_ = true;
}

void SpdySession::MaybeSendPrefacePing() {
  if (ping_in_flight_ || check_ping_status_pending_ ||
      !enable_ping_based_connection_checking_) {
    return;
  }

  // If there has been no read activity in the session for some time,
  // then send a preface-PING.
  if (time_func_() > last_read_time_ + connection_at_risk_of_loss_time_)
    WritePingFrame(next_ping_id_, false);
}

void SpdySession::SendWindowUpdateFrame(spdy::SpdyStreamId stream_id,
                                        uint32_t delta_window_size,
                                        RequestPriority priority) {
  ActiveStreamMap::const_iterator it = active_streams_.find(stream_id);
  if (it != active_streams_.end()) {
    CHECK_EQ(it->second->stream_id(), stream_id);
  } else {
    CHECK_EQ(stream_id, spdy::kSessionFlowControlStreamId);
  }

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_WINDOW_UPDATE, [&] {
    return NetLogSpdyWindowUpdateFrameParams(stream_id, delta_window_size);
  });

  DCHECK(buffered_spdy_framer_.get());
  std::unique_ptr<spdy::SpdySerializedFrame> window_update_frame(
      buffered_spdy_framer_->CreateWindowUpdate(stream_id, delta_window_size));
  EnqueueSessionWrite(priority, spdy::SpdyFrameType::WINDOW_UPDATE,
                      std::move(window_update_frame));
}

void SpdySession::WritePingFrame(spdy::SpdyPingId unique_id, bool is_ack) {
  DCHECK(buffered_spdy_framer_.get());
  std::unique_ptr<spdy::SpdySerializedFrame> ping_frame(
      buffered_spdy_framer_->CreatePingFrame(unique_id, is_ack));
  EnqueueSessionWrite(HIGHEST, spdy::SpdyFrameType::PING,
                      std::move(ping_frame));

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_PING, [&] {
    return NetLogSpdyPingParams(unique_id, is_ack, "sent");
  });

  if (!is_ack) {
    DCHECK(!ping_in_flight_);

    ping_in_flight_ = true;
    ++next_ping_id_;
    PlanToCheckPingStatus();
    last_ping_sent_time_ = time_func_();
  }
}

void SpdySession::PlanToCheckPingStatus() {
  if (check_ping_status_pending_)
    return;

  check_ping_status_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpdySession::CheckPingStatus, weak_factory_.GetWeakPtr(),
                     time_func_()),
      hung_interval_);
}

void SpdySession::CheckPingStatus(base::TimeTicks last_check_time) {
  CHECK(!in_io_loop_);
  DCHECK(check_ping_status_pending_);

  if (!ping_in_flight_) {
    // A response has been received for the ping we had sent.
    check_ping_status_pending_ = false;
    return;
  }

  const base::TimeTicks now = time_func_();
  if (now > last_read_time_ + hung_interval_ ||
      last_read_time_ < last_check_time) {
    check_ping_status_pending_ = false;
    DoDrainSession(ERR_HTTP2_PING_FAILED, "Failed ping.");
    return;
  }

  // Check the status of connection after a delay.
  const base::TimeDelta delay = last_read_time_ + hung_interval_ - now;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpdySession::CheckPingStatus, weak_factory_.GetWeakPtr(),
                     now),
      delay);
}

spdy::SpdyStreamId SpdySession::GetNewStreamId() {
  CHECK_LE(stream_hi_water_mark_, kLastStreamId);
  spdy::SpdyStreamId id = stream_hi_water_mark_;
  stream_hi_water_mark_ += 2;
  return id;
}

void SpdySession::EnqueueSessionWrite(
    RequestPriority priority,
    spdy::SpdyFrameType frame_type,
    std::unique_ptr<spdy::SpdySerializedFrame> frame) {
  DCHECK(frame_type == spdy::SpdyFrameType::RST_STREAM ||
         frame_type == spdy::SpdyFrameType::SETTINGS ||
         frame_type == spdy::SpdyFrameType::WINDOW_UPDATE ||
         frame_type == spdy::SpdyFrameType::PING ||
         frame_type == spdy::SpdyFrameType::GOAWAY);
  DCHECK(IsSpdyFrameTypeWriteCapped(frame_type));
  if (write_queue_.num_queued_capped_frames() >
      session_max_queued_capped_frames_) {
    LOG(WARNING)
        << "Draining session due to exceeding max queued capped frames";
    // Use ERR_CONNECTION_CLOSED to avoid sending a GOAWAY frame since that
    // frame would also exceed the cap.
    DoDrainSession(ERR_CONNECTION_CLOSED, "Exceeded max queued capped frames");
    return;
  }
  auto buffer = std::make_unique<SpdyBuffer>(std::move(frame));
  EnqueueWrite(priority, frame_type,
               std::make_unique<SimpleBufferProducer>(std::move(buffer)),
               base::WeakPtr<SpdyStream>(),
               kSpdySessionCommandsTrafficAnnotation);
  if (greased_http2_frame_ && frame_type == spdy::SpdyFrameType::SETTINGS) {
    net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_GREASED_FRAME, [&] {
      return NetLogSpdyGreasedFrameParams(
          /* stream_id = */ 0, greased_http2_frame_.value().type,
          greased_http2_frame_.value().flags,
          greased_http2_frame_.value().payload.length(), priority);
    });

    EnqueueWrite(
        priority,
        static_cast<spdy::SpdyFrameType>(greased_http2_frame_.value().type),
        std::make_unique<GreasedBufferProducer>(base::WeakPtr<SpdyStream>(),
                                                &greased_http2_frame_.value(),
                                                buffered_spdy_framer_.get()),
        base::WeakPtr<SpdyStream>(), kSpdySessionCommandsTrafficAnnotation);
  }
}

void SpdySession::EnqueueWrite(
    RequestPriority priority,
    spdy::SpdyFrameType frame_type,
    std::unique_ptr<SpdyBufferProducer> producer,
    const base::WeakPtr<SpdyStream>& stream,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  if (availability_state_ == STATE_DRAINING)
    return;

  write_queue_.Enqueue(priority, frame_type, std::move(producer), stream,
                       traffic_annotation);
  MaybePostWriteLoop();
}

void SpdySession::InsertCreatedStream(std::unique_ptr<SpdyStream> stream) {
  CHECK_EQ(stream->stream_id(), 0u);
  auto it = created_streams_.lower_bound(stream.get());
  CHECK(it == created_streams_.end() || *it != stream.get());
  created_streams_.insert(it, stream.release());
}

std::unique_ptr<SpdyStream> SpdySession::ActivateCreatedStream(
    SpdyStream* stream) {
  CHECK_EQ(stream->stream_id(), 0u);
  auto it = created_streams_.find(stream);
  CHECK(it != created_streams_.end());
  stream->set_stream_id(GetNewStreamId());
  std::unique_ptr<SpdyStream> owned_stream(stream);
  created_streams_.erase(it);
  return owned_stream;
}

void SpdySession::InsertActivatedStream(std::unique_ptr<SpdyStream> stream) {
  spdy::SpdyStreamId stream_id = stream->stream_id();
  CHECK_NE(stream_id, 0u);
  std::pair<ActiveStreamMap::iterator, bool> result =
      active_streams_.emplace(stream_id, stream.get());
  CHECK(result.second);
  std::ignore = stream.release();
}

void SpdySession::DeleteStream(std::unique_ptr<SpdyStream> stream, int status) {
  if (in_flight_write_stream_.get() == stream.get()) {
    // If we're deleting the stream for the in-flight write, we still
    // need to let the write complete, so we clear
    // |in_flight_write_stream_| and let the write finish on its own
    // without notifying |in_flight_write_stream_|.
    in_flight_write_stream_.reset();
  }

  write_queue_.RemovePendingWritesForStream(stream.get());
  if (stream->detect_broken_connection())
    MaybeDisableBrokenConnectionDetection();
  stream->OnClose(status);

  if (availability_state_ == STATE_AVAILABLE) {
    ProcessPendingStreamRequests();
  }
}

void SpdySession::RecordHistograms() {
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsPerSession",
                              streams_initiated_count_, 1, 300, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.SpdyStreamsAbandonedPerSession",
                              streams_abandoned_count_, 1, 300, 50);
  UMA_HISTOGRAM_BOOLEAN("Net.SpdySession.ServerSupportsWebSocket",
                        support_websocket_);
}

void SpdySession::RecordProtocolErrorHistogram(
    SpdyProtocolErrorDetails details) {
  UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionErrorDetails2", details,
                            NUM_SPDY_PROTOCOL_ERROR_DETAILS);
  if (base::EndsWith(host_port_pair().host(), "google.com",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    UMA_HISTOGRAM_ENUMERATION("Net.SpdySessionErrorDetails_Google2", details,
                              NUM_SPDY_PROTOCOL_ERROR_DETAILS);
  }
}

void SpdySession::DcheckGoingAway() const {
#if DCHECK_IS_ON()
  DCHECK_GE(availability_state_, STATE_GOING_AWAY);
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    DCHECK(pending_create_stream_queues_[i].empty());
  }
  DCHECK(created_streams_.empty());
#endif
}

void SpdySession::DcheckDraining() const {
  DcheckGoingAway();
  DCHECK_EQ(availability_state_, STATE_DRAINING);
  DCHECK(active_streams_.empty());
}

void SpdySession::DoDrainSession(Error err, const std::string& description) {
  if (availability_state_ == STATE_DRAINING) {
    return;
  }
  MakeUnavailable();

  // Mark host_port_pair requiring HTTP/1.1 for subsequent connections.
  if (err == ERR_HTTP_1_1_REQUIRED) {
    http_server_properties_->SetHTTP11Required(
        url::SchemeHostPort(url::kHttpsScheme, host_port_pair().host(),
                            host_port_pair().port()),
        spdy_session_key_.network_anonymization_key());
  }

  // If |err| indicates an error occurred, inform the peer that we're closing
  // and why. Don't GOAWAY on a graceful or idle close, as that may
  // unnecessarily wake the radio. We could technically GOAWAY on network errors
  // (we'll probably fail to actually write it, but that's okay), however many
  // unit-tests would need to be updated.
  if (err != OK &&
      err != ERR_ABORTED &&  // Used by SpdySessionPool to close idle sessions.
      err != ERR_NETWORK_CHANGED &&  // Used to deprecate sessions on IP change.
      err != ERR_SOCKET_NOT_CONNECTED && err != ERR_HTTP_1_1_REQUIRED &&
      err != ERR_CONNECTION_CLOSED && err != ERR_CONNECTION_RESET) {
    // Enqueue a GOAWAY to inform the peer of why we're closing the connection.
    spdy::SpdyGoAwayIR goaway_ir(/* last_good_stream_id = */ 0,
                                 MapNetErrorToGoAwayStatus(err), description);
    auto frame = std::make_unique<spdy::SpdySerializedFrame>(
        buffered_spdy_framer_->SerializeFrame(goaway_ir));
    EnqueueSessionWrite(HIGHEST, spdy::SpdyFrameType::GOAWAY, std::move(frame));
  }

  availability_state_ = STATE_DRAINING;
  error_on_close_ = err;

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_CLOSE, [&] {
    return NetLogSpdySessionCloseParams(err, description);
  });

  base::UmaHistogramSparse("Net.SpdySession.ClosedOnError", -err);

  if (err == OK) {
    // We ought to be going away already, as this is a graceful close.
    DcheckGoingAway();
  } else {
    StartGoingAway(0, err);
  }
  DcheckDraining();
  MaybePostWriteLoop();
}

void SpdySession::LogAbandonedStream(SpdyStream* stream, Error status) {
  DCHECK(stream);
  stream->LogStreamError(status, "Abandoned.");
  // We don't increment the streams abandoned counter here. If the
  // stream isn't active (i.e., it hasn't written anything to the wire
  // yet) then it's as if it never existed. If it is active, then
  // LogAbandonedActiveStream() will increment the counters.
}

void SpdySession::LogAbandonedActiveStream(ActiveStreamMap::const_iterator it,
                                           Error status) {
  DCHECK_GT(it->first, 0u);
  LogAbandonedStream(it->second, status);
  ++streams_abandoned_count_;
}

void SpdySession::CompleteStreamRequest(
    const base::WeakPtr<SpdyStreamRequest>& pending_request) {
  // Abort if the request has already been cancelled.
  if (!pending_request)
    return;

  base::WeakPtr<SpdyStream> stream;
  int rv = TryCreateStream(pending_request, &stream);

  if (rv == OK) {
    DCHECK(stream);
    pending_request->OnRequestCompleteSuccess(stream);
    return;
  }
  DCHECK(!stream);

  if (rv != ERR_IO_PENDING) {
    pending_request->OnRequestCompleteFailure(rv);
  }
}

void SpdySession::OnError(
    http2::Http2DecoderAdapter::SpdyFramerError spdy_framer_error) {
  CHECK(in_io_loop_);

  RecordProtocolErrorHistogram(
      MapFramerErrorToProtocolError(spdy_framer_error));
  std::string description = base::StringPrintf(
      "Framer error: %d (%s).", spdy_framer_error,
      http2::Http2DecoderAdapter::SpdyFramerErrorToString(spdy_framer_error));
  DoDrainSession(MapFramerErrorToNetError(spdy_framer_error), description);
}

void SpdySession::OnStreamError(spdy::SpdyStreamId stream_id,
                                const std::string& description) {
  CHECK(in_io_loop_);

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end()) {
    // We still want to send a frame to reset the stream even if we
    // don't know anything about it.
    EnqueueResetStreamFrame(stream_id, IDLE, spdy::ERROR_CODE_PROTOCOL_ERROR,
                            description);
    return;
  }

  ResetStreamIterator(it, ERR_HTTP2_PROTOCOL_ERROR, description);
}

void SpdySession::OnPing(spdy::SpdyPingId unique_id, bool is_ack) {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_PING, [&] {
    return NetLogSpdyPingParams(unique_id, is_ack, "received");
  });

  // Send response to a PING from server.
  if (!is_ack) {
    WritePingFrame(unique_id, true);
    return;
  }

  if (!ping_in_flight_) {
    RecordProtocolErrorHistogram(PROTOCOL_ERROR_UNEXPECTED_PING);
    DoDrainSession(ERR_HTTP2_PROTOCOL_ERROR, "Unexpected PING ACK.");
    return;
  }

  ping_in_flight_ = false;

  // Record RTT in histogram when there are no more pings in flight.
  base::TimeDelta ping_duration = time_func_() - last_ping_sent_time_;
  if (network_quality_estimator_) {
    network_quality_estimator_->RecordSpdyPingLatency(host_port_pair(),
                                                      ping_duration);
  }
}

void SpdySession::OnRstStream(spdy::SpdyStreamId stream_id,
                              spdy::SpdyErrorCode error_code) {
  CHECK(in_io_loop_);

  // Use sparse histogram to record the unlikely case that a server sends
  // an unknown error code.
  base::UmaHistogramSparse("Net.SpdySession.RstStreamReceived", error_code);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_RST_STREAM, [&] {
    return NetLogSpdyRecvRstStreamParams(stream_id, error_code);
  });

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end()) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received RST for invalid stream" << stream_id;
    return;
  }

  DCHECK(it->second);
  CHECK_EQ(it->second->stream_id(), stream_id);

  if (error_code == spdy::ERROR_CODE_NO_ERROR) {
    CloseActiveStreamIterator(it, ERR_HTTP2_RST_STREAM_NO_ERROR_RECEIVED);
  } else if (error_code == spdy::ERROR_CODE_REFUSED_STREAM) {
    CloseActiveStreamIterator(it, ERR_HTTP2_SERVER_REFUSED_STREAM);
  } else if (error_code == spdy::ERROR_CODE_HTTP_1_1_REQUIRED) {
    // TODO(bnc): Record histogram with number of open streams capped at 50.
    it->second->LogStreamError(ERR_HTTP_1_1_REQUIRED,
                               "Closing session because server reset stream "
                               "with ERR_HTTP_1_1_REQUIRED.");
    DoDrainSession(ERR_HTTP_1_1_REQUIRED, "HTTP_1_1_REQUIRED for stream.");
  } else {
    RecordProtocolErrorHistogram(
        PROTOCOL_ERROR_RST_STREAM_FOR_NON_ACTIVE_STREAM);
    it->second->LogStreamError(ERR_HTTP2_PROTOCOL_ERROR,
                               "Server reset stream.");
    // TODO(mbelshe): Map from Spdy-protocol errors to something sensical.
    //                For now, it doesn't matter much - it is a protocol error.
    CloseActiveStreamIterator(it, ERR_HTTP2_PROTOCOL_ERROR);
  }
}

void SpdySession::OnGoAway(spdy::SpdyStreamId last_accepted_stream_id,
                           spdy::SpdyErrorCode error_code,
                           std::string_view debug_data) {
  CHECK(in_io_loop_);

  // Use sparse histogram to record the unlikely case that a server sends
  // an unknown error code.
  base::UmaHistogramSparse("Net.SpdySession.GoAwayReceived", error_code);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_GOAWAY,
                    [&](NetLogCaptureMode capture_mode) {
                      return NetLogSpdyRecvGoAwayParams(
                          last_accepted_stream_id, active_streams_.size(),
                          error_code, debug_data, capture_mode);
                    });
  MakeUnavailable();
  if (error_code == spdy::ERROR_CODE_HTTP_1_1_REQUIRED) {
    // TODO(bnc): Record histogram with number of open streams capped at 50.
    DoDrainSession(ERR_HTTP_1_1_REQUIRED, "HTTP_1_1_REQUIRED for stream.");
  } else if (error_code == spdy::ERROR_CODE_NO_ERROR) {
    StartGoingAway(last_accepted_stream_id, ERR_HTTP2_SERVER_REFUSED_STREAM);
  } else {
    StartGoingAway(last_accepted_stream_id, ERR_HTTP2_PROTOCOL_ERROR);
  }
  // This is to handle the case when we already don't have any active
  // streams (i.e., StartGoingAway() did nothing). Otherwise, we have
  // active streams and so the last one being closed will finish the
  // going away process (see DeleteStream()).
  MaybeFinishGoingAway();
}

void SpdySession::OnDataFrameHeader(spdy::SpdyStreamId stream_id,
                                    size_t length,
                                    bool fin) {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_DATA, [&] {
    return NetLogSpdyDataParams(stream_id, length, fin);
  });

  auto it = active_streams_.find(stream_id);

  // By the time data comes in, the stream may already be inactive.
  if (it == active_streams_.end())
    return;

  SpdyStream* stream = it->second;
  CHECK_EQ(stream->stream_id(), stream_id);

  DCHECK(buffered_spdy_framer_);
  stream->AddRawReceivedBytes(spdy::kDataFrameMinimumSize);
}

void SpdySession::OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                    const char* data,
                                    size_t len) {
  CHECK(in_io_loop_);
  DCHECK_LT(len, 1u << 24);

  // Build the buffer as early as possible so that we go through the
  // session flow control checks and update
  // |unacked_recv_window_bytes_| properly even when the stream is
  // inactive (since the other side has still reduced its session send
  // window).
  std::unique_ptr<SpdyBuffer> buffer;
  if (data) {
    DCHECK_GT(len, 0u);
    CHECK_LE(len, static_cast<size_t>(kReadBufferSize));
    buffer = std::make_unique<SpdyBuffer>(data, len);

    DecreaseRecvWindowSize(static_cast<int32_t>(len));
    buffer->AddConsumeCallback(base::BindRepeating(
        &SpdySession::OnReadBufferConsumed, weak_factory_.GetWeakPtr()));
  } else {
    DCHECK_EQ(len, 0u);
  }

  auto it = active_streams_.find(stream_id);

  // By the time data comes in, the stream may already be inactive.
  if (it == active_streams_.end())
    return;

  SpdyStream* stream = it->second;
  CHECK_EQ(stream->stream_id(), stream_id);

  stream->AddRawReceivedBytes(len);
  stream->OnDataReceived(std::move(buffer));
}

void SpdySession::OnStreamEnd(spdy::SpdyStreamId stream_id) {
  CHECK(in_io_loop_);

  auto it = active_streams_.find(stream_id);
  // By the time data comes in, the stream may already be inactive.
  if (it == active_streams_.end())
    return;

  SpdyStream* stream = it->second;
  CHECK_EQ(stream->stream_id(), stream_id);

  stream->OnDataReceived(std::unique_ptr<SpdyBuffer>());
}

void SpdySession::OnStreamPadding(spdy::SpdyStreamId stream_id, size_t len) {
  CHECK(in_io_loop_);

  // Decrease window size because padding bytes are received.
  // Increase window size because padding bytes are consumed (by discarding).
  // Net result: |session_unacked_recv_window_bytes_| increases by |len|,
  // |session_recv_window_size_| does not change.
  DecreaseRecvWindowSize(static_cast<int32_t>(len));
  IncreaseRecvWindowSize(static_cast<int32_t>(len));

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end())
    return;
  it->second->OnPaddingConsumed(len);
}

void SpdySession::OnSettings() {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_SETTINGS);
  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_SEND_SETTINGS_ACK);

  if (!settings_frame_received_) {
    base::UmaHistogramCounts1000(
        "Net.SpdySession.OnSettings.CreatedStreamCount2",
        created_streams_.size());
    base::UmaHistogramCounts1000(
        "Net.SpdySession.OnSettings.ActiveStreamCount2",
        active_streams_.size());
    base::UmaHistogramCounts1000(
        "Net.SpdySession.OnSettings.CreatedAndActiveStreamCount2",
        created_streams_.size() + active_streams_.size());
    base::UmaHistogramCounts1000(
        "Net.SpdySession.OnSettings.PendingStreamCount2",
        GetTotalSize(pending_create_stream_queues_));
  }

  // Send an acknowledgment of the setting.
  spdy::SpdySettingsIR settings_ir;
  settings_ir.set_is_ack(true);
  auto frame = std::make_unique<spdy::SpdySerializedFrame>(
      buffered_spdy_framer_->SerializeFrame(settings_ir));
  EnqueueSessionWrite(HIGHEST, spdy::SpdyFrameType::SETTINGS, std::move(frame));
}

void SpdySession::OnSettingsAck() {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_SETTINGS_ACK);
}

void SpdySession::OnSetting(spdy::SpdySettingsId id, uint32_t value) {
  CHECK(in_io_loop_);

  HandleSetting(id, value);

  // Log the setting.
  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_SETTING,
                    [&] { return NetLogSpdyRecvSettingParams(id, value); });
}

void SpdySession::OnSettingsEnd() {
  settings_frame_received_ = true;
}

void SpdySession::OnWindowUpdate(spdy::SpdyStreamId stream_id,
                                 int delta_window_size) {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_WINDOW_UPDATE, [&] {
    return NetLogSpdyWindowUpdateFrameParams(stream_id, delta_window_size);
  });

  if (stream_id == spdy::kSessionFlowControlStreamId) {
    // WINDOW_UPDATE for the session.
    if (delta_window_size < 1) {
      RecordProtocolErrorHistogram(PROTOCOL_ERROR_INVALID_WINDOW_UPDATE_SIZE);
      DoDrainSession(
          ERR_HTTP2_PROTOCOL_ERROR,
          "Received WINDOW_UPDATE with an invalid delta_window_size " +
              base::NumberToString(delta_window_size));
      return;
    }

    IncreaseSendWindowSize(delta_window_size);
  } else {
    // WINDOW_UPDATE for a stream.
    auto it = active_streams_.find(stream_id);

    if (it == active_streams_.end()) {
      // NOTE:  it may just be that the stream was cancelled.
      LOG(WARNING) << "Received WINDOW_UPDATE for invalid stream " << stream_id;
      return;
    }

    SpdyStream* stream = it->second;
    CHECK_EQ(stream->stream_id(), stream_id);

    if (delta_window_size < 1) {
      ResetStreamIterator(
          it, ERR_HTTP2_FLOW_CONTROL_ERROR,
          "Received WINDOW_UPDATE with an invalid delta_window_size.");
      return;
    }

    CHECK_EQ(it->second->stream_id(), stream_id);
    it->second->IncreaseSendWindowSize(delta_window_size);
  }
}

void SpdySession::OnPushPromise(spdy::SpdyStreamId /*stream_id*/,
                                spdy::SpdyStreamId /*promised_stream_id*/,
                                quiche::HttpHeaderBlock /*headers*/) {
  CHECK(in_io_loop_);
  DoDrainSession(ERR_HTTP2_PROTOCOL_ERROR, "PUSH_PROMISE received");
}

void SpdySession::OnHeaders(spdy::SpdyStreamId stream_id,
                            bool has_priority,
                            int weight,
                            spdy::SpdyStreamId parent_stream_id,
                            bool exclusive,
                            bool fin,
                            quiche::HttpHeaderBlock headers,
                            base::TimeTicks recv_first_byte_time) {
  CHECK(in_io_loop_);

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_RECV_HEADERS,
                    [&](NetLogCaptureMode capture_mode) {
                      return NetLogSpdyHeadersReceivedParams(
                          &headers, fin, stream_id, capture_mode);
                    });

  auto it = active_streams_.find(stream_id);
  if (it == active_streams_.end()) {
    // NOTE:  it may just be that the stream was cancelled.
    LOG(WARNING) << "Received HEADERS for invalid stream " << stream_id;
    return;
  }

  SpdyStream* stream = it->second;
  CHECK_EQ(stream->stream_id(), stream_id);

  stream->AddRawReceivedBytes(last_compressed_frame_len_);
  last_compressed_frame_len_ = 0;

  base::Time response_time = base::Time::Now();
  // May invalidate |stream|.
  stream->OnHeadersReceived(headers, response_time, recv_first_byte_time);
}

void SpdySession::OnAltSvc(
    spdy::SpdyStreamId stream_id,
    std::string_view origin,
    const spdy::SpdyAltSvcWireFormat::AlternativeServiceVector& altsvc_vector) {
  url::SchemeHostPort scheme_host_port;
  if (stream_id == 0) {
    if (origin.empty())
      return;
    const GURL gurl(origin);
    if (!gurl.is_valid() || gurl.host().empty())
      return;
    if (!gurl.SchemeIs(url::kHttpsScheme))
      return;
    SSLInfo ssl_info;
    if (!GetSSLInfo(&ssl_info)) {
      return;
    }
    if (!CanPool(transport_security_state_, ssl_info, *ssl_config_service_,
                 host_port_pair().host(), gurl.host_piece())) {
      return;
    }
    scheme_host_port = url::SchemeHostPort(gurl);
  } else {
    if (!origin.empty())
      return;
    const ActiveStreamMap::iterator it = active_streams_.find(stream_id);
    if (it == active_streams_.end())
      return;
    const GURL& gurl(it->second->url());
    if (!gurl.SchemeIs(url::kHttpsScheme))
      return;
    scheme_host_port = url::SchemeHostPort(gurl);
  }

  http_server_properties_->SetAlternativeServices(
      scheme_host_port, spdy_session_key_.network_anonymization_key(),
      ProcessAlternativeServices(altsvc_vector, is_http2_enabled_,
                                 is_quic_enabled_, quic_supported_versions_));
}

bool SpdySession::OnUnknownFrame(spdy::SpdyStreamId stream_id,
                                 uint8_t frame_type) {
  if (stream_id % 2 == 1) {
    return stream_id <= stream_hi_water_mark_;
  } else {
    // Reject frames on push streams, but not on the control stream.
    return stream_id == 0;
  }
}

void SpdySession::OnSendCompressedFrame(spdy::SpdyStreamId stream_id,
                                        spdy::SpdyFrameType type,
                                        size_t payload_len,
                                        size_t frame_len) {
  if (type != spdy::SpdyFrameType::HEADERS) {
    return;
  }

  DCHECK(buffered_spdy_framer_.get());
  size_t compressed_len = frame_len - spdy::kFrameMinimumSize;

  if (payload_len) {
    // Make sure we avoid early decimal truncation.
    int compression_pct = 100 - (100 * compressed_len) / payload_len;
    UMA_HISTOGRAM_PERCENTAGE("Net.SpdyHeadersCompressionPercentage",
                             compression_pct);
  }
}

void SpdySession::OnReceiveCompressedFrame(spdy::SpdyStreamId stream_id,
                                           spdy::SpdyFrameType type,
                                           size_t frame_len) {
  last_compressed_frame_len_ = frame_len;
}

void SpdySession::OnWriteBufferConsumed(
    size_t frame_payload_size,
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  // We can be called with |in_io_loop_| set if a write SpdyBuffer is
  // deleted (e.g., a stream is closed due to incoming data).
  if (consume_source == SpdyBuffer::DISCARD) {
    // If we're discarding a frame or part of it, increase the send
    // window by the number of discarded bytes. (Although if we're
    // discarding part of a frame, it's probably because of a write
    // error and we'll be tearing down the session soon.)
    int remaining_payload_bytes = std::min(consume_size, frame_payload_size);
    DCHECK_GT(remaining_payload_bytes, 0);
    IncreaseSendWindowSize(remaining_payload_bytes);
  }
  // For consumed bytes, the send window is increased when we receive
  // a WINDOW_UPDATE frame.
}

void SpdySession::IncreaseSendWindowSize(int delta_window_size) {
  // We can be called with |in_io_loop_| set if a SpdyBuffer is
  // deleted (e.g., a stream is closed due to incoming data).
  DCHECK_GE(delta_window_size, 1);

  // Check for overflow.
  int32_t max_delta_window_size =
      std::numeric_limits<int32_t>::max() - session_send_window_size_;
  if (delta_window_size > max_delta_window_size) {
    RecordProtocolErrorHistogram(PROTOCOL_ERROR_INVALID_WINDOW_UPDATE_SIZE);
    DoDrainSession(
        ERR_HTTP2_PROTOCOL_ERROR,
        "Received WINDOW_UPDATE [delta: " +
            base::NumberToString(delta_window_size) +
            "] for session overflows session_send_window_size_ [current: " +
            base::NumberToString(session_send_window_size_) + "]");
    return;
  }

  session_send_window_size_ += delta_window_size;

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_UPDATE_SEND_WINDOW, [&] {
    return NetLogSpdySessionWindowUpdateParams(delta_window_size,
                                               session_send_window_size_);
  });

  DCHECK(!IsSendStalled());
  ResumeSendStalledStreams();
}

void SpdySession::DecreaseSendWindowSize(int32_t delta_window_size) {
  // We only call this method when sending a frame. Therefore,
  // |delta_window_size| should be within the valid frame size range.
  DCHECK_GE(delta_window_size, 1);
  DCHECK_LE(delta_window_size, kMaxSpdyFrameChunkSize);

  // |send_window_size_| should have been at least |delta_window_size| for
  // this call to happen.
  DCHECK_GE(session_send_window_size_, delta_window_size);

  session_send_window_size_ -= delta_window_size;

  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_UPDATE_SEND_WINDOW, [&] {
    return NetLogSpdySessionWindowUpdateParams(-delta_window_size,
                                               session_send_window_size_);
  });
}

void SpdySession::OnReadBufferConsumed(
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  // We can be called with |in_io_loop_| set if a read SpdyBuffer is
  // deleted (e.g., discarded by a SpdyReadQueue).
  DCHECK_GE(consume_size, 1u);
  DCHECK_LE(consume_size,
            static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  IncreaseRecvWindowSize(static_cast<int32_t>(consume_size));
}

void SpdySession::IncreaseRecvWindowSize(int32_t delta_window_size) {
  DCHECK_GE(session_unacked_recv_window_bytes_, 0);
  DCHECK_GE(session_recv_window_size_, session_unacked_recv_window_bytes_);
  DCHECK_GE(delta_window_size, 1);
  // Check for overflow.
  DCHECK_LE(delta_window_size,
            std::numeric_limits<int32_t>::max() - session_recv_window_size_);

  session_recv_window_size_ += delta_window_size;
  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_UPDATE_RECV_WINDOW, [&] {
    return NetLogSpdySessionWindowUpdateParams(delta_window_size,
                                               session_recv_window_size_);
  });

  // Update the receive window once half of the buffer is ready to be acked
  // to prevent excessive window updates on fast downloads. Also send an update
  // if too much time has elapsed since the last update to deal with
  // slow-reading clients so the server doesn't think the session is idle.
  session_unacked_recv_window_bytes_ += delta_window_size;
  const base::TimeDelta elapsed =
      base::TimeTicks::Now() - last_recv_window_update_;
  if (session_unacked_recv_window_bytes_ > session_max_recv_window_size_ / 2 ||
      elapsed >= time_to_buffer_small_window_updates_) {
    last_recv_window_update_ = base::TimeTicks::Now();
    SendWindowUpdateFrame(spdy::kSessionFlowControlStreamId,
                          session_unacked_recv_window_bytes_, HIGHEST);
    session_unacked_recv_window_bytes_ = 0;
  }
}

void SpdySession::DecreaseRecvWindowSize(int32_t delta_window_size) {
  CHECK(in_io_loop_);
  DCHECK_GE(delta_window_size, 1);

  // The receiving window size as the peer knows it is
  // |session_recv_window_size_ - session_unacked_recv_window_bytes_|, if more
  // data are sent by the peer, that means that the receive window is not being
  // respected.
  int32_t receiving_window_size =
      session_recv_window_size_ - session_unacked_recv_window_bytes_;
  if (delta_window_size > receiving_window_size) {
    RecordProtocolErrorHistogram(PROTOCOL_ERROR_RECEIVE_WINDOW_VIOLATION);
    DoDrainSession(
        ERR_HTTP2_FLOW_CONTROL_ERROR,
        "delta_window_size is " + base::NumberToString(delta_window_size) +
            " in DecreaseRecvWindowSize, which is larger than the receive " +
            "window size of " + base::NumberToString(receiving_window_size));
    return;
  }

  session_recv_window_size_ -= delta_window_size;
  net_log_.AddEvent(NetLogEventType::HTTP2_SESSION_UPDATE_RECV_WINDOW, [&] {
    return NetLogSpdySessionWindowUpdateParams(-delta_window_size,
                                               session_recv_window_size_);
  });
}

void SpdySession::QueueSendStalledStream(const SpdyStream& stream) {
  DCHECK(stream.send_stalled_by_flow_control() || IsSendStalled());
  RequestPriority priority = stream.priority();
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);
  stream_send_unstall_queue_[priority].push_back(stream.stream_id());
}

void SpdySession::ResumeSendStalledStreams() {
  // We don't have to worry about new streams being queued, since
  // doing so would cause IsSendStalled() to return true. But we do
  // have to worry about streams being closed, as well as ourselves
  // being closed.

  base::circular_deque<SpdyStream*> streams_to_requeue;

  while (!IsSendStalled()) {
    size_t old_size = 0;
#if DCHECK_IS_ON()
    old_size = GetTotalSize(stream_send_unstall_queue_);
#endif

    spdy::SpdyStreamId stream_id = PopStreamToPossiblyResume();
    if (stream_id == 0)
      break;
    ActiveStreamMap::const_iterator it = active_streams_.find(stream_id);
    // The stream may actually still be send-stalled after this (due
    // to its own send window) but that's okay -- it'll then be
    // resumed once its send window increases.
    if (it != active_streams_.end()) {
      if (it->second->PossiblyResumeIfSendStalled() == SpdyStream::Requeue)
        streams_to_requeue.push_back(it->second);
    }

    // The size should decrease unless we got send-stalled again.
    if (!IsSendStalled())
      DCHECK_LT(GetTotalSize(stream_send_unstall_queue_), old_size);
  }
  while (!streams_to_requeue.empty()) {
    SpdyStream* stream = streams_to_requeue.front();
    streams_to_requeue.pop_front();
    QueueSendStalledStream(*stream);
  }
}

spdy::SpdyStreamId SpdySession::PopStreamToPossiblyResume() {
  for (int i = MAXIMUM_PRIORITY; i >= MINIMUM_PRIORITY; --i) {
    base::circular_deque<spdy::SpdyStreamId>* queue =
        &stream_send_unstall_queue_[i];
    if (!queue->empty()) {
      spdy::SpdyStreamId stream_id = queue->front();
      queue->pop_front();
      return stream_id;
    }
  }
  return 0;
}

void SpdySession::CheckConnectionStatus() {
  MaybeSendPrefacePing();
  // Also schedule the next check.
  heartbeat_timer_.Start(
      FROM_HERE, heartbeat_interval_,
      base::BindOnce(&SpdySession::MaybeCheckConnectionStatus,
                     weak_factory_.GetWeakPtr()));
}

void SpdySession::OnDefaultNetworkActive() {
  if (!check_connection_on_radio_wakeup_)
    return;

  check_connection_on_radio_wakeup_ = false;
  CheckConnectionStatus();
}

void SpdySession::MaybeDisableBrokenConnectionDetection() {
  DCHECK_GT(broken_connection_detection_requests_, 0);
  DCHECK(IsBrokenConnectionDetectionEnabled());
  if (--broken_connection_detection_requests_ > 0)
    return;

  heartbeat_timer_.Stop();
  NetworkChangeNotifier::RemoveDefaultNetworkActiveObserver(this);
  check_connection_on_radio_wakeup_ = false;
}

}  // namespace net
