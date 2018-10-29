// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_DECODER_ADAPTER_H_
#define NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_DECODER_ADAPTER_H_

// HpackDecoderAdapter uses http2::HpackDecoder to decode HPACK blocks into
// HTTP/2 header lists as outlined in http://tools.ietf.org/html/rfc7541.

#include <stddef.h>

#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "net/third_party/http2/hpack/decoder/hpack_decoder.h"
#include "net/third_party/http2/hpack/decoder/hpack_decoder_listener.h"
#include "net/third_party/http2/hpack/decoder/hpack_decoder_tables.h"
#include "net/third_party/http2/hpack/hpack_string.h"
#include "net/third_party/http2/hpack/http2_hpack_constants.h"
#include "net/third_party/spdy/core/hpack/hpack_header_table.h"
#include "net/third_party/spdy/core/spdy_header_block.h"
#include "net/third_party/spdy/core/spdy_headers_handler_interface.h"
#include "net/third_party/spdy/platform/api/spdy_export.h"
#include "net/third_party/spdy/platform/api/spdy_string_piece.h"

namespace spdy {
namespace test {
class HpackDecoderAdapterPeer;
}  // namespace test

class SPDY_EXPORT_PRIVATE HpackDecoderAdapter {
 public:
  friend test::HpackDecoderAdapterPeer;
  HpackDecoderAdapter();
  HpackDecoderAdapter(const HpackDecoderAdapter&) = delete;
  HpackDecoderAdapter& operator=(const HpackDecoderAdapter&) = delete;
  ~HpackDecoderAdapter();

  // Called upon acknowledgement of SETTINGS_HEADER_TABLE_SIZE.
  void ApplyHeaderTableSizeSetting(size_t size_setting);

  // If a SpdyHeadersHandlerInterface is provided, the decoder will emit
  // headers to it rather than accumulating them in a SpdyHeaderBlock.
  // Does not take ownership of the handler, but does use the pointer until
  // the current HPACK block is completely decoded.
  void HandleControlFrameHeadersStart(SpdyHeadersHandlerInterface* handler);

  // Called as HPACK block fragments arrive. Returns false if an error occurred
  // while decoding the block. Does not take ownership of headers_data.
  bool HandleControlFrameHeadersData(const char* headers_data,
                                     size_t headers_data_length);

  // Called after a HPACK block has been completely delivered via
  // HandleControlFrameHeadersData(). Returns false if an error occurred.
  // |compressed_len| if non-null will be set to the size of the encoded
  // buffered block that was accumulated in HandleControlFrameHeadersData(),
  // to support subsequent calculation of compression percentage.
  // Discards the handler supplied at the start of decoding the block.
  // TODO(jamessynge): Determine if compressed_len is needed; it is used to
  // produce UUMA stat Net.SpdyHpackDecompressionPercentage, but only for
  // deprecated SPDY3.
  bool HandleControlFrameHeadersComplete(size_t* compressed_len);

  // Accessor for the most recently decoded headers block. Valid until the next
  // call to HandleControlFrameHeadersData().
  // TODO(birenroy): Remove this method when all users of HpackDecoder specify
  // a SpdyHeadersHandlerInterface.
  const SpdyHeaderBlock& decoded_block() const;

  void SetHeaderTableDebugVisitor(
      std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor);

  // Set how much encoded data this decoder is willing to buffer.
  // TODO(jamessynge): Resolve definition of this value, as it is currently
  // too tied to a single implementation. We probably want to limit one or more
  // of these: individual name or value strings, header entries, the entire
  // header list, or the HPACK block; we probably shouldn't care about the size
  // of individual transport buffers.
  void set_max_decode_buffer_size_bytes(size_t max_decode_buffer_size_bytes);

  size_t EstimateMemoryUsage() const;

 private:
  class SPDY_EXPORT_PRIVATE ListenerAdapter
      : public http2::HpackDecoderListener,
        public http2::HpackDecoderTablesDebugListener {
   public:
    ListenerAdapter();
    ~ListenerAdapter() override;

    // If a SpdyHeadersHandlerInterface is provided, the decoder will emit
    // headers to it rather than accumulating them in a SpdyHeaderBlock.
    // Does not take ownership of the handler, but does use the pointer until
    // the current HPACK block is completely decoded.
    void set_handler(SpdyHeadersHandlerInterface* handler);
    const SpdyHeaderBlock& decoded_block() const { return decoded_block_; }

    void SetHeaderTableDebugVisitor(
        std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor);

    // Override the HpackDecoderListener methods:
    void OnHeaderListStart() override;
    void OnHeader(http2::HpackEntryType entry_type,
                  const http2::HpackString& name,
                  const http2::HpackString& value) override;
    void OnHeaderListEnd() override;
    void OnHeaderErrorDetected(SpdyStringPiece error_message) override;

    // Override the HpackDecoderTablesDebugListener methods:
    int64_t OnEntryInserted(const http2::HpackStringPair& entry,
                            size_t insert_count) override;
    void OnUseEntry(const http2::HpackStringPair& entry,
                    size_t insert_count,
                    int64_t insert_time) override;

    void AddToTotalHpackBytes(size_t delta) { total_hpack_bytes_ += delta; }
    size_t total_hpack_bytes() const { return total_hpack_bytes_; }

   private:
    // If the caller doesn't provide a handler, the header list is stored in
    // this SpdyHeaderBlock.
    SpdyHeaderBlock decoded_block_;

    // If non-NULL, handles decoded headers. Not owned.
    SpdyHeadersHandlerInterface* handler_;

    // Total bytes that have been received as input (i.e. HPACK encoded)
    // in the current HPACK block.
    size_t total_hpack_bytes_;

    // Total bytes of the name and value strings in the current HPACK block.
    size_t total_uncompressed_bytes_;

    // visitor_ is used by a QUIC experiment regarding HPACK; remove
    // when the experiment is done.
    std::unique_ptr<HpackHeaderTable::DebugVisitorInterface> visitor_;
  };

  // Converts calls to HpackDecoderListener into calls to
  // SpdyHeadersHandlerInterface.
  ListenerAdapter listener_adapter_;

  // The actual decoder.
  http2::HpackDecoder hpack_decoder_;

  // How much encoded data this decoder is willing to buffer.
  size_t max_decode_buffer_size_bytes_;

  // Flag to keep track of having seen the header block start. Needed at the
  // moment because HandleControlFrameHeadersStart won't be called if a handler
  // is not being provided by the caller.
  bool header_block_started_;
};

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_HPACK_HPACK_DECODER_ADAPTER_H_
