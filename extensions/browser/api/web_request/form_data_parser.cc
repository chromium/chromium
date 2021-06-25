// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/form_data_parser.h"

#include <stddef.h>

#include <vector>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/http/http_request_headers.h"
#include "third_party/re2/src/re2/re2.h"

using base::DictionaryValue;
using base::ListValue;
using base::StringPiece;
using re2::RE2;

namespace extensions {

namespace {

const char kContentDisposition[] = "content-disposition:";
const size_t kContentDispositionLength = base::size(kContentDisposition) - 1;
// kCharacterPattern is an allowed character in a URL encoding. Definition is
// from RFC 1738, end of section 2.2.
const char kCharacterPattern[] =
    "(?:[a-zA-Z0-9$_.+!*'(),]|-|(?:%[a-fA-F0-9]{2}))";
const char kEscapeClosingQuote[] = "\\\\E";

// A wrapper struct for static RE2 objects to be held as LazyInstance.
struct Patterns {
  Patterns();
  // Patterns is only instantiated as a leaky LazyInstance, so the destructor
  // is never called.
  ~Patterns() = delete;
  const RE2 transfer_padding_pattern;
  const RE2 crlf_pattern;
  const RE2 closing_pattern;
  const RE2 epilogue_pattern;
  const RE2 crlf_free_pattern;
  const RE2 preamble_pattern;
  const RE2 header_pattern;
  const RE2 content_disposition_pattern;
  const RE2 name_pattern;
  const RE2 value_pattern;
  const RE2 unquote_pattern;
  const RE2 url_encoded_pattern;
  const RE2 content_type_octet_stream;
};

Patterns::Patterns()
    : transfer_padding_pattern("[ \\t]*\\r\\n"),
      crlf_pattern("\\r\\n"),
      closing_pattern("--[ \\t]*"),
      epilogue_pattern("|\\r\\n(?s:.)*"),
      crlf_free_pattern("(?:[^\\r]|\\r+[^\\r\\n])*"),
      preamble_pattern(".+?"),
      header_pattern("[!-9;-~]+:(.|\\r\\n[\\t ])*\\r\\n"),
      content_disposition_pattern(std::string("(?i:") + kContentDisposition +
                                  ")"),
      name_pattern("\\bname=\"([^\"]*)\""),
      value_pattern("\\bfilename=\"([^\"]*)\""),
      unquote_pattern(kEscapeClosingQuote),
      url_encoded_pattern(std::string("(") + kCharacterPattern + "*)=(" +
                          kCharacterPattern + "*)"),
      content_type_octet_stream(
          "Content-Type: application\\/octet-stream\\r\\n") {}

base::LazyInstance<Patterns>::Leaky g_patterns = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Parses URLencoded forms, see
// http://www.w3.org/TR/REC-html40-971218/interact/forms.html#h-17.13.4.1 .
class FormDataParserUrlEncoded : public FormDataParser {
 public:
  FormDataParserUrlEncoded();
  ~FormDataParserUrlEncoded() override;

  // Implementation of FormDataParser.
  bool AllDataReadOK() override;
  bool GetNextNameValue(Result* result) override;
  bool SetSource(base::StringPiece source) override;

 private:
  // Returns the pattern to match a single name-value pair. This could be even
  // static, but then we would have to spend more code on initializing the
  // cached pointer to g_patterns.Get().
  const RE2& pattern() const {
    return patterns_->url_encoded_pattern;
  }

  // Auxiliary constant for using RE2. Number of arguments for parsing
  // name-value pairs (one for name, one for value).
  static const size_t args_size_ = 2u;

  re2::StringPiece source_;
  bool source_set_;
  bool source_malformed_;

  // Auxiliary store for using RE2.
  std::string name_;
  std::string value_;
  const RE2::Arg arg_name_;
  const RE2::Arg arg_value_;
  const RE2::Arg* args_[args_size_];

  // Caching the pointer to g_patterns.Get().
  const Patterns* patterns_;

