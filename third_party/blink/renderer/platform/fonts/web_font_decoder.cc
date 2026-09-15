/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"

#include <hb.h>
#include <stdarg.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/ift/ift_patcher.h"
#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/ots/src/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/woff2/include/woff2/decode.h"
#include "third_party/woff2/include/woff2/output.h"

namespace blink {

namespace {

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
const size_t kMaxDecompressedSizeMb = 30;
#else
const size_t kMaxDecompressedSizeMb = 128;
#endif

constexpr size_t kMaxDecompressedSize = kMaxDecompressedSizeMb * 1024 * 1024;

constexpr uint32_t kWoffTag = OTS_TAG('w', 'O', 'F', 'F');
constexpr uint32_t kWoff2Tag = OTS_TAG('w', 'O', 'F', '2');

class BlinkOTSContext final : public ots::OTSContext {
  DISALLOW_NEW();

 public:
  void Message(int level, const char* format, ...) override;
  ots::TableAction GetTableAction(uint32_t tag) override;
  String GetErrorString() { return accumulated_error_string_.ToString(); }

 private:
  void AppendErrorMessage(const String& new_error_string);

  StringBuilder accumulated_error_string_;
};

void BlinkOTSContext::Message(int level, const char* format, ...) {
  va_list args;
  va_start(args, format);

#if defined(COMPILER_MSVC)
  int result = _vscprintf(format, args);
#else
  char ch;
  int result = UNSAFE_TODO(vsnprintf(&ch, 1, format, args));
#endif
  va_end(args);

  if (result <= 0) {
    AppendErrorMessage(String("Unspecified OTS Error"));
  } else {
    Vector<char, 256> buffer;
    unsigned len = result;
    buffer.Grow(len + 1);

    va_start(args, format);
    UNSAFE_TODO(vsnprintf(buffer.data(), buffer.size(), format, args));
    va_end(args);

    AppendErrorMessage(
        String(StringImpl::Create(base::span(buffer).first(len))));
  }
}

void BlinkOTSContext::AppendErrorMessage(const String& new_error_string) {
  // OTS can emit a large number of warnings for malformed fonts. Keep enough
  // text for diagnostics, but avoid unbounded string growth. Once the
  // accumulated string reaches the budget, stop accepting further messages
  // entirely rather than truncating individual ones.
  static constexpr unsigned kMaxAccumulatedErrorStringLength = 4096;

  if (accumulated_error_string_.length() >= kMaxAccumulatedErrorStringLength) {
    return;
  }

  if (!accumulated_error_string_.empty()) {
    accumulated_error_string_.Append('\n');
  }
  accumulated_error_string_.Append(new_error_string);
}

#if !defined(HB_VERSION_ATLEAST)
#define HB_VERSION_ATLEAST(major, minor, micro) 0
#endif

ots::TableAction BlinkOTSContext::GetTableAction(uint32_t tag) {
  const uint32_t kCbdtTag = OTS_TAG('C', 'B', 'D', 'T');
  const uint32_t kCblcTag = OTS_TAG('C', 'B', 'L', 'C');
  const uint32_t kColrTag = OTS_TAG('C', 'O', 'L', 'R');
  const uint32_t kCpalTag = OTS_TAG('C', 'P', 'A', 'L');
  const uint32_t kCff2Tag = OTS_TAG('C', 'F', 'F', '2');
  const uint32_t kSbixTag = OTS_TAG('s', 'b', 'i', 'x');
  const uint32_t kStatTag = OTS_TAG('S', 'T', 'A', 'T');
#if HB_VERSION_ATLEAST(1, 0, 0)
  const uint32_t kBaseTag = OTS_TAG('B', 'A', 'S', 'E');
  const uint32_t kGdefTag = OTS_TAG('G', 'D', 'E', 'F');
  const uint32_t kGposTag = OTS_TAG('G', 'P', 'O', 'S');
  const uint32_t kGsubTag = OTS_TAG('G', 'S', 'U', 'B');

  // Font Variations related tables
  // See "Variation Tables" in Terminology section of
  // https://www.microsoft.com/typography/otspec/otvaroverview.htm
  const uint32_t kAvarTag = OTS_TAG('a', 'v', 'a', 'r');
  const uint32_t kCvarTag = OTS_TAG('c', 'v', 'a', 'r');
  const uint32_t kFvarTag = OTS_TAG('f', 'v', 'a', 'r');
  const uint32_t kGvarTag = OTS_TAG('g', 'v', 'a', 'r');
  const uint32_t kHvarTag = OTS_TAG('H', 'V', 'A', 'R');
  const uint32_t kMvarTag = OTS_TAG('M', 'V', 'A', 'R');
  const uint32_t kVvarTag = OTS_TAG('V', 'V', 'A', 'R');
#endif

  switch (tag) {
    // Google Color Emoji Tables
    case kCbdtTag:
    case kCblcTag:
    // Windows Color Emoji Tables
    case kColrTag:
    case kCpalTag:
    case kCff2Tag:
    case kSbixTag:
    case kStatTag:
#if HB_VERSION_ATLEAST(1, 0, 0)
    // Let HarfBuzz handle how to deal with broken tables.
    case kAvarTag:
    case kBaseTag:
    case kCvarTag:
    case kFvarTag:
    case kGvarTag:
    case kHvarTag:
    case kMvarTag:
    case kVvarTag:
    case kGdefTag:
    case kGposTag:
    case kGsubTag:
#endif
      return ots::TABLE_ACTION_PASSTHRU;
    default:
      return ots::TABLE_ACTION_DEFAULT;
  }
}

struct FontDecodeResult {
  std::unique_ptr<IftPatcher> ift_patcher;
};

// Decompresses and sanitizes `font_data` into an output stream using
// `ots_context`, and initializes `ift_patcher` if `font_data` is an IFT-enabled
// font. Returns whether decoding succeeded. On failure, the error string can be
// retrieved from `ots_context`.
bool DecodeFontWithIft(BlinkOTSContext& ots_context,
                       base::span<const uint8_t> font_data,
                       ots::ExpandingMemoryStream* output,
                       std::unique_ptr<IftPatcher>& ift_patcher) {
  TRACE_EVENT("blink", "DecodeFont");

  uint32_t magic = 0;
  if (font_data.size() >= 4) {
    magic = base::U32FromBigEndian(font_data.first<4u>());
  }

  switch (magic) {
    case kWoff2Tag: {
      size_t decompressed_size =
          woff2::ComputeWOFF2FinalSize(font_data.data(), font_data.size());
      if (decompressed_size == 0) {
        ots_context.Message(0, "Size of decompressed WOFF 2.0 is set to 0");
        return false;
      }
      if (decompressed_size < font_data.size()) {
        ots_context.Message(
            0, "Size of decompressed WOFF 2.0 is less than compressed size");
        return false;
      }
      if (decompressed_size > kMaxDecompressedSize) {
        ots_context.Message(0,
                            "Size of decompressed WOFF 2.0 font exceeds %gMB",
                            kMaxDecompressedSize / (1024.0 * 1024.0));
        return false;
      }

      std::string buf(decompressed_size, 0);
      woff2::WOFF2StringOut out(&buf);
      if (!woff2::ConvertWOFF2ToTTF(font_data.data(), font_data.size(), &out)) {
        ots_context.Message(0, "Failed to convert WOFF 2.0 font to SFNT");
        return false;
      }

      base::span<const uint8_t> decompressed_span =
          base::as_byte_span(buf).first(out.Size());
      if (!ots_context.Process(output, decompressed_span.data(),
                               decompressed_span.size())) {
        return false;
      }
      ift_patcher = IftPatcher::Create(decompressed_span);
      break;
    }
    case kWoffTag: {
      // The IFT (Incremental Font Transfer) specification recommends using
      // WOFF2. OTS handles WOFF decompression directly, so we do not extract
      // unsanitized font data or create an IftPatcher for the WOFF format.
      if (!ots_context.Process(output, font_data.data(), font_data.size())) {
        return false;
      }
      break;
    }
    default: {
      if (!ots_context.Process(output, font_data.data(), font_data.size())) {
        return false;
      }
      ift_patcher = IftPatcher::Create(font_data);
      break;
    }
  }

  return true;
}

}  // namespace

DecodedWebFont::DecodedWebFont() = default;
DecodedWebFont::DecodedWebFont(DecodedWebFont&&) noexcept = default;
DecodedWebFont& DecodedWebFont::operator=(DecodedWebFont&&) noexcept = default;
DecodedWebFont::~DecodedWebFont() = default;

base::expected<DecodedWebFont, String> DecodeWebFont(SegmentedBuffer* buffer) {
  if (!buffer) {
    return base::unexpected("Empty Buffer");
  }

  if (buffer->size() > kMaxDecompressedSize) {
    return base::unexpected(
        Format("Web font size more than {}MB", kMaxDecompressedSizeMb));
  }

  // Most web fonts are compressed, so the result can be much larger than
  // the original.
  std::unique_ptr<ots::ExpandingMemoryStream> output =
      std::make_unique<ots::ExpandingMemoryStream>(buffer->size(),
                                                   kMaxDecompressedSize);
  BlinkOTSContext ots_context;
  SegmentedBuffer::DeprecatedFlatData flattened_buffer(buffer);

  bool ok;
  std::unique_ptr<IftPatcher> ift_patcher;
  if (RuntimeEnabledFeatures::IncrementalFontTransferEnabled()) {
    ok = DecodeFontWithIft(ots_context, base::as_byte_span(flattened_buffer),
                           output.get(), ift_patcher);
  } else {
    TRACE_EVENT("blink", "DecodeFont");
    ok = ots_context.Process(
        output.get(), reinterpret_cast<const uint8_t*>(flattened_buffer.data()),
        buffer->size());
  }

  if (!ok) {
    return base::unexpected(ots_context.GetErrorString());
  }

  const void* decoded_data = output->get();
  DecodedWebFont result;
  result.ift_patcher = std::move(ift_patcher);
  result.decoded_size = base::checked_cast<size_t>(output->Tell());
  sk_sp<SkData> sk_data = SkData::MakeWithProc(
      decoded_data, result.decoded_size,
      [](const void*, void* output) {
        delete static_cast<const ots::ExpandingMemoryStream*>(output);
      },
      output.release());

  if (!WebFontTypefaceFactory::CreateTypeface(sk_data, result.sk_typeface)) {
    return base::unexpected("Unable to instantiate font face from font data.");
  }

  return result;
}

}  // namespace blink
