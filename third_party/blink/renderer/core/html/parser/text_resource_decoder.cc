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

#include "third_party/blink/renderer/core/dom/dom_implementation.h"
#include "third_party/blink/renderer/core/html/parser/html_meta_charset_parser.h"
#include "third_party/blink/renderer/platform/text/text_encoding_detector.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

const int kMinimumLengthOfXMLDeclaration = 8;

static inline bool BytesEqual(const char* p,
                              char b0,
                              char b1,
                              char b2,
                              char b3,
                              char b4) {
  return p[0] == b0 && p[1] == b1 && p[2] == b2 && p[3] == b3 && p[4] == b4;
}

static inline bool BytesEqual(const char* p,
                              char b0,
                              char b1,
                              char b2,
                              char b3,
                              char b4,
                              char b5) {
  return p[0] == b0 && p[1] == b1 && p[2] == b2 && p[3] == b3 && p[4] == b4 &&
         p[5] == b5;
}

static inline bool BytesEqual(const char* p,
                              char b0,
                              char b1,
                              char b2,
                              char b3,
                              char b4,
                              char b5,
                              char b6,
                              char b7,
                              char b8,
                              char b9) {
  return p[0] == b0 && p[1] == b1 && p[2] == b2 && p[3] == b3 && p[4] == b4 &&
         p[5] == b5 && p[6] == b6 && p[7] == b7 && p[8] == b8 && p[9] == b9;
}

// You might think we should put these find functions elsewhere, perhaps with
// the similar functions that operate on UChar, but arguably only the decoder
// has a reason to process strings of char rather than UChar.

static int Find(const char* subject, int subject_length, const char* target) {
  size_t target_length = strlen(target);
  if (target_length > static_cast<size_t>(subject_length))
    return -1;
  for (size_t i = 0; i <= subject_length - target_length; ++i) {
    bool match = true;
    for (size_t j = 0; j < target_length; ++j) {
      if (subject[i + j] != target[j]) {
        match = false;
        break;
      }
    }
    if (match)
      return i;
  }
  return -1;
}