  DISALLOW_COPY_AND_ASSIGN(FormDataParserUrlEncoded);
};

// The following class, FormDataParserMultipart, parses forms encoded as
// multipart, defined in RFCs 2388 (specific to forms), 2046 (multipart
// encoding) and 5322 (MIME-headers).
//
// Implementation details
//
// The original grammar from RFC 2046 is this, "multipart-body" being the root
// non-terminal:
//
// boundary := 0*69<bchars> bcharsnospace
// bchars := bcharsnospace / " "
// bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" / "+" / "_" / ","
//                  / "-" / "." / "/" / ":" / "=" / "?"
// dash-boundary := "--" boundary
// multipart-body := [preamble CRLF]
//                        dash-boundary transport-padding CRLF
//                        body-part *encapsulation
//                        close-delimiter transport-padding
//                        [CRLF epilogue]
// transport-padding := *LWSP-char
// encapsulation := delimiter transport-padding CRLF body-part
// delimiter := CRLF dash-boundary
// close-delimiter := delimiter "--"
// preamble := discard-text
// epilogue := discard-text
// discard-text := *(*text CRLF) *text
// body-part := MIME-part-headers [CRLF *OCTET]
// OCTET := <any 0-255 octet value>
//
// Uppercase non-terminals are defined in RFC 5234, Appendix B.1; i.e. CRLF,
// DIGIT, and ALPHA stand for "\r\n", '0'-'9' and the set of letters of the
// English alphabet, respectively.
// The non-terminal "text" is presumably just any text, excluding line breaks.
// The non-terminal "LWSP-char" is not directly defined in the original grammar
// but it means "linear whitespace", which is a space or a horizontal tab.
// The non-terminal "MIME-part-headers" is not discussed in RFC 2046, so we use
// the syntax for "optional fields" from Section 3.6.8 of RFC 5322:
//
// MIME-part-headers := field-name ":" unstructured CRLF
// field-name := 1*ftext
// ftext := %d33-57 /          ; Printable US-ASCII
//          %d59-126           ;  characters not including ":".
// Based on Section 2.2.1 of RFC 5322, "unstructured" matches any string which
// does not contain a CRLF sub-string, except for substrings "CRLF<space>" and
// "CRLF<horizontal tab>", which serve for "folding".
//
// The FormDataParseMultipart class reads the input source and tries to parse it
// according to the grammar above, rooted at the "multipart-body" non-terminal.
// This happens in stages:
//
// 1. The optional preamble and the initial dash-boundary with transport padding
// and a CRLF are read and ignored.
//
// 2. Repeatedly each body part is read. The body parts can either serve to
//    upload a file, or just a string of bytes.
// 2.a. The headers of that part are searched for the "content-disposition"
//      header, which contains the name of the value represented by that body
//      part. If the body-part is for file upload, that header also contains a
//      filename.
// 2.b. The "*OCTET" part of the body part is then read and passed as the value
//      of the name-value pair for body parts representing a string of bytes.
//      For body parts for uploading a file the "*OCTET" part is just ignored
//      and the filename is used for value instead.
//
// 3. The final close-delimiter and epilogue are read and ignored.
//
// IMPORTANT NOTE
// This parser supports sources split into multiple chunks. Therefore SetSource
// can be called multiple times if the source is spread over several chunks.
// However, the split may only occur inside a body part, right after the
// trailing CRLF of headers.
class FormDataParserMultipart : public FormDataParser {
 public:
  explicit FormDataParserMultipart(const std::string& boundary_separator);
  ~FormDataParserMultipart() override;

  // Implementation of FormDataParser.
  bool AllDataReadOK() override;
  bool GetNextNameValue(Result* result) override;
  bool SetSource(base::StringPiece source) override;

 private:
  enum State {
    STATE_INIT,      // No input read yet.
    STATE_READY,     // Ready to call GetNextNameValue.
    STATE_FINISHED,  // Read the input until the end.
    STATE_SUSPEND,   // Waiting until a new |source_| is set.
    STATE_ERROR
  };

  // Produces a regexp to match the string "--" + |literal|. The idea is to
  // represent "--" + |literal| as a "quoted pattern", a verbatim copy enclosed
  // in "\\Q" and "\\E". The only catch is to watch out for occurrences of "\\E"
  // inside |literal|. Those must be excluded from the quote and the backslash
  // doubly escaped. For example, for literal == "abc\\Edef" the result is
  // "\\Q--abc\\E\\\\E\\Qdef\\E".
  static std::string CreateBoundaryPatternFromLiteral(
      const std::string& literal);

  // Tests whether |input| has a prefix matching |pattern|.
  static bool StartsWithPattern(const re2::StringPiece& input,
                                const RE2& pattern);

  // If |source_| starts with a header, seeks |source_| beyond the header. If
  // the header is Content-Disposition, extracts |name| from "name=" and
  // possibly |value| from "filename=" fields of that header. Only if the
  // "name" or "filename" fields are found, then |name| or |value| are touched.
  // Returns true iff |source_| is seeked forward. Sets |value_assigned|
  // to true iff |value| has been assigned to. Sets |value_is_binary| to true if
  // header has content-type: application/octet-stream.
  bool TryReadHeader(base::StringPiece* name,
                     base::StringPiece* value,
                     bool* value_assigned,
                     bool* value_is_binary);

