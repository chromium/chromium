/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All
    rights reserved.
    Copyright (C) 2005, 2006, 2007 Alexey Proskuryakov (ap@nypop.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"

#include <string_view>

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/html/parser/html_meta_charset_parser.h"
#include "third_party/blink/renderer/platform/text/text_encoding_detector.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

const int kMinimumLengthOfXMLDeclaration = 8;

template <typename... Bytes>
static inline bool BytesEqual(base::span<const char> bytes,
                              Bytes... bytes_sequence) {
  constexpr size_t prefix_length = sizeof...(bytes_sequence);
  const std::array<char, prefix_length> prefix = {bytes_sequence...};
  return bytes.first<prefix_length>() == prefix;
}

static WTF::TextEncoding FindTextEncoding(std::string_view encoding_name) {
  const wtf_size_t length =
      base::checked_cast<wtf_size_t>(encoding_name.size());
  Vector<char, 64> buffer(length + 1);
  base::span(buffer).copy_prefix_from(encoding_name);
  buffer[length] = '\0';
  return WTF::TextEncoding(buffer.data());
}

const WTF::TextEncoding& TextResourceDecoder::DefaultEncoding(
    TextResourceDecoderOptions::ContentType content_type,
    const WTF::TextEncoding& specified_default_encoding) {
  // Despite 8.5 "Text/xml with Omitted Charset" of RFC 3023, we assume UTF-8
  // instead of US-ASCII for text/xml. This matches Firefox.
  if (content_type == TextResourceDecoderOptions::kXMLContent ||
      content_type == TextResourceDecoderOptions::kJSONContent)
    return UTF8Encoding();
  if (!specified_default_encoding.IsValid())
    return Latin1Encoding();
  return specified_default_encoding;
}

TextResourceDecoder::TextResourceDecoder(
    const TextResourceDecoderOptions& options)
    : options_(options),
      encoding_(DefaultEncoding(options_.GetContentType(),
                                options_.DefaultEncoding())),
      source_(kDefaultEncoding),
      checked_for_bom_(false),
      checked_for_css_charset_(false),
      checked_for_xml_charset_(false),
      checked_for_meta_charset_(false),
      saw_error_(false),
      detection_completed_(false) {
  // TODO(hiroshige): Move the invariant check to TextResourceDecoderOptions.
  if (options_.GetEncodingDetectionOption() ==
      TextResourceDecoderOptions::kAlwaysUseUTF8ForText) {
    DCHECK_EQ(options_.GetContentType(),
              TextResourceDecoderOptions::kPlainTextContent);
    DCHECK(encoding_ == UTF8Encoding());
  }
}

TextResourceDecoder::~TextResourceDecoder() = default;

void TextResourceDecoder::AddToBuffer(base::span<const char> data) {
  // Explicitly reserve capacity in the Vector to avoid triggering the growth
  // heuristic (== no excess capacity).
  buffer_.reserve(base::checked_cast<wtf_size_t>(buffer_.size() + data.size()));
  buffer_.AppendSpan(data);
}

void TextResourceDecoder::AddToBufferIfEmpty(base::span<const char> data) {
  if (buffer_.empty())
    buffer_.AppendSpan(data);
}

void TextResourceDecoder::SetEncoding(const WTF::TextEncoding& encoding,
                                      EncodingSource source) {
  // In case the encoding didn't exist, we keep the old one (helps some sites
  // specifying invalid encodings).
  if (!encoding.IsValid())
    return;

  // Always use UTF-8 for |kAlwaysUseUTF8ForText|.
  if (options_.GetEncodingDetectionOption() ==
      TextResourceDecoderOptions::kAlwaysUseUTF8ForText)
    return;

  // When encoding comes from meta tag (i.e. it cannot be XML files sent via
  // XHR), treat x-user-defined as windows-1252 (bug 18270)
  if (source == kEncodingFromMetaTag &&
      WTF::EqualIgnoringASCIICase(encoding.GetName(), "x-user-defined"))
    encoding_ = WTF::TextEncoding("windows-1252");
  else if (source == kEncodingFromMetaTag || source == kEncodingFromXMLHeader ||
           source == kEncodingFromCSSCharset)
    encoding_ = encoding.ClosestByteBasedEquivalent();
  else
    encoding_ = encoding;

  codec_.reset();
  source_ = source;
}

// Returns the substring containing the encoding string.
static std::string_view FindXMLEncoding(std::string_view str) {
  size_t pos = str.find("encoding");
  if (pos == std::string_view::npos) {
    return {};
  }
  pos += 8;

  // Skip spaces and stray control characters.
  while (pos < str.size() && str[pos] <= ' ') {
    ++pos;
  }

  // Skip equals sign.
  if (pos >= str.size() || str[pos] != '=') {
    return {};
  }
  ++pos;

  // Skip spaces and stray control characters.
  while (pos < str.size() && str[pos] <= ' ') {
    ++pos;
  }

  // Skip quotation mark.
  if (pos >= str.size()) {
    return {};
  }
  char quote_mark = str[pos];
  if (quote_mark != '"' && quote_mark != '\'')
    return {};
  ++pos;

  // Find the trailing quotation mark.
  size_t end = pos;
  while (end < str.size() && str[end] != quote_mark) {
    ++end;
  }
  if (end >= str.size()) {
    return {};
  }

  return str.substr(pos, end - pos);
}

wtf_size_t TextResourceDecoder::CheckForBOM(base::span<const char> data) {
  // Check for UTF-16 or UTF-8 BOM mark at the beginning, which is a sure
  // sign of a Unicode encoding. We let it override even a user-chosen encoding.

  // if |options_|'s value corresponds to #decode or #utf-8-decode,
  // CheckForBOM() corresponds to
  // - Steps 1-6 of https://encoding.spec.whatwg.org/#decode or
  // - Steps 1-3 of https://encoding.spec.whatwg.org/#utf-8-decode,
  // respectively.
  DCHECK(!checked_for_bom_);

  if (options_.GetNoBOMDecoding()) {
    checked_for_bom_ = true;
    return 0;
  }

  auto bytes = base::as_bytes(data);
  if (bytes.size() < 2) {
    return 0;
  }

  const uint8_t c1 = bytes[0];
  const uint8_t c2 = bytes[1];
  const uint8_t c3 = bytes.size() >= 3 ? bytes[2] : 0;

  // Check for the BOM.
  wtf_size_t length_of_bom = 0;
  if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
    SetEncoding(UTF8Encoding(), kAutoDetectedEncoding);
    length_of_bom = 3;
  } else if (options_.GetEncodingDetectionOption() !=
             TextResourceDecoderOptions::kAlwaysUseUTF8ForText) {
    if (c1 == 0xFE && c2 == 0xFF) {
      SetEncoding(UTF16BigEndianEncoding(), kAutoDetectedEncoding);
      length_of_bom = 2;
    } else if (c1 == 0xFF && c2 == 0xFE) {
      SetEncoding(UTF16LittleEndianEncoding(), kAutoDetectedEncoding);
      length_of_bom = 2;
    }
  }

  constexpr wtf_size_t kMaxBOMLength = 3;
  if (length_of_bom || bytes.size() >= kMaxBOMLength) {
    checked_for_bom_ = true;
  }

  return length_of_bom;
}

