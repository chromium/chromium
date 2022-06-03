#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <memory>
#include <string>

#include "net/server/web_socket_encoder.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  auto server = net::WebSocketEncoder::CreateServer();
  int bytes_consumed;
  std::string decoded;

  while (fuzzed_data_provider.remaining_bytes() > 0) {
    size_t chunk_size = fuzzed_data_provider.ConsumeIntegralInRange(1, 125);
    std::string chunk = fuzzed_data_provider.ConsumeBytesAsString(chunk_size);
    server->DecodeFrame(chunk, &bytes_consumed, &decoded);
  }
  return 0;
}