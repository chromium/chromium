// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_ALPS_DECODER_H_
#define NET_SPDY_ALPS_DECODER_H_

#include <cstddef>

#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/http2/core/http2_frame_decoder_adapter.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_no_op_visitor.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"

namespace net {

// Class to parse HTTP/2 frames in the extension_data field
// of the ALPS TLS extension.
class NET_EXPORT_PRIVATE AlpsDecoder {
 public:
  // These values are persisted to logs. Entries should not be renumbered, and
  // numeric values should never be reused.
  enum class Error {
    // No error has occurred.
    kNoError = 0,
    // HTTP/2 framing error detected by Http2DecoderAdapter.
    kFramingError = 1,
    // Forbidden HTTP/2 frame received.
    kForbiddenFrame = 2,
    // Input does not end on HTTP/2 frame boundary.
    kNotOnFrameBoundary = 3,
    // SETTINGS frame with ACK received.
    kSettingsWithAck = 4,
    // ACCEPT_CH received on invalid stream.
    kAcceptChInvalidStream = 5,
    // ACCEPT_CH received with flags.
    kAcceptChWithFlags = 6,
    // Malformed ACCEPT_CH payload.
    kAcceptChMalformed = 7,
    kMaxValue = kAcceptChMalformed
  };

  AlpsDecoder();
  ~AlpsDecoder();

  // Decode a stream of HTTP/2 frames received via the ALPS TLS extension.
  // The HTTP/2 connection preface MUST NOT be present in the input.
  // Frames other than SETTINGS and ACCEPT_CH are ignored other than for the
  // purposes of enforcing HTTP/2 framing rules.
  // May only be called once, with the entire ALPS extension_data.
  // Returns an error code, or Error::kNoError if no error has occurred.
  // The requirement that the first frame MUST be SETTINGS is not enforced,
  // because that only applies to HTTP/2 connections, not ALPS data.
  [[nodiscard]] Error Decode(base::span<const char> data);

  // The number of SETTINGS frames received.
  int settings_frame_count() const;

  // The HTTP/2 setting parameters parsed from |data|.
  const spdy::SettingsMap& GetSettings() const {
    return settings_parser_.GetSettings();
  }
  // Origins and corresponding Accept-CH values parsed from |data|.  See
  // https://tools.ietf.org/html/draft-davidben-http-client-hint-reliability-02
  const std::vector<spdy::AcceptChOriginValuePair>& GetAcceptCh() const {
    return accept_ch_parser_.GetAcceptCh();
  }

 private:
  class SettingsParser : public spdy::SpdyNoOpVisitor {
   public:
    SettingsParser();
    ~SettingsParser() override;

    bool forbidden_frame_received() const { return forbidden_frame_received_; }
    bool settings_ack_received() const { return settings_ack_received_; }
    int settings_frame_count() const { return settings_frame_count_; }
    // Number of SETTINGS frames received.
    const spdy::SettingsMap& GetSettings() const { return settings_; }

    // SpdyFramerVisitorInterface overrides.
    void OnCommonHeader(spdy::SpdyStreamId stream_id,
                        size_t length,
                        uint8_t type,
                        uint8_t flags) override;
    void OnSettings() override;
    void OnSetting(spdy::SpdySettingsId id, uint32_t value) override;
    void OnSettingsAck() override;

   private:
    // True if a forbidden HTTP/2 frame has been received.
    bool forbidden_frame_received_ = false;
    // True if a SETTINGS frame with ACK flag has been received.
    bool settings_ack_received_ = false;
    // Number of SETTINGS frames received.
    int settings_frame_count_ = 0;
    // Accumulated setting parameters.
    spdy::SettingsMap settings_;
  };

  // Class to parse ACCEPT_CH frames.
  class AcceptChParser : public spdy::ExtensionVisitorInterface {
   public:
    AcceptChParser();
    ~AcceptChParser() override;

    const std::vector<spdy::AcceptChOriginValuePair>& GetAcceptCh() const {
      return accept_ch_;
    }

    // Returns an error code, or Error::kNoError if no error has occurred.
    Error error() const { return error_; }

    // Returns an error code if it was bypassed, or Error::kNoError if no error was bypassed.
    Error error_bypass() const { return error_bypass_; }

    // ExtensionVisitorInterface implementation.

    // Settings are parsed in a SpdyFramerVisitorInterface implementation,
    // because ExtensionVisitorInterface does not provide information about
    // receiving an empty SETTINGS frame.
    void OnSetting(spdy::SpdySettingsId id, uint32_t value) override {}

    bool OnFrameHeader(spdy::SpdyStreamId stream_id,
                       size_t length,
                       uint8_t type,
                       uint8_t flags) override;
    void OnFramePayload(const char* data, size_t len) override;

   private:
    // Accumulated ACCEPT_CH values.
    std::vector<spdy::AcceptChOriginValuePair> accept_ch_;

    Error error_ = Error::kNoError;
    Error error_bypass_ = Error::kNoError;
  };

  SettingsParser settings_parser_;
  AcceptChParser accept_ch_parser_;
  http2::Http2DecoderAdapter decoder_adapter_;
};

}  // namespace net

#endif  // NET_SPDY_ALPS_DECODER_H_