bool TextResourceDecoder::CheckForCSSCharset(base::span<const char> data) {
  if (source_ != kDefaultEncoding && source_ != kEncodingFromParentFrame) {
    checked_for_css_charset_ = true;
    return true;
  }

  if (data.size() <= 13) {  // strlen('@charset "x";') == 13
    return false;
  }

  if (BytesEqual(data, '@', 'c', 'h', 'a', 'r', 's', 'e', 't', ' ', '"')) {
    data = data.subspan(10u);

    auto it = base::ranges::find(data, '"');
    if (it == data.end()) {
      return false;
    }

    const size_t encoding_name_length = std::distance(data.begin(), it);

    ++it;
    if (it == data.end()) {
      return false;
    }
    if (*it == ';') {
      const auto encoding_name =
          base::as_string_view(data.first(encoding_name_length));
      SetEncoding(FindTextEncoding(encoding_name), kEncodingFromCSSCharset);
    }
  }

  checked_for_css_charset_ = true;
  return true;
}

bool TextResourceDecoder::CheckForXMLCharset(base::span<const char> data) {
  if (source_ != kDefaultEncoding && source_ != kEncodingFromParentFrame) {
    checked_for_xml_charset_ = true;
    return true;
  }

  // Is there enough data available to check for XML declaration?
  if (data.size() < kMinimumLengthOfXMLDeclaration) {
    return false;
  }

  // Handle XML declaration, which can have encoding in it. This encoding is
  // honored even for HTML documents. It is an error for an XML declaration not
  // to be at the start of an XML document, and it is ignored in HTML documents
  // in such case.
  if (BytesEqual(data, '<', '?', 'x', 'm', 'l')) {
    auto it = base::ranges::find(data, '>');
    if (it == data.end()) {
      return false;
    }
    const size_t search_length = std::distance(data.begin(), it);
    const std::string_view encoding_name =
        FindXMLEncoding(base::as_string_view(data.first(search_length)));
    if (!encoding_name.empty()) {
      SetEncoding(FindTextEncoding(encoding_name), kEncodingFromXMLHeader);
    }
    // continue looking for a charset - it may be specified in an HTTP-Equiv
    // meta
  } else if (BytesEqual(data, '<', '\0', '?', '\0', 'x', '\0')) {
    SetEncoding(UTF16LittleEndianEncoding(), kAutoDetectedEncoding);
  } else if (BytesEqual(data, '\0', '<', '\0', '?', '\0', 'x')) {
    SetEncoding(UTF16BigEndianEncoding(), kAutoDetectedEncoding);
  }

  checked_for_xml_charset_ = true;
  return true;
}

