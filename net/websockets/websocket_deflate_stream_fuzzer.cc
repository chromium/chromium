// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_deflate_predictor.h"
#include "net/websockets/websocket_deflate_predictor_impl.h"
#include "net/websockets/websocket_deflate_stream.h"
#include "net/websockets/websocket_extension.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_stream.h"

namespace net {

namespace {

// If there are less random bytes left than MIN_BYTES_TO_CREATE_A_FRAME then
// CreateFrame() will always create an empty frame. Since the fuzzer can create
// the same empty frame with MIN_BYTES_TO_CREATE_A_FRAME bytes of input, save it
// from exploring a large space of ways to do the same thing.
constexpr size_t MIN_BYTES_TO_CREATE_A_FRAME = 3;

constexpr size_t BYTES_CONSUMED_BY_PARAMS = 2;

// If there are exactly BYTES_CONSUMED_BY_PARAMS + MIN_BYTES_TO_CREATE_A_FRAME
// bytes of input, then the fuzzer will test a single frame. In order to also
// test the case with zero frames, allow one less byte than this.
constexpr size_t MIN_USEFUL_SIZE =
    BYTES_CONSUMED_BY_PARAMS + MIN_BYTES_TO_CREATE_A_FRAME - 1;

class WebSocketFuzzedStream final : public WebSocketStream {
 public:
  explicit WebSocketFuzzedStream(FuzzedDataProvider* fuzzed_data_provider)
      : fuzzed_data_provider_(fuzzed_data_provider) {}

  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override {
    if (fuzzed_data_provider_->remaining_bytes() < MIN_BYTES_TO_CREATE_A_FRAME)
      return ERR_CONNECTION_CLOSED;
    while (fuzzed_data_provider_->remaining_bytes() > 0)
      frames->push_back(CreateFrame());
    return OK;
  }

  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override {
    return ERR_FILE_NOT_FOUND;
  }

  void Close() override {}
  std::string GetSubProtocol() const override { return std::string(); }
  std::string GetExtensions() const override { return std::string(); }

 private:
  std::unique_ptr<WebSocketFrame> CreateFrame() {
    WebSocketFrameHeader::OpCode opcode =
        fuzzed_data_provider_
            ->ConsumeIntegralInRange<WebSocketFrameHeader::OpCode>(
                WebSocketFrameHeader::kOpCodeContinuation,
                WebSocketFrameHeader::kOpCodeControlUnused);
    auto frame = std::make_unique<WebSocketFrame>(opcode);
    // Bad news: ConsumeBool actually consumes a whole byte per call, so do
    // something hacky to conserve precious bits.
    uint8_t flags = fuzzed_data_provider_->ConsumeIntegral<uint8_t>();
    frame->header.final = flags & 0x1;
    frame->header.reserved1 = (flags >> 1) & 0x1;
    frame->header.reserved2 = (flags >> 2) & 0x1;
    frame->header.reserved3 = (flags >> 3) & 0x1;
    frame->header.masked = (flags >> 4) & 0x1;
    uint64_t payload_length =
        fuzzed_data_provider_->ConsumeIntegralInRange(0, 64);
    std::vector<char> payload =
        fuzzed_data_provider_->ConsumeBytes<char>(payload_length);
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(payload.size());
    memcpy(buffer->data(), payload.data(), payload.size());
    buffers_.push_back(buffer);
    frame->payload = buffer->data();
    frame->header.payload_length = payload.size();
    return frame;
  }

  std::vector<scoped_refptr<IOBufferWithSize>> buffers_;

  FuzzedDataProvider* fuzzed_data_provider_;
};

void WebSocketDeflateStreamFuzz(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  uint8_t flags = fuzzed_data_provider.ConsumeIntegral<uint8_t>();
  bool server_no_context_takeover = flags & 0x1;
  bool client_no_context_takeover = (flags >> 1) & 0x1;
  uint8_t window_bits = fuzzed_data_provider.ConsumeIntegral<uint8_t>();
  int server_max_window_bits = (window_bits & 0x7) + 8;
  int client_max_window_bits = ((window_bits >> 3) & 0x7) + 8;
  // WebSocketDeflateStream needs to be constructed on each call because it
  // has state.
  WebSocketExtension params("permessage-deflate");
  if (server_no_context_takeover)
    params.Add(WebSocketExtension::Parameter("server_no_context_takeover"));
  if (client_no_context_takeover)
    params.Add(WebSocketExtension::Parameter("client_no_context_takeover"));
  params.Add(WebSocketExtension::Parameter(
      "server_max_window_bits", base::NumberToString(server_max_window_bits)));
  params.Add(WebSocketExtension::Parameter(
      "client_max_window_bits", base::NumberToString(client_max_window_bits)));
  std::string failure_message;
  WebSocketDeflateParameters parameters;
  DCHECK(parameters.Initialize(params, &failure_message)) << failure_message;
  WebSocketDeflateStream deflate_stream(
      std::make_unique<WebSocketFuzzedStream>(&fuzzed_data_provider),
      parameters, std::make_unique<WebSocketDeflatePredictorImpl>());
  std::vector<std::unique_ptr<net::WebSocketFrame>> frames;
  deflate_stream.ReadFrames(&frames, CompletionOnceCallback());
}

}  // namespace

}  // namespace net

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < net::MIN_USEFUL_SIZE)
    return 0;
  net::WebSocketDeflateStreamFuzz(data, size);

  return 0;
}