  // Helper to GetNextNameValue. Expects that the input starts with a data
  // portion of a body part. An attempt is made to read the input until the end
  // of that body part. If |data| is not NULL, it is set to contain the data
  // portion. Returns true iff the reading was successful.
  bool FinishReadingPart(base::StringPiece* data);

  // These methods could be even static, but then we would have to spend more
  // code on initializing the cached pointer to g_patterns.Get().
  const RE2& transfer_padding_pattern() const {
    return patterns_->transfer_padding_pattern;
  }
  const RE2& crlf_pattern() const {
    return patterns_->crlf_pattern;
  }
  const RE2& closing_pattern() const {
    return patterns_->closing_pattern;
  }
  const RE2& epilogue_pattern() const {
    return patterns_->epilogue_pattern;
  }
  const RE2& crlf_free_pattern() const {
    return patterns_->crlf_free_pattern;
  }
  const RE2& preamble_pattern() const {
    return patterns_->preamble_pattern;
  }
  const RE2& header_pattern() const {
    return patterns_->header_pattern;
  }
  const RE2& content_disposition_pattern() const {
    return patterns_->content_disposition_pattern;
  }
  const RE2& name_pattern() const {
    return patterns_->name_pattern;
  }
  const RE2& value_pattern() const {
    return patterns_->value_pattern;
  }

  const RE2& content_type_octet_stream() const {
    return patterns_->content_type_octet_stream;
  }

  // However, this is used in a static method so it needs to be static.
  static const RE2& unquote_pattern() {
    return g_patterns.Get().unquote_pattern;  // No caching g_patterns here.
  }

  const RE2 dash_boundary_pattern_;

  // Because of initialisation dependency, |state_| needs to be declared after
  // |dash_boundary_pattern_|.
  State state_;

  // The parsed message can be split into multiple sources which we read
  // sequentially.
  re2::StringPiece source_;

  // Caching the pointer to g_patterns.Get().
  const Patterns* patterns_;