void TextResourceDecoder::CheckForMetaCharset(base::span<const char> data) {
  if (source_ == kEncodingFromHTTPHeader || source_ == kAutoDetectedEncoding) {
    checked_for_meta_charset_ = true;
    return;
  }

  if (!charset_parser_)
    charset_parser_ = std::make_unique<HTMLMetaCharsetParser>();

  if (!charset_parser_->CheckForMetaCharset(data)) {
    return;
  }

  SetEncoding(charset_parser_->Encoding(), kEncodingFromMetaTag);
  charset_parser_.reset();
  checked_for_meta_charset_ = true;
  return;
}

// We use the encoding detector in two cases:
//   1. Encoding detector is turned ON and no other encoding source is
//      available (that is, it's DefaultEncoding).
//   2. Encoding detector is turned ON and the encoding is set to
//      the encoding of the parent frame, which is also auto-detected.
//   Note that condition #2 is NOT satisfied unless parent-child frame
//   relationship is compliant to the same-origin policy. If they're from
//   different domains, |source_| would not be set to EncodingFromParentFrame
//   in the first place.
void TextResourceDecoder::AutoDetectEncodingIfAllowed(
    base::span<const char> data) {
  if (options_.GetEncodingDetectionOption() !=
          TextResourceDecoderOptions::kUseAllAutoDetection ||
      detection_completed_)
    return;

  // Just checking hint_encoding_ suffices here because it's only set
  // in SetHintEncoding when the source is AutoDetectedEncoding.
  if (!(source_ == kDefaultEncoding ||
        (source_ == kEncodingFromParentFrame && options_.HintEncoding())))
    return;

  WTF::TextEncoding detected_encoding;
  if (DetectTextEncoding(
          base::as_bytes(data), options_.HintEncoding().Utf8().c_str(),
          options_.HintURL(), options_.HintLanguage(), &detected_encoding)) {
    SetEncoding(detected_encoding, kEncodingFromContentSniffing);
  }
  if (detected_encoding != WTF::UnknownEncoding())
    detection_completed_ = true;
}

