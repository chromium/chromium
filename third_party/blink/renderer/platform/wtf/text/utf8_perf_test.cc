// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/features.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::unicode {

class Utf8PerfTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        base::features::kUtfConversionAsciiFastPath, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, Utf8PerfTest, testing::Bool());

TEST_P(Utf8PerfTest, ConvertUtf16ToUtf8_Ascii) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 'a' + (i % 26);
  }

  std::vector<uint8_t> dst(kSize * 3);  // Enough space for UTF-8

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, true);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_Ascii (10k, "
            << (GetParam() ? "Fast" : "Legacy") << "): " << time_per_op
            << " us/op";
}

TEST_P(Utf8PerfTest, ConvertUtf16ToUtf8_NonAscii) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 0x0800 + (i % 26);  // 3-byte UTF-8
  }

  std::vector<uint8_t> dst(kSize * 3);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, false);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_NonAscii (10k, "
            << (GetParam() ? "Fast" : "Legacy") << "): " << time_per_op
            << " us/op";
}

TEST_P(Utf8PerfTest, ConvertUtf16ToUtf8_Strict_NonAscii) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  Vector<UChar> src(kSize);
  for (int i = 0; i < kSize; ++i) {
    src[i] = 0x0800 + (i % 26);  // 3-byte UTF-8
  }

  std::vector<uint8_t> dst(kSize * 3);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    ConvertUtf16ToUtf8(src, dst, true);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "ConvertUtf16ToUtf8_Strict_NonAscii (10k, "
            << (GetParam() ? "Fast" : "Legacy") << "): " << time_per_op
            << " us/op";
}

TEST_P(Utf8PerfTest, Decode_Ascii) {
  const int kSize = 10000;
  const int kNumIterations = 100000;

  std::string src(kSize, 'a');
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    bool saw_error = false;
    codec->Decode(src_span, FlushBehavior::kDataEof, false, saw_error);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_Ascii (10k, " << (GetParam() ? "Fast" : "Legacy")
            << "): " << time_per_op << " us/op";
}

TEST_P(Utf8PerfTest, Decode_NonAscii) {
  const int kNumIterations = 20000;

  std::string src;
  while (src.length() < 10000) {
    src += "日本語の文章テキストデータです。漢字とひらがなとカタカナ。";
  }
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    bool saw_error = false;
    codec->Decode(src_span, FlushBehavior::kDataEof, false, saw_error);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_NonAscii (10k CJK, " << (GetParam() ? "Fast" : "Legacy")
            << "): " << time_per_op << " us/op";
}

TEST_P(Utf8PerfTest, Decode_MixedJson) {
  const int kNumIterations = 50000;

  std::string src = "{\"results\": [";
  while (src.length() < 10000) {
    src +=
        "{\"id\": 1024, \"title\": \"Google 検索\", \"desc\": \"Bonjour le "
        "monde!\"},";
  }
  src += "]}";
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    bool saw_error = false;
    codec->Decode(src_span, FlushBehavior::kDataEof, false, saw_error);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_MixedJson (10k, " << (GetParam() ? "Fast" : "Legacy")
            << "): " << time_per_op << " us/op";
}

TEST_P(Utf8PerfTest, Decode_MostlyAscii_32k) {
  const int kNumIterations = 20000;

  std::string src(32700, 'A');
  src += " // Copyright \xc2\xa9 2026 Caf\xc3\xa9 \xe6\xa4\x9c\xe7\xb4\xa2";
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    bool saw_error = false;
    codec->Decode(src_span, FlushBehavior::kDataEof, false, saw_error);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_MostlyAscii_32k (" << (GetParam() ? "Fast" : "Legacy")
            << "): " << time_per_op << " us/op";
}

TEST_P(Utf8PerfTest, Decode_MostlyAscii_64k) {
  const int kNumIterations = 10000;

  std::string src;
  src.reserve(65536);
  while (src.length() < 65400) {
    src +=
        "function computeLayout(node) { if (!node) return 0; return "
        "node.offsetWidth + 10; }\n";
  }
  src.resize(65400);
  src +=
      " // \xf0\x9f\x8c\x8d \xf0\x9f\x9a\x80 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa"
      "\x9e";
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");
  std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kNumIterations; ++i) {
    bool saw_error = false;
    codec->Decode(src_span, FlushBehavior::kDataEof, false, saw_error);
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_MostlyAscii_64k (" << (GetParam() ? "Fast" : "Legacy")
            << "): " << time_per_op << " us/op";
}

TEST_P(Utf8PerfTest, Decode_StreamingChunks_64k) {
  const int kNumIterations = 5000;
  const size_t kChunkSize = 4096;

  std::string src;
  src.reserve(65536);
  while (src.length() < 65536) {
    src +=
        "function handleEvent(e) { const target = e.target; if (!target) "
        "return; target.classList.add('active'); updateLayout(target); }\n";
  }
  src.resize(65536);
  auto src_span = base::as_bytes(base::span(src));

  TextEncoding encoding("UTF-8");

  base::TimeTicks start = base::TimeTicks::Now();
  for (int iter = 0; iter < kNumIterations; ++iter) {
    std::unique_ptr<TextCodec> codec(NewTextCodec(encoding));
    bool saw_error = false;
    for (size_t offset = 0; offset < src_span.size(); offset += kChunkSize) {
      size_t current_chunk_size =
          std::min(kChunkSize, src_span.size() - offset);
      auto chunk = src_span.subspan(offset, current_chunk_size);
      bool is_last = (offset + current_chunk_size >= src_span.size());
      FlushBehavior flush =
          is_last ? FlushBehavior::kDataEof : FlushBehavior::kDoNotFlush;
      codec->Decode(chunk, flush, false, saw_error);
    }
  }
  base::TimeTicks end = base::TimeTicks::Now();
  double time_per_op = (end - start).InMicrosecondsF() / kNumIterations;
  LOG(INFO) << "Decode_StreamingChunks_64k (chunk 4k, "
            << (GetParam() ? "Fast" : "Legacy") << "): " << time_per_op
            << " us/op";
}

}  // namespace blink::unicode