  DISALLOW_COPY_AND_ASSIGN(FormDataParserMultipart);
};

FormDataParser::Result::Result() {}
FormDataParser::Result::~Result() {}

void FormDataParser::Result::SetBinaryValue(base::StringPiece str) {
  value_ = base::Value(
      base::Value::BlobStorage(str.data(), str.data() + str.size()));
}

void FormDataParser::Result::SetStringValue(std::string str) {
  value_ = base::Value(std::move(str));
}

FormDataParser::~FormDataParser() {}

// static
std::unique_ptr<FormDataParser> FormDataParser::Create(
    const net::HttpRequestHeaders& request_headers) {
  std::string value;
  const bool found =
      request_headers.GetHeader(net::HttpRequestHeaders::kContentType, &value);
  return CreateFromContentTypeHeader(found ? &value : NULL);
}

// static
std::unique_ptr<FormDataParser> FormDataParser::CreateFromContentTypeHeader(
    const std::string* content_type_header) {
  enum ParserChoice {URL_ENCODED, MULTIPART, ERROR_CHOICE};
  ParserChoice choice = ERROR_CHOICE;
  std::string boundary;

  if (content_type_header == NULL) {
    choice = URL_ENCODED;
  } else {
    const std::string content_type(
        content_type_header->substr(0, content_type_header->find(';')));

    if (base::EqualsCaseInsensitiveASCII(content_type,
                                         "application/x-www-form-urlencoded")) {
      choice = URL_ENCODED;
    } else if (base::EqualsCaseInsensitiveASCII(content_type,
                                                "multipart/form-data")) {
      static const char kBoundaryString[] = "boundary=";
      size_t offset = content_type_header->find(kBoundaryString);
      if (offset == std::string::npos) {
        // Malformed header.
        return std::unique_ptr<FormDataParser>();
      }
      offset += sizeof(kBoundaryString) - 1;
      boundary = content_type_header->substr(
          offset, content_type_header->find(';', offset));
      if (!boundary.empty())
        choice = MULTIPART;
    }
  }
  // Other cases are unparseable, including when |content_type| is "text/plain".

  switch (choice) {
    case URL_ENCODED:
      return std::unique_ptr<FormDataParser>(new FormDataParserUrlEncoded());
    case MULTIPART:
      return std::unique_ptr<FormDataParser>(
          new FormDataParserMultipart(boundary));
    case ERROR_CHOICE:
      return std::unique_ptr<FormDataParser>();
  }
  NOTREACHED();  // Some compilers do not believe this is unreachable.
  return std::unique_ptr<FormDataParser>();
}

FormDataParser::FormDataParser() {}

FormDataParserUrlEncoded::FormDataParserUrlEncoded()
    : source_(NULL),
      source_set_(false),
      source_malformed_(false),
      arg_name_(&name_),
      arg_value_(&value_),
      patterns_(g_patterns.Pointer()) {
  args_[0] = &arg_name_;
  args_[1] = &arg_value_;
}

FormDataParserUrlEncoded::~FormDataParserUrlEncoded() {}

bool FormDataParserUrlEncoded::AllDataReadOK() {
  // All OK means we read the whole source.
  return source_set_ && source_.empty() && !source_malformed_;
}

bool FormDataParserUrlEncoded::GetNextNameValue(Result* result) {
  if (!source_set_ || source_malformed_)
    return false;

  bool success = RE2::ConsumeN(&source_, pattern(), args_, args_size_);
  if (success) {
    const net::UnescapeRule::Type kUnescapeRules =
        net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;

    std::string unescaped_name =
        net::UnescapeBinaryURLComponent(name_, kUnescapeRules);
    result->set_name(unescaped_name);
    std::string unescaped_value =
        net::UnescapeBinaryURLComponent(value_, kUnescapeRules);
    const base::StringPiece unescaped_data(unescaped_value.data(),
                                           unescaped_value.length());
    if (base::IsStringUTF8(unescaped_data)) {
      result->SetStringValue(std::move(unescaped_value));
    } else {
      result->SetBinaryValue(unescaped_data);
    }
  }
  if (source_.length() > 0) {
    if (source_[0] == '&')
      source_.remove_prefix(1);  // Remove the leading '&'.
    else
      source_malformed_ = true;  // '&' missing between two name-value pairs.
  }
  return success && !source_malformed_;
}

bool FormDataParserUrlEncoded::SetSource(base::StringPiece source) {
  if (source_set_)
    return false;  // We do not allow multiple sources for this parser.
  source_.set(source.data(), source.size());
  source_set_ = true;
  source_malformed_ = false;
  return true;
}

// static
std::string FormDataParserMultipart::CreateBoundaryPatternFromLiteral(
    const std::string& literal) {
  static const char quote[] = "\\Q";
  static const char unquote[] = "\\E";

  // The result always starts with opening the qoute and then "--".
  std::string result("\\Q--");

  // This StringPiece is used below to record the next occurrence of "\\E" in
  // |literal|.
  re2::StringPiece seek_unquote(literal);
  const char* copy_start = literal.data();
  size_t copy_length = literal.size();

  // Find all "\\E" in |literal| and exclude them from the \Q...\E quote.
  while (RE2::FindAndConsume(&seek_unquote, unquote_pattern())) {
    copy_length = seek_unquote.data() - copy_start;
    result.append(copy_start, copy_length);
    result.append(kEscapeClosingQuote);
    result.append(quote);
    copy_start = seek_unquote.data();
  }

  // Finish the last \Q...\E quote.
  copy_length = (literal.data() + literal.size()) - copy_start;
  result.append(copy_start, copy_length);
  result.append(unquote);
  return result;
}

// static
bool FormDataParserMultipart::StartsWithPattern(const re2::StringPiece& input,
                                                const RE2& pattern) {
  return pattern.Match(input, 0, input.size(), RE2::ANCHOR_START, NULL, 0);
}

FormDataParserMultipart::FormDataParserMultipart(
    const std::string& boundary_separator)
    : dash_boundary_pattern_(
          CreateBoundaryPatternFromLiteral(boundary_separator)),
      state_(dash_boundary_pattern_.ok() ? STATE_INIT : STATE_ERROR),
      patterns_(g_patterns.Pointer()) {}

FormDataParserMultipart::~FormDataParserMultipart() {}

bool FormDataParserMultipart::AllDataReadOK() {
  return state_ == STATE_FINISHED;
}

bool FormDataParserMultipart::FinishReadingPart(base::StringPiece* data) {
  const char* data_start = source_.data();
  while (!StartsWithPattern(source_, dash_boundary_pattern_)) {
    if (!RE2::Consume(&source_, crlf_free_pattern()) ||
        !RE2::Consume(&source_, crlf_pattern())) {
      state_ = STATE_ERROR;
      return false;
    }
  }
  if (data != NULL) {
    if (source_.data() == data_start) {
      // No data in this body part.
      state_ = STATE_ERROR;
      return false;
    }
    // Subtract 2 for the trailing "\r\n".
    *data = base::StringPiece(data_start, source_.data() - data_start - 2);
  }

  // Finally, read the dash-boundary and either skip to the next body part, or
  // finish reading the source.
  CHECK(RE2::Consume(&source_, dash_boundary_pattern_));
  if (StartsWithPattern(source_, closing_pattern())) {
    CHECK(RE2::Consume(&source_, closing_pattern()));
    if (RE2::Consume(&source_, epilogue_pattern()))
      state_ = STATE_FINISHED;
    else
      state_ = STATE_ERROR;
  } else {  // Next body part ahead.
    if (!RE2::Consume(&source_, transfer_padding_pattern()))
      state_ = STATE_ERROR;
  }
  return state_ != STATE_ERROR;
}

bool FormDataParserMultipart::GetNextNameValue(Result* result) {
  if (source_.empty() || state_ != STATE_READY)
    return false;

  // 1. Read body-part headers.
  base::StringPiece name;
  base::StringPiece value;
  bool value_assigned = false;
  bool value_is_binary = false;
  bool value_assigned_temp;
  bool value_is_binary_temp;
  while (TryReadHeader(&name, &value, &value_assigned_temp,
                       &value_is_binary_temp)) {
    value_is_binary |= value_is_binary_temp;
    value_assigned |= value_assigned_temp;
  }
  if (name.empty() || state_ == STATE_ERROR) {
    state_ = STATE_ERROR;
    return false;
  }

  // 2. Read the trailing CRLF after headers.
  if (!RE2::Consume(&source_, crlf_pattern())) {
    state_ = STATE_ERROR;
    return false;
  }

  // 3. Read the data of this body part, i.e., everything until the first
  // dash-boundary.
  bool return_value;
  if (value_assigned && source_.empty()) {  // Wait for a new source?
    return_value = true;
    state_ = STATE_SUSPEND;
  } else {
    return_value = FinishReadingPart(value_assigned ? nullptr : &value);
  }

  result->set_name(net::UnescapeBinaryURLComponent(name));
  if (value_assigned) {
    // Hold filename as value.
    result->SetStringValue(value.as_string());
  } else if (value_is_binary) {
    result->SetBinaryValue(value);
  } else {
    result->SetStringValue(value.as_string());
  }

  return return_value;
}

bool FormDataParserMultipart::SetSource(base::StringPiece source) {
  if (source.data() == NULL || !source_.empty())
    return false;
  source_.set(source.data(), source.size());

  switch (state_) {
    case STATE_INIT:
      // Seek behind the preamble.
      while (!StartsWithPattern(source_, dash_boundary_pattern_)) {
        if (!RE2::Consume(&source_, preamble_pattern())) {
          state_ = STATE_ERROR;
          break;
        }
      }
      // Read dash-boundary, transfer padding, and CRLF.
      if (state_ != STATE_ERROR) {
        if (!RE2::Consume(&source_, dash_boundary_pattern_) ||
            !RE2::Consume(&source_, transfer_padding_pattern()))
          state_ = STATE_ERROR;
        else
          state_ = STATE_READY;
      }
      break;
    case STATE_READY:  // Nothing to do.
      break;
    case STATE_SUSPEND:
      state_ = FinishReadingPart(nullptr) ? STATE_READY : STATE_ERROR;
      break;
    default:
      state_ = STATE_ERROR;
  }
  return state_ != STATE_ERROR;
}

bool FormDataParserMultipart::TryReadHeader(base::StringPiece* name,
                                            base::StringPiece* value,
                                            bool* value_assigned,
                                            bool* value_is_binary) {
  *value_assigned = false;
  *value_is_binary = false;
  // Support Content-Type: application/octet-stream.
  // Form data with this content type is represented as string of bytes.
  if (RE2::Consume(&source_, content_type_octet_stream())) {
    *value_is_binary = true;
    return true;
  }
  const char* header_start = source_.data();
  if (!RE2::Consume(&source_, header_pattern()))
    return false;
  // (*) After this point we must return true, because we consumed one header.

  // Subtract 2 for the trailing "\r\n".
  re2::StringPiece header(header_start, source_.data() - header_start - 2);

  if (!StartsWithPattern(header, content_disposition_pattern()))
    return true;  // Skip headers that don't describe the content-disposition.

  re2::StringPiece groups[2];

  if (!name_pattern().Match(header,
                            kContentDispositionLength, header.size(),
                            RE2::UNANCHORED, groups, 2)) {
    state_ = STATE_ERROR;
    return true;  // See (*) for why true.
  }
  *name = base::StringPiece(groups[1].data(), groups[1].size());

  if (value_pattern().Match(header,
                            kContentDispositionLength, header.size(),
                            RE2::UNANCHORED, groups, 2)) {
    *value = base::StringPiece(groups[1].data(), groups[1].size());
    *value_assigned = true;
  }
  return true;
}

}  // namespace extensions