static WTF::TextEncoding FindTextEncoding(const char* encoding_name,
                                          wtf_size_t length) {
  Vector<char, 64> buffer(length + 1);
  memcpy(buffer.data(), encoding_name, length);
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

// Returns the position of the encoding string.
static int FindXMLEncoding(const char* str, int len, int& encoding_length) {
  int pos = Find(str, len, "encoding");
  if (pos == -1)
    return -1;
  pos += 8;

  // Skip spaces and stray control characters.
  while (pos < len && str[pos] <= ' ')
    ++pos;

  // Skip equals sign.
  if (pos >= len || str[pos] != '=')
    return -1;
  ++pos;

  // Skip spaces and stray control characters.
  while (pos < len && str[pos] <= ' ')
    ++pos;

  // Skip quotation mark.
  if (pos >= len)
    return -1;
  char quote_mark = str[pos];
  if (quote_mark != '"' && quote_mark != '\'')
    return -1;
  ++pos;

  // Find the trailing quotation mark.
  int end = pos;
  while (end < len && str[end] != quote_mark)
    ++end;
  if (end >= len)
    return -1;

  encoding_length = end - pos;
  return pos;
}

wtf_size_t TextResourceDecoder::CheckForBOM(const char* data, wtf_size_t len) {
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

  wtf_size_t length_of_bom = 0;
  const wtf_size_t max_bom_length = 3;

  wtf_size_t buffer_length = buffer_.size();

  wtf_size_t buf1_len = buffer_length;
  wtf_size_t buf2_len = len;
  const unsigned char* buf1 =
      reinterpret_cast<const unsigned char*>(buffer_.data());
  const unsigned char* buf2 = reinterpret_cast<const unsigned char*>(data);
  unsigned char c1 =
      buf1_len ? (--buf1_len, *buf1++) : buf2_len ? (--buf2_len, *buf2++) : 0;
  unsigned char c2 =
      buf1_len ? (--buf1_len, *buf1++) : buf2_len ? (--buf2_len, *buf2++) : 0;
  unsigned char c3 = buf1_len ? *buf1 : buf2_len ? *buf2 : 0;

  // Check for the BOM.
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

  if (length_of_bom || buffer_length + len >= max_bom_length)
    checked_for_bom_ = true;

  return length_of_bom;
}

bool TextResourceDecoder::CheckForCSSCharset(const char* data,
                                             wtf_size_t len,
                                             bool& moved_data_to_buffer) {
  if (source_ != kDefaultEncoding && source_ != kEncodingFromParentFrame) {
    checked_for_css_charset_ = true;
    return true;
  }

  wtf_size_t old_size = buffer_.size();
  buffer_.Grow(old_size + len);
  memcpy(buffer_.data() + old_size, data, len);

  moved_data_to_buffer = true;

  if (buffer_.size() <= 13)  // strlen('@charset "x";') == 13
    return false;

  const char* data_start = buffer_.data();
  const char* data_end = data_start + buffer_.size();

  if (BytesEqual(data_start, '@', 'c', 'h', 'a', 'r', 's', 'e', 't', ' ',
                 '"')) {
    data_start += 10;
    const char* pos = data_start;

    while (pos < data_end && *pos != '"')
      ++pos;
    if (pos == data_end)
      return false;

    wtf_size_t encoding_name_length = static_cast<wtf_size_t>(pos - data_start);

    ++pos;
    if (pos == data_end)
      return false;

    if (*pos == ';')
      SetEncoding(FindTextEncoding(data_start, encoding_name_length),
                  kEncodingFromCSSCharset);
  }

  checked_for_css_charset_ = true;
  return true;
}

bool TextResourceDecoder::CheckForXMLCharset(const char* data,
                                             wtf_size_t len,
                                             bool& moved_data_to_buffer) {
  if (source_ != kDefaultEncoding && source_ != kEncodingFromParentFrame) {
    checked_for_xml_charset_ = true;
    return true;
  }

  // This is not completely efficient, since the function might go
  // through the HTML head several times.

  wtf_size_t old_size = buffer_.size();
  buffer_.Grow(old_size + len);
  memcpy(buffer_.data() + old_size, data, len);

  moved_data_to_buffer = true;

  const char* ptr = buffer_.data();
  const char* p_end = ptr + buffer_.size();

  // Is there enough data available to check for XML declaration?
  if (buffer_.size() < kMinimumLengthOfXMLDeclaration)
    return false;

  // Handle XML declaration, which can have encoding in it. This encoding is
  // honored even for HTML documents. It is an error for an XML declaration not
  // to be at the start of an XML document, and it is ignored in HTML documents
  // in such case.
  if (BytesEqual(ptr, '<', '?', 'x', 'm', 'l')) {
    const char* xml_declaration_end = ptr;
    while (xml_declaration_end != p_end && *xml_declaration_end != '>')
      ++xml_declaration_end;
    if (xml_declaration_end == p_end)
      return false;
    // No need for +1, because we have an extra "?" to lose at the end of XML
    // declaration.
    int encoding_length = 0;
    int encoding_pos = FindXMLEncoding(
        ptr, static_cast<int>(xml_declaration_end - ptr), encoding_length);
    if (encoding_pos != -1) {
      SetEncoding(FindTextEncoding(ptr + encoding_pos, encoding_length),
                  kEncodingFromXMLHeader);
    }
    // continue looking for a charset - it may be specified in an HTTP-Equiv
    // meta
  } else if (BytesEqual(ptr, '<', 0, '?', 0, 'x', 0)) {
    SetEncoding(UTF16LittleEndianEncoding(), kAutoDetectedEncoding);
  } else if (BytesEqual(ptr, 0, '<', 0, '?', 0, 'x')) {
    SetEncoding(UTF16BigEndianEncoding(), kAutoDetectedEncoding);
  }

  checked_for_xml_charset_ = true;
  return true;
}

void TextResourceDecoder::CheckForMetaCharset(const char* data,
                                              wtf_size_t length) {
  if (source_ == kEncodingFromHTTPHeader || source_ == kAutoDetectedEncoding) {
    checked_for_meta_charset_ = true;
    return;
  }

  if (!charset_parser_)
    charset_parser_ = std::make_unique<HTMLMetaCharsetParser>();

  if (!charset_parser_->CheckForMetaCharset(data, length))
    return;

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
void TextResourceDecoder::AutoDetectEncodingIfAllowed(const char* data,
                                                      wtf_size_t len) {
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
  if (DetectTextEncoding(data, len, options_.HintEncoding(), options_.HintURL(),
                         options_.HintLanguage(), &detected_encoding))
    SetEncoding(detected_encoding, kEncodingFromContentSniffing);
  if (detected_encoding != WTF::UnknownEncoding())
    detection_completed_ = true;
}

String TextResourceDecoder::Decode(const char* data, size_t data_len) {
  wtf_size_t len = SafeCast<wtf_size_t>(data_len);
  wtf_size_t length_of_bom = 0;
  if (!checked_for_bom_) {
    length_of_bom = CheckForBOM(data, len);

    // BOM check can fail when the available data is not enough.
    if (!checked_for_bom_) {
      DCHECK_EQ(0u, length_of_bom);
      buffer_.Append(data, len);
      return g_empty_string;
    }
  }
  DCHECK_LE(length_of_bom, buffer_.size() + len);

  bool moved_data_to_buffer = false;

  if (options_.GetContentType() == TextResourceDecoderOptions::kCSSContent &&
      !checked_for_css_charset_) {
    if (!CheckForCSSCharset(data, len, moved_data_to_buffer))
      return g_empty_string;
  }

  // We check XML declaration in HTML content only if there is enough data
  // available
  if (((options_.GetContentType() == TextResourceDecoderOptions::kHTMLContent &&
        len >= kMinimumLengthOfXMLDeclaration) ||
       options_.GetContentType() == TextResourceDecoderOptions::kXMLContent) &&
      !checked_for_xml_charset_) {
    if (!CheckForXMLCharset(data, len, moved_data_to_buffer))
      return g_empty_string;
  }

  const char* data_for_decode = data + length_of_bom;
  wtf_size_t length_for_decode = len - length_of_bom;

  if (!buffer_.IsEmpty()) {
    if (!moved_data_to_buffer) {
      wtf_size_t old_size = buffer_.size();
      buffer_.Grow(old_size + len);
      memcpy(buffer_.data() + old_size, data, len);
    }

    data_for_decode = buffer_.data() + length_of_bom;
    length_for_decode = buffer_.size() - length_of_bom;
  }

  if (options_.GetContentType() == TextResourceDecoderOptions::kHTMLContent &&
      !checked_for_meta_charset_)
    CheckForMetaCharset(data_for_decode, length_for_decode);

  AutoDetectEncodingIfAllowed(data, len);

  DCHECK(encoding_.IsValid());

  if (!codec_)
    codec_ = NewTextCodec(encoding_);

  String result = codec_->Decode(
      data_for_decode, length_for_decode, WTF::FlushBehavior::kDoNotFlush,
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
    AutoDetectEncodingIfAllowed(buffer_.data(), buffer_.size());
  }

  if (!codec_)
    codec_ = NewTextCodec(encoding_);

  String result = codec_->Decode(
      buffer_.data(), buffer_.size(), WTF::FlushBehavior::kFetchEOF,
      options_.GetContentType() == TextResourceDecoderOptions::kXMLContent &&
          !options_.GetUseLenientXMLDecoding(),
      saw_error_);
  buffer_.clear();
  codec_.reset();
  checked_for_bom_ = false;  // Skip BOM again when re-decoding.
  return result;
}

}  // namespace blink
