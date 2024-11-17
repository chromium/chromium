// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_content_disposition.h"

#include <string_view>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/strings/escape.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_string_util.h"
#include "net/http/http_util.h"

namespace net {

namespace {

enum RFC2047EncodingType {
  Q_ENCODING,
  B_ENCODING
};

// Decodes a "Q" encoded string as described in RFC 2047 section 4.2. Similar to
// decoding a quoted-printable string.  Returns true if the input was valid.
bool DecodeQEncoding(std::string_view input, std::string* output) {
  std::string temp;
  temp.reserve(input.size());
  for (auto it = input.begin(); it != input.end(); ++it) {
    if (*it == '_') {
      temp.push_back(' ');
    } else if (*it == '=') {
      if ((input.end() - it < 3) ||
          !base::IsHexDigit(static_cast<unsigned char>(*(it + 1))) ||
          !base::IsHexDigit(static_cast<unsigned char>(*(it + 2))))
        return false;
      unsigned char ch =
          base::HexDigitToInt(*(it + 1)) * 16 + base::HexDigitToInt(*(it + 2));
      temp.push_back(static_cast<char>(ch));
      ++it;
      ++it;
    } else if (0x20 < *it && *it < 0x7F && *it != '?') {
      // In a Q-encoded word, only printable ASCII characters
      // represent themselves. Besides, space, '=', '_' and '?' are
      // not allowed, but they're already filtered out.
      DCHECK_NE('=', *it);
      DCHECK_NE('?', *it);
      DCHECK_NE('_', *it);
      temp.push_back(*it);
    } else {
      return false;
    }
  }
  output->swap(temp);
  return true;
}

// Decodes a "Q" or "B" encoded string as per RFC 2047 section 4. The encoding
// type is specified in |enc_type|.
bool DecodeBQEncoding(std::string_view part,
                      RFC2047EncodingType enc_type,
                      const std::string& charset,
                      std::string* output) {
  std::string decoded;
  if (!((enc_type == B_ENCODING) ?
        base::Base64Decode(part, &decoded) : DecodeQEncoding(part, &decoded))) {
    return false;
  }

  if (decoded.empty()) {
    output->clear();
    return true;
  }

  return ConvertToUtf8(decoded, charset.c_str(), output);
}

bool DecodeWord(std::string_view encoded_word,
                const std::string& referrer_charset,
                bool* is_rfc2047,
                std::string* output,
                int* parse_result_flags) {
  *is_rfc2047 = false;
  output->clear();
  if (encoded_word.empty())
    return true;

  if (!base::IsStringASCII(encoded_word)) {
    // Try UTF-8, referrer_charset and the native OS default charset in turn.
    if (base::IsStringUTF8(encoded_word)) {
      *output = std::string(encoded_word);
    } else {
      std::u16string utf16_output;
      if (!referrer_charset.empty() &&
          ConvertToUTF16(encoded_word, referrer_charset.c_str(),
                         &utf16_output)) {
        *output = base::UTF16ToUTF8(utf16_output);
      } else {
        *output = base::WideToUTF8(base::SysNativeMBToWide(encoded_word));
      }
    }

    *parse_result_flags |= HttpContentDisposition::HAS_NON_ASCII_STRINGS;
    return true;
  }

  // RFC 2047 : one of encoding methods supported by Firefox and relatively
  // widely used by web servers.
  // =?charset?<E>?<encoded string>?= where '<E>' is either 'B' or 'Q'.
  // We don't care about the length restriction (72 bytes) because
  // many web servers generate encoded words longer than the limit.
  std::string decoded_word;
  *is_rfc2047 = true;
  int part_index = 0;
  std::string charset;
  base::CStringTokenizer t(encoded_word.data(),
                           encoded_word.data() + encoded_word.size(), "?");
  RFC2047EncodingType enc_type = Q_ENCODING;
  while (*is_rfc2047 && t.GetNext()) {
    std::string_view part = t.token_piece();
    switch (part_index) {
      case 0:
        if (part != "=") {
          *is_rfc2047 = false;
          break;
        }
        ++part_index;
        break;
      case 1:
        // Do we need charset validity check here?
        charset = std::string(part);
        ++part_index;
        break;
      case 2:
        if (part.size() > 1 ||
            part.find_first_of("bBqQ") == std::string::npos) {
          *is_rfc2047 = false;
          break;
        }
        if (part[0] == 'b' || part[0] == 'B') {
          enc_type = B_ENCODING;
        }
        ++part_index;
        break;
      case 3:
        *is_rfc2047 = DecodeBQEncoding(part, enc_type, charset, &decoded_word);
        if (!*is_rfc2047) {
          // Last minute failure. Invalid B/Q encoding. Rather than
          // passing it through, return now.
          return false;
        }
        ++part_index;
        break;
      case 4:
        if (part != "=") {
          // Another last minute failure !
          // Likely to be a case of two encoded-words in a row or
          // an encoded word followed by a non-encoded word. We can be
          // generous, but it does not help much in terms of compatibility,
          // I believe. Return immediately.
          *is_rfc2047 = false;
          return false;
        }
        ++part_index;
        break;
      default:
        *is_rfc2047 = false;
        return false;
    }
  }

  if (*is_rfc2047) {
    if (*(encoded_word.end() - 1) == '=') {
      output->swap(decoded_word);
      *parse_result_flags |=
          HttpContentDisposition::HAS_RFC2047_ENCODED_STRINGS;
      return true;
    }
    // encoded_word ending prematurelly with '?' or extra '?'
    *is_rfc2047 = false;
    return false;
  }

  // We're not handling 'especial' characters quoted with '\', but
  // it should be Ok because we're not an email client but a
  // web browser.

  // What IE6/7 does: %-escaped UTF-8.
  decoded_word = base::UnescapeBinaryURLComponent(encoded_word,
                                                  base::UnescapeRule::NORMAL);
  if (decoded_word != encoded_word)
    *parse_result_flags |= HttpContentDisposition::HAS_PERCENT_ENCODED_STRINGS;
  if (base::IsStringUTF8(decoded_word)) {
    output->swap(decoded_word);
    return true;
    // We can try either the OS default charset or 'origin charset' here,
    // As far as I can tell, IE does not support it. However, I've seen
    // web servers emit %-escaped string in a legacy encoding (usually
    // origin charset).
    // TODO(jungshik) : Test IE further and consider adding a fallback here.
  }
  return false;
}

// Decodes the value of a 'filename' or 'name' parameter given as |input|. The
// value is supposed to be of the form:
//
//   value                   = token | quoted-string
//
// However we currently also allow RFC 2047 encoding and non-ASCII
// strings. Non-ASCII strings are interpreted based on |referrer_charset|.
bool DecodeFilenameValue(std::string_view input,
                         const std::string& referrer_charset,
                         std::string* output,
                         int* parse_result_flags) {
  int current_parse_result_flags = 0;
  std::string decoded_value;
  bool is_previous_token_rfc2047 = true;

  // Tokenize with whitespace characters.
  base::StringViewTokenizer t(input, " \t\n\r");
  t.set_options(base::StringViewTokenizer::RETURN_DELIMS);
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      // If the previous non-delimeter token is not RFC2047-encoded,
      // put in a space in its place. Otheriwse, skip over it.
      if (!is_previous_token_rfc2047)
        decoded_value.push_back(' ');
      continue;
    }
    // We don't support a single multibyte character split into
    // adjacent encoded words. Some broken mail clients emit headers
    // with that problem, but most web servers usually encode a filename
    // in a single encoded-word. Firefox/Thunderbird do not support
    // it, either.
    std::string decoded;
    if (!DecodeWord(t.token_piece(), referrer_charset,
                    &is_previous_token_rfc2047, &decoded,
                    &current_parse_result_flags))
      return false;
    decoded_value.append(decoded);
  }
  output->swap(decoded_value);
  if (parse_result_flags && !output->empty())
    *parse_result_flags |= current_parse_result_flags;
  return true;
}

