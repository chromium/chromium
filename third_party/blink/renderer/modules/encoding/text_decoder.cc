/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/encoding/text_decoder.h"

#include <array>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/encoding/encoding.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf8.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

BASE_FEATURE(kTextDecoderFastPath, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr size_t kFastPathMaxBytes = 128;

// Decodes a complete (non-streaming) short UTF-8 span directly into a V8
// string, avoiding TextCodecUtf8 instance allocation and WTF::StringImpl heap
// allocation. Caller must ensure `!do_not_flush_ && is_utf8_ && !stream`.
// Returns an empty Local if `input` exceeds `kFastPathMaxBytes` and is not
// pure ASCII (and did not throw).
v8::MaybeLocal<v8::String> TryDecodeShortUtf8(v8::Isolate* isolate,
                                              base::span<const uint8_t> input,
                                              bool fatal,
                                              bool ignore_bom,
                                              bool& bom_seen,
                                              ExceptionState& exception_state,
                                              bool& handled) {
  if (input.empty()) {
    bom_seen = false;
    handled = true;
    return v8::String::Empty(isolate);
  }
  if (base::IsStringASCII(base::as_string_view(base::as_chars(input)))) {
    bom_seen = true;
    handled = true;
    return v8::String::NewFromOneByte(isolate, input.data(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(input.size()));
  }
  if (input.size() > kFastPathMaxBytes) {
    handled = false;
    return {};
  }

  handled = true;
  base::span<const uint8_t> bytes = input;
  if (!ignore_bom && bytes.size() >= 3 && bytes[0] == 0xEF &&
      bytes[1] == 0xBB && bytes[2] == 0xBF) {
    bytes = bytes.subspan(3u);
  }
  bom_seen = !bytes.empty();
  if (bytes.empty()) {
    return v8::String::Empty(isolate);
  }

  std::array<LChar, kFastPathMaxBytes> buffer8;
  std::array<UChar, kFastPathMaxBytes> buffer16;
  TextCodecUtf8::DecodedBufferResult result =
      TextCodecUtf8::DecodeToBuffer(bytes, buffer8, buffer16, fatal);
  if (fatal && result.saw_error) {
    exception_state.ThrowTypeError("The encoded data was not valid.");
    return {};
  }
  if (result.length == 0) {
    return v8::String::Empty(isolate);
  }
  if (result.is_8bit) {
    return v8::String::NewFromOneByte(isolate, buffer8.data(),
                                      v8::NewStringType::kNormal,
                                      static_cast<int>(result.length));
  }
  return v8::String::NewFromTwoByte(
      isolate, reinterpret_cast<const uint16_t*>(buffer16.data()),
      v8::NewStringType::kNormal, static_cast<int>(result.length));
}

}  // namespace

TextDecoder* TextDecoder::Create(const String& label,
                                 const TextDecoderOptions* options,
                                 ExceptionState& exception_state) {
  TextEncoding encoding(label.StripWhiteSpace(&encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() ||
      EqualIgnoringAsciiCase(encoding.GetName(), "replacement")) {
    exception_state.ThrowRangeError(
        StrCat({"The encoding label provided ('", label, "') is invalid."}));
    return nullptr;
  }

  return MakeGarbageCollected<TextDecoder>(encoding, options->fatal(),
                                           options->ignoreBOM());
}

TextDecoder::TextDecoder(const TextEncoding& encoding,
                         bool fatal,
                         bool ignore_bom)
    : encoding_(encoding),
      is_utf8_(encoding == Utf8Encoding()),
      fatal_(fatal),
      ignore_bom_(ignore_bom),
      bom_seen_(false) {}

TextDecoder::~TextDecoder() = default;

String TextDecoder::encoding() const {
  String name = encoding_.GetName().GetString().ToAsciiLower();
  // Where possible, encoding aliases should be handled by changes to Chromium's
  // ICU or Blink's WTF.  The same codec is used, but WTF maintains a different
  // name/identity for these.
  if (name == "iso-8859-1" || name == "us-ascii")
    return "windows-1252";
  return name;
}

v8::MaybeLocal<v8::String> TextDecoder::decode(
    v8::Isolate* isolate,
    std::optional<base::span<const uint8_t>> input,
    const TextDecodeOptions* options,
    ExceptionState& exception_state) {
  DCHECK(options);
  base::span<const uint8_t> input_span =
      input.value_or(base::span<const uint8_t>());
  // Account for up to 3 bytes of partial sequence in TextCodecUtf8 so that
  // a 16-bit StringImpl allocation never exceeds
  // partition_alloc::MaxAllocationSize().
  if (input_span.size() > kStringMaxUCharLength - 3) [[unlikely]] {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return {};
  }

  return Decode(isolate, input_span, options->stream(), exception_state);
}

v8::MaybeLocal<v8::String> TextDecoder::decode(
    v8::Isolate* isolate,
    std::optional<base::span<const uint8_t>> input,
    ExceptionState& exception_state) {
  base::span<const uint8_t> input_span =
      input.value_or(base::span<const uint8_t>());
  if (input_span.size() > kStringMaxUCharLength - 3) [[unlikely]] {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return {};
  }

  return Decode(isolate, input_span, /*stream=*/false, exception_state);
}

v8::MaybeLocal<v8::String> TextDecoder::Decode(
    v8::Isolate* isolate,
    base::span<const uint8_t> input,
    bool stream,
    ExceptionState& exception_state) {
  const bool fast_path_enabled =
      base::FeatureList::IsEnabled(kTextDecoderFastPath);
  if (!do_not_flush_) {
    if (fast_path_enabled && is_utf8_ && !stream) {
      bool handled = false;
      v8::MaybeLocal<v8::String> fast_result =
          TryDecodeShortUtf8(isolate, input, fatal_, ignore_bom_, bom_seen_,
                             exception_state, handled);
      if (handled) {
        return fast_result;
      }
    }
    if (!codec_) {
      // Reuse the codec after a full flush instead of allocating a new one
      // for each session. Fatal errors while flushing discard the codec below.
      // https://encoding.spec.whatwg.org/#dom-textdecoder-decode
      codec_ = NewTextCodec(encoding_);
      CHECK(codec_) << encoding_.GetName();
    }
    bom_seen_ = false;
  }

  DCHECK(codec_);
  do_not_flush_ = stream;
  FlushBehavior flush =
      do_not_flush_ ? FlushBehavior::kDoNotFlush : FlushBehavior::kDataEof;

  bool saw_error = false;
  String s = codec_->Decode(input, flush, fatal_, saw_error);

  if (fatal_ && saw_error) {
    if (!do_not_flush_) {
      // If flushing, the error should not persist.
      codec_.reset();
    }
    exception_state.ThrowTypeError("The encoded data was not valid.");
    return {};
  }

  if (!ignore_bom_ && !bom_seen_ && !s.empty()) {
    bom_seen_ = true;
    if (s[0] == 0xFEFF) {
      if (is_utf8_) {
        s.erase(0, 1);
      } else {
        const AtomicString& name = encoding_.GetName();
        if (name == "UTF-16LE" || name == "UTF-16BE") {
          s.erase(0, 1);
        }
      }
    }
  }

  if (fast_path_enabled) {
    if (s.empty()) {
      return v8::String::Empty(isolate);
    }
    constexpr wtf_size_t kDirectV8StringThreshold = 128;
    if (s.length() <= kDirectV8StringThreshold) {
      if (s.Is8Bit()) {
        return v8::String::NewFromOneByte(isolate, s.Span8().data(),
                                          v8::NewStringType::kNormal,
                                          s.length());
      }
      return v8::String::NewFromTwoByte(
          isolate, reinterpret_cast<const uint16_t*>(s.Span16().data()),
          v8::NewStringType::kNormal, s.length());
    }
  }
  return V8String(isolate, s);
}

v8::MaybeLocal<v8::String> TextDecoder::decode(
    v8::Isolate* isolate,
    ExceptionState& exception_state) {
  return Decode(isolate, {}, /*stream=*/false, exception_state);
}

}  // namespace blink
