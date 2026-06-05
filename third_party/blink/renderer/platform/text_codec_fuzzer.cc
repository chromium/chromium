// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

// TODO(jsbell): This fuzzes code in wtf/ but has dependencies on platform/,
// so it must live in the latter directory. Once wtf/ moves into platform/wtf
// this should move there as well.

constexpr blink::FlushBehavior kFlushBehavior[] = {
    blink::FlushBehavior::kDoNotFlush, blink::FlushBehavior::kFetchEof,
    blink::FlushBehavior::kDataEof};

constexpr blink::UnencodableHandling kUnencodableHandlingOptions[] = {
    blink::UnencodableHandling::kXmlCharRef,
    blink::UnencodableHandling::kUrlEncodedCharRef,
    blink::UnencodableHandling::kCssEscape};

class TextCodecFuzzHarness {};

// Fuzzer for blink::TextCodec.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;
  blink::test::TaskEnvironment task_environment;
  // The fuzzer picks 3 bytes off the end of the data to initialize metadata, so
  // abort if the input is smaller than that.
  if (size < 3)
    return 0;

  // TODO(csharrison): When crbug.com/701825 is resolved, add the rest of the
  // text codecs.

  // Initializes the codec map.
  static const blink::TextEncoding encoding = blink::TextEncoding(
#if defined(UTF_8)
      "UTF-8"
#elif defined(WINDOWS_1252)
      "windows-1252"
#endif
      "");

  // Use the fully qualified name to avoid ambiguity with the standard class.
  blink::FuzzedDataProvider fuzzed_data(data, size);

  // Initialize metadata using the fuzzed data.
  bool stop_on_error = fuzzed_data.ConsumeBool();
  blink::UnencodableHandling unencodable_handling =
      fuzzed_data.PickValueInArray(kUnencodableHandlingOptions);
  blink::FlushBehavior flush_behavior =
      fuzzed_data.PickValueInArray(kFlushBehavior);

  // Now, use the rest of the fuzzy data to stress test decoding and encoding.
  const std::vector<uint8_t> bytes =
      fuzzed_data.ConsumeRemainingBytesAs<uint8_t>();
  base::span<const uint8_t> byte_span = base::as_byte_span(bytes);
  std::unique_ptr<blink::TextCodec> codec = NewTextCodec(encoding);

  // Treat as bytes-off-the-wire.
  bool saw_error;
  const blink::String decoded =
      codec->Decode(byte_span, flush_behavior, stop_on_error, saw_error);

  // Treat as blink 8-bit string (latin1).
  static_assert(sizeof(uint8_t) == sizeof(blink::LChar));
  std::unique_ptr<blink::TextCodec> lchar_codec = NewTextCodec(encoding);
  lchar_codec->Encode(byte_span, unencodable_handling);

  // Treat as blink 16-bit string (utf-16).
  //
  // Use a round number of bytes to mint a span of `UChar` (required
  // to call `reinterpret_span()`).
  const size_t round_number_of_bytes =
      (byte_span.size() / sizeof(UChar)) * sizeof(UChar);
  base::span<const UChar> uchars = base::subtle::reinterpret_span<const UChar>(
      byte_span.first(round_number_of_bytes));
  std::unique_ptr<blink::TextCodec> uchar_codec = NewTextCodec(encoding);
  uchar_codec->Encode(uchars, unencodable_handling);

  if (decoded.IsNull())
    return 0;

  // Round trip the bytes (aka encode the decoded bytes).
  if (decoded.Is8Bit()) {
    codec->Encode(decoded.Span8(), unencodable_handling);
  } else {
    codec->Encode(decoded.Span16(), unencodable_handling);
  }
  return 0;
}