// Parses the charset and value-chars out of an ext-value string.
//
//  ext-value     = charset  "'" [ language ] "'" value-chars
bool ParseExtValueComponents(std::string_view input,
                             std::string* charset,
                             std::string* value_chars) {
  base::StringViewTokenizer t(input, "'");
  t.set_options(base::StringTokenizer::RETURN_DELIMS);
  std::string_view temp_charset;
  std::string_view temp_value;
  int num_delims_seen = 0;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      ++num_delims_seen;
      continue;
    } else {
      switch (num_delims_seen) {
        case 0:
          temp_charset = t.token_piece();
          break;
        case 1:
          // Language is ignored.
          break;
        case 2:
          temp_value = t.token_piece();
          break;
        default:
          return false;
      }
    }
  }
  if (num_delims_seen != 2)
    return false;
  if (temp_charset.empty() || temp_value.empty())
    return false;
  *charset = std::string(temp_charset);
  *value_chars = std::string(temp_value);
  return true;
}

// http://tools.ietf.org/html/rfc5987#section-3.2
//
//  ext-value     = charset  "'" [ language ] "'" value-chars
//
//  charset       = "UTF-8" / "ISO-8859-1" / mime-charset
//
//  mime-charset  = 1*mime-charsetc
//  mime-charsetc = ALPHA / DIGIT
//                 / "!" / "#" / "$" / "%" / "&"
//                 / "+" / "-" / "^" / "_" / "`"
//                 / "{" / "}" / "~"
//
//  language      = <Language-Tag, defined in [RFC5646], Section 2.1>
//
//  value-chars   = *( pct-encoded / attr-char )
//
//  pct-encoded   = "%" HEXDIG HEXDIG
//
//  attr-char     = ALPHA / DIGIT
//                 / "!" / "#" / "$" / "&" / "+" / "-" / "."
//                 / "^" / "_" / "`" / "|" / "~"
bool DecodeExtValue(std::string_view param_value, std::string* decoded) {
  if (param_value.find('"') != std::string::npos)
    return false;

  std::string charset;
  std::string value;
  if (!ParseExtValueComponents(param_value, &charset, &value))
    return false;

  // RFC 5987 value should be ASCII-only.
  if (!base::IsStringASCII(value)) {
    decoded->clear();
    return true;
  }

  std::string unescaped =
      base::UnescapeBinaryURLComponent(value, base::UnescapeRule::NORMAL);

  return ConvertToUtf8AndNormalize(unescaped, charset.c_str(), decoded);
}

} // namespace