String TextResourceDecoder::Decode(base::span<const char> data) {
  TRACE_EVENT1("blink", "TextResourceDecoder::Decode", "data_len", data.size());
  // If we have previously buffered data, then add the new data to the buffer
  // and use the buffered content. Any case that depends on buffering (== return
  // the empty string) should call AddToBufferIfEmpty() if it needs more data to
  // make sure that the first data segment is buffered.
  if (!buffer_.empty()) {
    AddToBuffer(data);
    data = base::span(buffer_);
  }

  wtf_size_t length_of_bom = 0;
  if (!checked_for_bom_) {
    length_of_bom = CheckForBOM(data);

    // BOM check can fail when the available data is not enough.
    if (!checked_for_bom_) {
      DCHECK_EQ(0u, length_of_bom);
      AddToBufferIfEmpty(data);
      return g_empty_string;
    }
  }
  DCHECK_LE(length_of_bom, data.size());

  if (options_.GetContentType() == TextResourceDecoderOptions::kCSSContent &&
      !checked_for_css_charset_) {
    if (!CheckForCSSCharset(data)) {
      AddToBufferIfEmpty(data);
      return g_empty_string;
    }
  }

  if ((options_.GetContentType() == TextResourceDecoderOptions::kHTMLContent ||
       options_.GetContentType() == TextResourceDecoderOptions::kXMLContent) &&
      !checked_for_xml_charset_) {
    if (!CheckForXMLCharset(data)) {
      AddToBufferIfEmpty(data);
      return g_empty_string;
    }
  }

  auto data_for_decode = data.subspan(length_of_bom);

  if (options_.GetContentType() == TextResourceDecoderOptions::kHTMLContent &&
      !checked_for_meta_charset_)
    CheckForMetaCharset(data_for_decode);

  AutoDetectEncodingIfAllowed(data);

  DCHECK(encoding_.IsValid());

  if (!codec_)
    codec_ = NewTextCodec(encoding_);

  String result = codec_->Decode(
      base::as_bytes(data_for_decode), WTF::FlushBehavior::kDoNotFlush,
      options_.GetContentType() == TextResourceDecoderOptions::kXMLContent &&
          !options_.GetUseLenientXMLDecoding(),
      saw_error_);

  buffer_.clear();
  return result;
}

String TextResourceDecoder::Flush() {
  // If we can not identify the encoding even after a document is completely
  // loaded, we need to detect the encoding if other conditions for
  // autodetection is satisfied.
  if (buffer_.size() && ((!checked_for_xml_charset_ &&
                          (options_.GetContentType() ==
                               TextResourceDecoderOptions::kHTMLContent ||
                           options_.GetContentType() ==
                               TextResourceDecoderOptions::kXMLContent)) ||
                         (!checked_for_css_charset_ &&
                          (options_.GetContentType() ==
                           TextResourceDecoderOptions::kCSSContent)))) {
    AutoDetectEncodingIfAllowed(buffer_);
  }

  if (!codec_)
    codec_ = NewTextCodec(encoding_);

  String result = codec_->Decode(
      base::as_byte_span(buffer_), WTF::FlushBehavior::kFetchEOF,
      options_.GetContentType() == TextResourceDecoderOptions::kXMLContent &&
          !options_.GetUseLenientXMLDecoding(),
      saw_error_);
  buffer_.clear();
  codec_.reset();
  checked_for_bom_ = false;  // Skip BOM again when re-decoding.
  return result;
}

WebEncodingData TextResourceDecoder::GetEncodingData() const {
  return WebEncodingData{
      .encoding = encoding_.GetName(),
      .was_detected_heuristically = EncodingWasDetectedHeuristically(),
      .saw_decoding_error = SawError()};
}

}  // namespace blink
