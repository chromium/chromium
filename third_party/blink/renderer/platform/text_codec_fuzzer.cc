// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/fuzzed_data_provider.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

// TODO(jsbell): This fuzzes code in wtf/ but has dependencies on platform/,
// so it must live in the latter directory. Once wtf/ moves into platform/wtf
// this should move there as well.

WTF::FlushBehavior kFlushBehavior[] = {WTF::FlushBehavior::kDoNotFlush,
                                       WTF::FlushBehavior::kFetchEOF,
                                       WTF::FlushBehavior::kDataEOF};

WTF::UnencodableHandling kUnencodableHandlingOptions[] = {
    WTF::kEntitiesForUnencodables, WTF::kURLEncodedEntitiesForUnencodables,
    WTF::kCSSEncodedEntitiesForUnencodables};

class TextCodecFuzzHarness {};

// Fuzzer for WTF::TextCodec.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static blink::BlinkFuzzerTestSupport test_support;
  // The fuzzer picks 3 bytes off the end of the data to initialize metadata, so
  // abort if the input is smaller than that.
  if (size < 3)
    return 0;

  // TODO(csharrison): When crbug.com/701825 is resolved, add the rest of the
  // text codecs.

  // Initializes the codec map.
  static const WTF::TextEncoding encoding = WTF::TextEncoding(
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
  WTF::UnencodableHandling unencodable_handling =
      fuzzed_data.PickValueInArray(kUnencodableHandlingOptions);
  WTF::FlushBehavior flush_behavior =
      fuzzed_data.PickValueInArray(kFlushBehavior);

  // Now, use the rest of the fuzzy data to stress test decoding and encoding.
  const std::string byte_string = fuzzed_data.ConsumeRemainingBytes();
  std::unique_ptr<TextCodec> codec = NewTextCodec(encoding);

  // Treat as bytes-off-the-wire.
  bool saw_error;
  const String decoded =
      codec->Decode(byte_string.data(), byte_string.length(), flush_behavior,
                    stop_on_error, saw_error);

  // Treat as blink 8-bit string (latin1).
  if (size % sizeof(LChar) == 0) {
    std::unique_ptr<TextCodec> codec = NewTextCodec(encoding);
    codec->Encode(reinterpret_cast<const LChar*>(byte_string.data()),
                  byte_string.length() / sizeof(LChar), unencodable_handling);
  }

  // Treat as blink 16-bit string (utf-16) if there are an even number of bytes.
  if (size % sizeof(UChar) == 0) {
    std::unique_ptr<TextCodec> codec = NewTextCodec(encoding);
    codec->Encode(reinterpret_cast<const UChar*>(byte_string.data()),
                  byte_string.length() / sizeof(UChar), unencodable_handling);
  }

  if (decoded.IsNull())
    return 0;

  // Round trip the bytes (aka encode the decoded bytes).
  if (decoded.Is8Bit()) {
    codec->Encode(decoded.Characters8(), decoded.length(),
                  unencodable_handling);
  } else {
    codec->Encode(decoded.Characters16(), decoded.length(),
                  unencodable_handling);
  }
  return 0;
}