HttpContentDisposition::HttpContentDisposition(
    const std::string& header,
    const std::string& referrer_charset) {
  Parse(header, referrer_charset);
}

HttpContentDisposition::~HttpContentDisposition() = default;

std::string::const_iterator HttpContentDisposition::ConsumeDispositionType(
    std::string::const_iterator begin, std::string::const_iterator end) {
  DCHECK(type_ == INLINE);
  auto header = base::MakeStringPiece(begin, end);
  size_t delimiter = header.find(';');
  std::string_view type = header.substr(0, delimiter);
  type = HttpUtil::TrimLWS(type);

  // If the disposition-type isn't a valid token the then the
  // Content-Disposition header is malformed, and we treat the first bytes as
  // a parameter rather than a disposition-type.
  if (type.empty() || !HttpUtil::IsToken(type))
    return begin;

  parse_result_flags_ |= HAS_DISPOSITION_TYPE;

  DCHECK(type.find('=') == std::string_view::npos);

  if (base::EqualsCaseInsensitiveASCII(type, "inline")) {
    type_ = INLINE;
  } else if (base::EqualsCaseInsensitiveASCII(type, "attachment")) {
    type_ = ATTACHMENT;
  } else {
    parse_result_flags_ |= HAS_UNKNOWN_DISPOSITION_TYPE;
    type_ = ATTACHMENT;
  }
  return begin + (type.data() + type.size() - header.data());
}

// http://tools.ietf.org/html/rfc6266
//
//  content-disposition = "Content-Disposition" ":"
//                         disposition-type *( ";" disposition-parm )
//
//  disposition-type    = "inline" | "attachment" | disp-ext-type
//                      ; case-insensitive
//  disp-ext-type       = token
//
//  disposition-parm    = filename-parm | disp-ext-parm
//
//  filename-parm       = "filename" "=" value
//                      | "filename*" "=" ext-value
//
//  disp-ext-parm       = token "=" value
//                      | ext-token "=" ext-value
//  ext-token           = <the characters in token, followed by "*">
//
void HttpContentDisposition::Parse(const std::string& header,
                                   const std::string& referrer_charset) {
  DCHECK(type_ == INLINE);
  DCHECK(filename_.empty());

  std::string::const_iterator pos = header.begin();
  std::string::const_iterator end = header.end();
  pos = ConsumeDispositionType(pos, end);

  std::string filename;
  std::string ext_filename;

  HttpUtil::NameValuePairsIterator iter(base::MakeStringPiece(pos, end), ';');
  while (iter.GetNext()) {
    if (filename.empty() &&
        base::EqualsCaseInsensitiveASCII(iter.name(), "filename")) {
      DecodeFilenameValue(iter.value(), referrer_charset, &filename,
                          &parse_result_flags_);
      if (!filename.empty()) {
        parse_result_flags_ |= HAS_FILENAME;
        if (filename[0] == '\'')
          parse_result_flags_ |= HAS_SINGLE_QUOTED_FILENAME;
      }
    } else if (ext_filename.empty() &&
               base::EqualsCaseInsensitiveASCII(iter.name(), "filename*")) {
      DecodeExtValue(iter.raw_value(), &ext_filename);
      if (!ext_filename.empty())
        parse_result_flags_ |= HAS_EXT_FILENAME;
    }
  }

  if (!ext_filename.empty())
    filename_ = ext_filename;
  else
    filename_ = filename;

  if (!filename.empty() && filename[0] == '\'')
    parse_result_flags_ |= HAS_SINGLE_QUOTED_FILENAME;
}

}  // namespace net
