// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_stream_parser.h"
#include "third_party/protobuf/src/google/protobuf/stubs/logging.h"

// Does initialization and holds state that's shared across all runs.
class Environment {
 public:
  Environment() {
    // Disable noisy logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

 private:
  google::protobuf::LogSilencer log_silencer_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMaxInputSize = 100 * 1000;

  static Environment env;

  if (size > kMaxInputSize) {
    // To avoid spurious timeout and out-of-memory fuzz reports.
    return 0;
  }
  FuzzedDataProvider provider(data, size);

  // Create a parser. `stream_closed_callback` destroys it, to satisfy the
  // implicit API that nothing should be passed after the stream close message.
  std::unique_ptr<remoting::ProtobufHttpStreamParser> parser;
  auto on_stream_closed = [&](const remoting::ProtobufHttpStatus&) {
    parser.reset();
  };
  parser = std::make_unique<remoting::ProtobufHttpStreamParser>(
      /*message_callback=*/base::DoNothing(),
      /*stream_closed_callback=*/base::BindLambdaForTesting(on_stream_closed));

  while (parser && provider.remaining_bytes() > 0) {
    parser->Append(provider.ConsumeRandomLengthString());
  }

  return 0;
}
