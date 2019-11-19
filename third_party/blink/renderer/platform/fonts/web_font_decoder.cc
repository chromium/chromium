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

#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#include "third_party/ots/include/ots-memory-stream.h"
#include "third_party/skia/include/core/SkStream.h"

#include <hb.h>
#include <stdarg.h>

namespace blink {

namespace {

class BlinkOTSContext final : public ots::OTSContext {
  DISALLOW_NEW();

 public:
  void Message(int level, const char* format, ...) override;
  ots::TableAction GetTableAction(uint32_t tag) override;
  const String& GetErrorString() { return error_string_; }

 private:
  String error_string_;
};

void BlinkOTSContext::Message(int level, const char* format, ...) {
  va_list args;
  va_start(args, format);

#if defined(COMPILER_MSVC)
  int result = _vscprintf(format, args);
#else
  char ch;
  int result = vsnprintf(&ch, 1, format, args);
#endif
  va_end(args);

  if (result <= 0) {
    error_string_ = String("OTS Error");
  } else {
    Vector<char, 256> buffer;
    unsigned len = result;
    buffer.Grow(len + 1);

    va_start(args, format);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);
    error_string_ =
        StringImpl::Create(reinterpret_cast<const LChar*>(buffer.data()), len);
  }
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
#if HB_VERSION_ATLEAST(1, 0, 0)
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
#if HB_VERSION_ATLEAST(1, 0, 0)
    // Let HarfBuzz handle how to deal with broken tables.
    case kAvarTag:
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

void RecordDecodeSpeedHistogram(const char* data,
                                size_t length,
                                double decode_time,
                                size_t decoded_size) {
  if (decode_time <= 0)
    return;

  double kb_per_second = decoded_size / (1000 * decode_time);
  if (length >= 4) {
    if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F') {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, woff_histogram,
          ("WebFont.DecodeSpeed.WOFF", 1000, 300000, 50));
      woff_histogram.Count(kb_per_second);
      return;
    }

    if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2') {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, woff2_histogram,
          ("WebFont.DecodeSpeed.WOFF2", 1000, 300000, 50));
      woff2_histogram.Count(kb_per_second);
      return;
    }
  }

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, sfnt_histogram,
      ("WebFont.DecodeSpeed.SFNT", 1000, 300000, 50));
  sfnt_histogram.Count(kb_per_second);
}

}  // namespace

sk_sp<SkTypeface> WebFontDecoder::Decode(SharedBuffer* buffer) {
  if (!buffer) {
    SetErrorString("Empty Buffer");
    return nullptr;
  }

  // This is the largest web font size which we'll try to transcode.
  // TODO(bashi): 30MB seems low. Update the limit if necessary.
  static const size_t kMaxWebFontSize = 30 * 1024 * 1024;  // 30 MB
  if (buffer->size() > kMaxWebFontSize) {
    SetErrorString("Web font size more than 30MB");
    return nullptr;
  }

  // Most web fonts are compressed, so the result can be much larger than
  // the original.
  ots::ExpandingMemoryStream output(buffer->size(), kMaxWebFontSize);
  base::ElapsedTimer timer;
  BlinkOTSContext ots_context;
  SharedBuffer::DeprecatedFlatData flattened_buffer(buffer);
  const char* data = flattened_buffer.Data();

  TRACE_EVENT_BEGIN0("blink", "DecodeFont");
  bool ok = ots_context.Process(&output, reinterpret_cast<const uint8_t*>(data),
                                buffer->size());
  TRACE_EVENT_END0("blink", "DecodeFont");

  if (!ok) {
    SetErrorString(ots_context.GetErrorString());
    return nullptr;
  }

  const size_t decoded_length = SafeCast<size_t>(output.Tell());
  RecordDecodeSpeedHistogram(data, buffer->size(), timer.Elapsed().InSecondsF(),
                             decoded_length);

  sk_sp<SkData> sk_data = SkData::MakeWithCopy(output.get(), decoded_length);

  sk_sp<SkTypeface> new_typeface;

  if (!WebFontTypefaceFactory::CreateTypeface(sk_data, new_typeface)) {
    SetErrorString("Unable to instantiate font face from font data.");
    return nullptr;
  }

  decoded_size_ = decoded_length;

  return new_typeface;
}

}  // namespace blink
