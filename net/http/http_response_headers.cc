// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for header parsing were borrowed from Firefox:
// http://lxr.mozilla.org/seamonkey/source/netwerk/protocol/http/src/nsHttpResponseHead.cpp
// The rules for parsing content-types were also borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_response_headers.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/parse_number.h"
#include "net/base/tracing.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_log_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/http/structured_headers.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"

using base::Time;

namespace net {

//-----------------------------------------------------------------------------

namespace {

// These headers are RFC 2616 hop-by-hop headers;
// not to be stored by caches.
const char* const kHopByHopResponseHeaders[] = {
  "connection",
  "proxy-connection",
  "keep-alive",
  "trailer",
  "transfer-encoding",
  "upgrade"
};

// These headers are challenge response headers;
// not to be stored by caches.
const char* const kChallengeResponseHeaders[] = {
  "www-authenticate",
  "proxy-authenticate"
};

// These headers are cookie setting headers;
// not to be stored by caches or disclosed otherwise.
const char* const kCookieResponseHeaders[] = {
  "set-cookie",
  "set-cookie2",
  "clear-site-data",
};

// By default, do not cache Strict-Transport-Security.
// This avoids erroneously re-processing it on page loads from cache ---
// it is defined to be valid only on live and error-free HTTPS connections.
const char* const kSecurityStateHeaders[] = {
  "strict-transport-security",
};

// These response headers are not copied from a 304/206 response to the cached
// response headers.  This list is based on Mozilla's nsHttpResponseHead.cpp.
const char* const kNonUpdatedHeaders[] = {
    "connection",
    "proxy-connection",
    "keep-alive",
    "www-authenticate",
    "proxy-authenticate",
    "proxy-authorization",
    "te",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "content-location",
    "content-md5",
    "etag",
    "content-encoding",
    "content-range",
    "content-type",
    "content-length",
    "x-frame-options",
    "x-xss-protection",
};

// Some header prefixes mean "Don't copy this header from a 304 response.".
// Rather than listing all the relevant headers, we can consolidate them into
// this list:
const char* const kNonUpdatedHeaderPrefixes[] = {
  "x-content-",
  "x-webkit-"
};

constexpr char kActivateStorageAccessHeader[] = "activate-storage-access";

bool ShouldUpdateHeader(std::string_view name) {
  for (const auto* header : kNonUpdatedHeaders) {
    if (base::EqualsCaseInsensitiveASCII(name, header))
      return false;
  }
  for (const auto* prefix : kNonUpdatedHeaderPrefixes) {
    if (base::StartsWith(name, prefix, base::CompareCase::INSENSITIVE_ASCII))
      return false;
  }
  return true;
}

bool HasEmbeddedNulls(std::string_view str) {
  return str.find('\0') != std::string::npos;
}

void CheckDoesNotHaveEmbeddedNulls(std::string_view str) {
  // Care needs to be taken when adding values to the raw headers string to
  // make sure it does not contain embeded NULLs. Any embeded '\0' may be
  // understood as line terminators and change how header lines get tokenized.
  CHECK(!HasEmbeddedNulls(str));
}

void RemoveLeadingSpaces(std::string_view* s) {
  s->remove_prefix(std::min(s->find_first_not_of(' '), s->size()));
}

// Parses `status` for response code and status text. Returns the response code,
// and appends the response code and trimmed status text preceded by a space to
// `append_to`. For example, given the input " 404 Not found " would return 404
// and append " 404 Not found" to `append_to`. The odd calling convention is
// necessary to avoid extra copies in the implementation of
// HttpResponseHeaders::ParseStatusLine().
int ParseStatus(std::string_view status, std::string& append_to) {
  // Skip whitespace. Tabs are not skipped, for backwards compatibility.
  RemoveLeadingSpaces(&status);

  auto first_non_digit = std::ranges::find_if(
      status, [](char c) { return !base::IsAsciiDigit(c); });

  if (first_non_digit == status.begin()) {
    DVLOG(1) << "missing response status number; assuming 200";
    append_to.append(" 200");
    return HTTP_OK;
  }

  append_to.push_back(' ');
  append_to.append(status.begin(), first_non_digit);
  int response_code = -1;
  // For backwards compatibility, overlarge response codes are permitted.
  // base::StringToInt will clamp the value to INT_MAX.
  base::StringToInt(base::MakeStringPiece(status.begin(), first_non_digit),
                    &response_code);
  CHECK_GE(response_code, 0);

  status.remove_prefix(first_non_digit - status.begin());

  // Skip whitespace. Tabs are not skipped, as before.
  RemoveLeadingSpaces(&status);

  // Trim trailing whitespace. Tabs are not trimmed.
  const size_t last_non_space_pos = status.find_last_not_of(' ');
  if (last_non_space_pos != std::string_view::npos) {
    status.remove_suffix(status.size() - last_non_space_pos - 1);
  }

  if (status.empty()) {
    return response_code;
  }

  CheckDoesNotHaveEmbeddedNulls(status);

  append_to.push_back(' ');
  append_to.append(status);
  return response_code;
}

}  // namespace

const char HttpResponseHeaders::kContentRange[] = "Content-Range";
const char HttpResponseHeaders::kLastModified[] = "Last-Modified";
const char HttpResponseHeaders::kVary[] = "Vary";

struct HttpResponseHeaders::ParsedHeader {
  // A header "continuation" contains only a subsequent value for the
  // preceding header.  (Header values are comma separated.)
  bool is_continuation() const { return name_begin == name_end; }

  std::string::const_iterator name_begin;
  std::string::const_iterator name_end;
  std::string::const_iterator value_begin;
  std::string::const_iterator value_end;

  // Write a representation of this object into a tracing proto.
  void WriteIntoTrace(perfetto::TracedValue context) const {
    auto dict = std::move(context).WriteDictionary();
    dict.Add("name", base::MakeStringPiece(name_begin, name_end));
    dict.Add("value", base::MakeStringPiece(value_begin, value_end));
  }
};

//-----------------------------------------------------------------------------

HttpResponseHeaders::Builder::Builder(HttpVersion version,
                                      std::string_view status)
    : version_(version), status_(status) {
  DCHECK(version == HttpVersion(1, 0) || version == HttpVersion(1, 1) ||
         version == HttpVersion(2, 0));
}

HttpResponseHeaders::Builder::~Builder() = default;

scoped_refptr<HttpResponseHeaders> HttpResponseHeaders::Builder::Build() {
  return base::MakeRefCounted<HttpResponseHeaders>(BuilderPassKey(), version_,
                                                   status_, headers_);
}

HttpResponseHeaders::HttpResponseHeaders(const std::string& raw_input)
    : response_code_(-1) {
  Parse(raw_input);

  // As it happens right now, there aren't double-constructions of response
  // headers using this constructor, so our counts should also be accurate,
  // without instantiating the histogram in two places.  It is also
  // important that this histogram not collect data in the other
  // constructor, which rebuilds an histogram from a pickle, since
  // that would actually create a double call between the original
  // HttpResponseHeader that was serialized, and initialization of the
  // new object from that pickle.
  if (base::FeatureList::IsEnabled(features::kOptimizeParsingDataUrls)) {
    std::optional<HttpStatusCode> status_code =
        TryToGetHttpStatusCode(response_code_);
    if (status_code.has_value()) {
      UMA_HISTOGRAM_ENUMERATION("Net.HttpResponseCode2", status_code.value(),
                                net::HttpStatusCode::HTTP_STATUS_CODE_MAX);
    }
  } else {
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(
        "Net.HttpResponseCode",
        HttpUtil::MapStatusCodeForHistogram(response_code_),
        // Note the third argument is only
        // evaluated once, see macro
        // definition for details.
        HttpUtil::GetStatusCodesForHistogram());
  }
}

HttpResponseHeaders::HttpResponseHeaders(base::PickleIterator* iter)
    : response_code_(-1) {
  std::string raw_input;
  if (iter->ReadString(&raw_input))
    Parse(raw_input);
}

HttpResponseHeaders::HttpResponseHeaders(
    BuilderPassKey,
    HttpVersion version,
    std::string_view status,
    base::span<const std::pair<std::string_view, std::string_view>> headers)
    : http_version_(version) {
  // This must match the behaviour of Parse(). We don't use Parse() because
  // avoiding the overhead of parsing is the point of this constructor.

  std::string formatted_status;
  formatted_status.reserve(status.size() + 1);  // ParseStatus() may add a space
  response_code_ = ParseStatus(status, formatted_status);

  // First calculate how big the output will be so that we can allocate the
  // right amount of memory.
  size_t expected_size = 8;  // "HTTP/x.x"
  expected_size += formatted_status.size();
  expected_size += 1;  // "\0"
  size_t expected_parsed_size = 0;

  // Track which headers (by index) have a comma in the value. Since bools are
  // only 1 byte, we can afford to put 100 of them on the stack and avoid
  // allocating more memory 99.9% of the time.
  absl::InlinedVector<bool, 100> header_contains_comma;
  for (const auto& [key, value] : headers) {
    expected_size += key.size();
    expected_size += 1;  // ":"
    expected_size += value.size();
    expected_size += 1;  // "\0"
    // It's okay if we over-estimate the size of `parsed_`, so treat all ','
    // characters as if they might split the value to avoid parsing the value
    // carefully here.
    const size_t comma_count = base::ranges::count(value, ',') + 1;
    expected_parsed_size += comma_count;
    header_contains_comma.push_back(comma_count);
  }
  expected_size += 1;  // "\0"
  raw_headers_.reserve(expected_size);
  parsed_.reserve(expected_parsed_size);

  // Now fill in the output.
  const uint16_t major = version.major_value();
  const uint16_t minor = version.minor_value();
  CHECK_LE(major, 9);
  CHECK_LE(minor, 9);
  raw_headers_.append("HTTP/");
  raw_headers_.push_back('0' + major);
  raw_headers_.push_back('.');
  raw_headers_.push_back('0' + minor);
  raw_headers_.append(formatted_status);
  raw_headers_.push_back('\0');
  // It is vital that `raw_headers_` iterators are not invalidated after this
  // point.
  const char* const data_at_start = raw_headers_.data();
  size_t index = 0;
  for (const auto& [key, value] : headers) {
    CheckDoesNotHaveEmbeddedNulls(key);
    CheckDoesNotHaveEmbeddedNulls(value);
    // Because std::string iterators are random-access, end() has to point to
    // the position where the next character will be appended.
    const auto name_begin = raw_headers_.cend();
    raw_headers_.append(key);
    const auto name_end = raw_headers_.cend();
    raw_headers_.push_back(':');
    auto values_begin = raw_headers_.cend();
    raw_headers_.append(value);
    auto values_end = raw_headers_.cend();
    raw_headers_.push_back('\0');
    // The HTTP/2 standard disallows header values starting or ending with
    // whitespace (RFC 9113 8.2.1). Hopefully the same is also true of HTTP/3.
    // TODO(crbug.com/40282642): Validate that our implementations
    // actually enforce this constraint and change this TrimLWS() to a DCHECK.
    HttpUtil::TrimLWS(&values_begin, &values_end);
    AddHeader(name_begin, name_end, values_begin, values_end,
              header_contains_comma[index] ? ContainsCommas::kYes
                                           : ContainsCommas::kNo);
    ++index;
  }
  raw_headers_.push_back('\0');
  CHECK_EQ(expected_size, raw_headers_.size());
  CHECK_EQ(data_at_start, raw_headers_.data());
  DCHECK_LE(parsed_.size(), expected_parsed_size);

  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
}

scoped_refptr<HttpResponseHeaders> HttpResponseHeaders::TryToCreate(
    std::string_view headers) {
  // Reject strings with nulls.
  if (HasEmbeddedNulls(headers) ||
      headers.size() > std::numeric_limits<int>::max()) {
    return nullptr;
  }

  return base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(headers));
}

scoped_refptr<HttpResponseHeaders> HttpResponseHeaders::TryToCreateForDataURL(
    std::string_view content_type) {
  // Reject strings with nulls.
  if (HasEmbeddedNulls(content_type) ||
      content_type.size() > std::numeric_limits<int>::max()) {
    return nullptr;
  }

  constexpr char kStatusLineAndHeaderName[] = "HTTP/1.1 200 OK\0Content-Type:";
  std::string raw_headers =
      base::StrCat({std::string_view(kStatusLineAndHeaderName,
                                     sizeof(kStatusLineAndHeaderName) - 1),
                    content_type, std::string_view("\0\0", 2)});

  return base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
}

void HttpResponseHeaders::Persist(base::Pickle* pickle,
                                  PersistOptions options) {
  if (options == PERSIST_RAW) {
    pickle->WriteString(raw_headers_);
    return;  // Done.
  }

  HeaderSet filter_headers;

  // Construct set of headers to filter out based on options.
  if ((options & PERSIST_SANS_NON_CACHEABLE) == PERSIST_SANS_NON_CACHEABLE)
    AddNonCacheableHeaders(&filter_headers);

  if ((options & PERSIST_SANS_COOKIES) == PERSIST_SANS_COOKIES)
    AddCookieHeaders(&filter_headers);

  if ((options & PERSIST_SANS_CHALLENGES) == PERSIST_SANS_CHALLENGES)
    AddChallengeHeaders(&filter_headers);

  if ((options & PERSIST_SANS_HOP_BY_HOP) == PERSIST_SANS_HOP_BY_HOP)
    AddHopByHopHeaders(&filter_headers);

  if ((options & PERSIST_SANS_RANGES) == PERSIST_SANS_RANGES)
    AddHopContentRangeHeaders(&filter_headers);

  if ((options & PERSIST_SANS_SECURITY_STATE) == PERSIST_SANS_SECURITY_STATE)
    AddSecurityStateHeaders(&filter_headers);

  std::string blob;
  blob.reserve(raw_headers_.size());

  // This copies the status line w/ terminator null.
  // Note raw_headers_ has embedded nulls instead of \n,
  // so this just copies the first header line.
  blob.assign(raw_headers_.c_str(), strlen(raw_headers_.c_str()) + 1);

  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < parsed_.size() && parsed_[k].is_continuation()) {}
    --k;

    std::string header_name = base::ToLowerASCII(
        base::MakeStringPiece(parsed_[i].name_begin, parsed_[i].name_end));
    if (filter_headers.find(header_name) == filter_headers.end()) {
      // Make sure there is a null after the value.
      blob.append(parsed_[i].name_begin, parsed_[k].value_end);
      blob.push_back('\0');
    }

    i = k;
  }
  blob.push_back('\0');

  pickle->WriteString(blob);
}

void HttpResponseHeaders::Update(const HttpResponseHeaders& new_headers) {
  DCHECK(new_headers.response_code() == HTTP_NOT_MODIFIED ||
         new_headers.response_code() == HTTP_PARTIAL_CONTENT);

  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  HeaderSet updated_headers;

  // NOTE: we write the new headers then the old headers for convenience.  The
  // order should not matter.

  // Figure out which headers we want to take from new_headers:
  for (size_t i = 0; i < new_headers.parsed_.size(); ++i) {
    const HeaderList& new_parsed = new_headers.parsed_;

    DCHECK(!new_parsed[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < new_parsed.size() && new_parsed[k].is_continuation()) {}
    --k;

    auto name =
        base::MakeStringPiece(new_parsed[i].name_begin, new_parsed[i].name_end);
    if (ShouldUpdateHeader(name)) {
      std::string name_lower = base::ToLowerASCII(name);
      updated_headers.insert(name_lower);

      // Preserve this header line in the merged result, making sure there is
      // a null after the value.
      new_raw_headers.append(new_parsed[i].name_begin, new_parsed[k].value_end);
      new_raw_headers.push_back('\0');
    }

    i = k;
  }

  // Now, build the new raw headers.
  MergeWithHeaders(std::move(new_raw_headers), updated_headers);
}

void HttpResponseHeaders::MergeWithHeaders(std::string raw_headers,
                                           const HeaderSet& headers_to_remove) {
  for (size_t i = 0; i < parsed_.size(); ++i) {
    DCHECK(!parsed_[i].is_continuation());

    // Locate the start of the next header.
    size_t k = i;
    while (++k < parsed_.size() && parsed_[k].is_continuation()) {}
    --k;

    std::string name = base::ToLowerASCII(
        base::MakeStringPiece(parsed_[i].name_begin, parsed_[i].name_end));
    if (headers_to_remove.find(name) == headers_to_remove.end()) {
      // It's ok to preserve this header in the final result.
      raw_headers.append(parsed_[i].name_begin, parsed_[k].value_end);
      raw_headers.push_back('\0');
    }

    i = k;
  }
  raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(raw_headers);
}

void HttpResponseHeaders::RemoveHeader(std::string_view name) {
  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  HeaderSet to_remove;
  to_remove.insert(base::ToLowerASCII(name));
  MergeWithHeaders(std::move(new_raw_headers), to_remove);
}

void HttpResponseHeaders::RemoveHeaders(
    const std::unordered_set<std::string>& header_names) {
  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(raw_headers_.c_str());
  new_raw_headers.push_back('\0');

  HeaderSet to_remove;
  for (const auto& header_name : header_names) {
    to_remove.insert(base::ToLowerASCII(header_name));
  }
  MergeWithHeaders(std::move(new_raw_headers), to_remove);
}

void HttpResponseHeaders::RemoveHeaderLine(const std::string& name,
                                           const std::string& value) {
  std::string name_lowercase = base::ToLowerASCII(name);

  std::string new_raw_headers(GetStatusLine());
  new_raw_headers.push_back('\0');

  new_raw_headers.reserve(raw_headers_.size());

  size_t iter = 0;
  std::string old_header_name;
  std::string old_header_value;
  while (EnumerateHeaderLines(&iter, &old_header_name, &old_header_value)) {
    std::string old_header_name_lowercase = base::ToLowerASCII(old_header_name);
    if (name_lowercase == old_header_name_lowercase &&
        value == old_header_value)
      continue;

    new_raw_headers.append(old_header_name);
    new_raw_headers.push_back(':');
    new_raw_headers.push_back(' ');
    new_raw_headers.append(old_header_value);
    new_raw_headers.push_back('\0');
  }
  new_raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::AddHeader(std::string_view name,
                                    std::string_view value) {
  DCHECK(HttpUtil::IsValidHeaderName(name));
  DCHECK(HttpUtil::IsValidHeaderValue(value));

  // Don't copy the last null.
  std::string new_raw_headers(raw_headers_, 0, raw_headers_.size() - 1);
  new_raw_headers.append(name.begin(), name.end());
  new_raw_headers.append(": ");
  new_raw_headers.append(value.begin(), value.end());
  new_raw_headers.push_back('\0');
  new_raw_headers.push_back('\0');

  // Make this object hold the new data.
  raw_headers_.clear();
  parsed_.clear();
  Parse(new_raw_headers);
}

void HttpResponseHeaders::SetHeader(std::string_view name,
                                    std::string_view value) {
  RemoveHeader(name);
  AddHeader(name, value);
}

void HttpResponseHeaders::AddCookie(const std::string& cookie_string) {
  AddHeader("Set-Cookie", cookie_string);
}

void HttpResponseHeaders::ReplaceStatusLine(const std::string& new_status) {
  CheckDoesNotHaveEmbeddedNulls(new_status);
  // Copy up to the null byte.  This just copies the status line.
  std::string new_raw_headers(new_status);
  new_raw_headers.push_back('\0');

  HeaderSet empty_to_remove;
  MergeWithHeaders(std::move(new_raw_headers), empty_to_remove);
}

void HttpResponseHeaders::UpdateWithNewRange(const HttpByteRange& byte_range,
                                             int64_t resource_size,
                                             bool replace_status_line) {
  DCHECK(byte_range.IsValid());
  DCHECK(byte_range.HasFirstBytePosition());
  DCHECK(byte_range.HasLastBytePosition());

  const char kLengthHeader[] = "Content-Length";
  const char kRangeHeader[] = "Content-Range";

  RemoveHeader(kLengthHeader);
  RemoveHeader(kRangeHeader);

  int64_t start = byte_range.first_byte_position();
  int64_t end = byte_range.last_byte_position();
  int64_t range_len = end - start + 1;

  if (replace_status_line)
    ReplaceStatusLine("HTTP/1.1 206 Partial Content");

  AddHeader(kRangeHeader,
            base::StringPrintf("bytes %" PRId64 "-%" PRId64 "/%" PRId64, start,
                               end, resource_size));
  AddHeader(kLengthHeader, base::StringPrintf("%" PRId64, range_len));
}

void HttpResponseHeaders::Parse(const std::string& raw_input) {
  raw_headers_.reserve(raw_input.size());
  // TODO(crbug.com/40277776): Call reserve() on `parsed_` with an
  // appropriate value.

  // ParseStatusLine adds a normalized status line to raw_headers_
  std::string::const_iterator line_begin = raw_input.begin();
  std::string::const_iterator line_end = base::ranges::find(raw_input, '\0');
  // has_headers = true, if there is any data following the status line.
  // Used by ParseStatusLine() to decide if a HTTP/0.9 is really a HTTP/1.0.
  bool has_headers =
      (line_end != raw_input.end() && (line_end + 1) != raw_input.end() &&
       *(line_end + 1) != '\0');
  ParseStatusLine(line_begin, line_end, has_headers);
  raw_headers_.push_back('\0');  // Terminate status line with a null.

  if (line_end == raw_input.end()) {
    raw_headers_.push_back('\0');  // Ensure the headers end with a double null.

    DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
    DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
    return;
  }

  // Including a terminating null byte.
  size_t status_line_len = raw_headers_.size();

  // Now, we add the rest of the raw headers to raw_headers_, and begin parsing
  // it (to populate our parsed_ vector).
  raw_headers_.append(line_end + 1, raw_input.end());

  // Ensure the headers end with a double null.
  while (raw_headers_.size() < 2 ||
         raw_headers_[raw_headers_.size() - 2] != '\0' ||
         raw_headers_[raw_headers_.size() - 1] != '\0') {
    raw_headers_.push_back('\0');
  }

  // Adjust to point at the null byte following the status line
  line_end = raw_headers_.begin() + status_line_len - 1;

  HttpUtil::HeadersIterator headers(line_end + 1, raw_headers_.end(),
                                    std::string(1, '\0'));
  while (headers.GetNext()) {
    AddHeader(headers.name_begin(), headers.name_end(), headers.values_begin(),
              headers.values_end(), ContainsCommas::kMaybe);
  }

  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 2]);
  DCHECK_EQ('\0', raw_headers_[raw_headers_.size() - 1]);
}

bool HttpResponseHeaders::GetNormalizedHeader(std::string_view name,
                                              std::string* value) const {
  // If you hit this assertion, please use EnumerateHeader instead!
  DCHECK(!HttpUtil::IsNonCoalescingHeader(name));

  value->clear();

  bool found = false;
  size_t i = 0;
  while (i < parsed_.size()) {
    i = FindHeader(i, name);
    if (i == std::string::npos)
      break;

    if (found)
      value->append(", ");

    found = true;

    std::string::const_iterator value_begin = parsed_[i].value_begin;
    std::string::const_iterator value_end = parsed_[i].value_end;
    while (++i < parsed_.size() && parsed_[i].is_continuation())
      value_end = parsed_[i].value_end;
    value->append(value_begin, value_end);
  }

  return found;
}

std::string HttpResponseHeaders::GetStatusLine() const {
  // copy up to the null byte.
  return std::string(raw_headers_.c_str());
}

std::string HttpResponseHeaders::GetStatusText() const {
  // GetStatusLine() is already normalized, so it has the format:
  // '<http_version> SP <response_code>' or
  // '<http_version> SP <response_code> SP <status_text>'.
  std::string status_text = GetStatusLine();
  // Seek to beginning of <response_code>.
  std::string::const_iterator begin = base::ranges::find(status_text, ' ');
  std::string::const_iterator end = status_text.end();
  CHECK(begin != end);
  ++begin;
  CHECK(begin != end);
  // See if there is another space.
  begin = std::find(begin, end, ' ');
  if (begin == end)
    return std::string();
  ++begin;
  CHECK(begin != end);
  return std::string(begin, end);
}

bool HttpResponseHeaders::EnumerateHeaderLines(size_t* iter,
                                               std::string* name,
                                               std::string* value) const {
  size_t i = *iter;
  if (i == parsed_.size())
    return false;

  DCHECK(!parsed_[i].is_continuation());

  name->assign(parsed_[i].name_begin, parsed_[i].name_end);

  std::string::const_iterator value_begin = parsed_[i].value_begin;
  std::string::const_iterator value_end = parsed_[i].value_end;
  while (++i < parsed_.size() && parsed_[i].is_continuation())
    value_end = parsed_[i].value_end;

  value->assign(value_begin, value_end);

  *iter = i;
  return true;
}

std::optional<std::string_view> HttpResponseHeaders::EnumerateHeader(
    size_t* iter,
    std::string_view name) const {
  size_t i;
  if (!iter || !*iter) {
    i = FindHeader(0, name);
  } else {
    i = *iter;
    if (i >= parsed_.size()) {
      i = std::string::npos;
    } else if (!parsed_[i].is_continuation()) {
      i = FindHeader(i, name);
    }
  }

  if (i == std::string::npos) {
    return std::nullopt;
  }

  if (iter)
    *iter = i + 1;
  return std::string_view(parsed_[i].value_begin, parsed_[i].value_end);
}

bool HttpResponseHeaders::EnumerateHeader(size_t* iter,
                                          std::string_view name,
                                          std::string* value) const {
  std::optional<std::string_view> result = EnumerateHeader(iter, name);
  if (!result) {
    value->clear();
    return false;
  }
  value->assign(*result);
  return true;
}

bool HttpResponseHeaders::HasHeaderValue(std::string_view name,
                                         std::string_view value) const {
  // The value has to be an exact match.  This is important since
  // 'cache-control: no-cache' != 'cache-control: no-cache="foo"'
  size_t iter = 0;
  std::optional<std::string_view> temp;
  while ((temp = EnumerateHeader(&iter, name))) {
    if (base::EqualsCaseInsensitiveASCII(value, *temp)) {
      return true;
    }
  }
  return false;
}

bool HttpResponseHeaders::HasHeader(std::string_view name) const {
  return FindHeader(0, name) != std::string::npos;
}

HttpResponseHeaders::~HttpResponseHeaders() = default;

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
// static
HttpVersion HttpResponseHeaders::ParseVersion(
    std::string::const_iterator line_begin,
    std::string::const_iterator line_end) {
  std::string::const_iterator p = line_begin;

  // RFC9112 Section 2.3:
  // HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
  // HTTP-name     = %s"HTTP"

  if (!base::StartsWith(base::MakeStringPiece(line_begin, line_end), "http",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    DVLOG(1) << "missing status line";
    return HttpVersion();
  }

  p += 4;

  if (p >= line_end || *p != '/') {
    DVLOG(1) << "missing version";
    return HttpVersion();
  }

  std::string::const_iterator dot = std::find(p, line_end, '.');
  if (dot == line_end) {
    DVLOG(1) << "malformed version";
    return HttpVersion();
  }

  ++p;  // from / to first digit.
  ++dot;  // from . to second digit.

  if (!(base::IsAsciiDigit(*p) && base::IsAsciiDigit(*dot))) {
    DVLOG(1) << "malformed version number";
    return HttpVersion();
  }

  uint16_t major = *p - '0';
  uint16_t minor = *dot - '0';

  return HttpVersion(major, minor);
}

// Note: this implementation implicitly assumes that line_end points at a valid
// sentinel character (such as '\0').
void HttpResponseHeaders::ParseStatusLine(
    std::string::const_iterator line_begin,
    std::string::const_iterator line_end,
    bool has_headers) {
  // Extract the version number
  HttpVersion parsed_http_version = ParseVersion(line_begin, line_end);

  // Clamp the version number to one of: {0.9, 1.0, 1.1, 2.0}
  if (parsed_http_version == HttpVersion(0, 9) && !has_headers) {
    http_version_ = HttpVersion(0, 9);
    raw_headers_ = "HTTP/0.9";
  } else if (parsed_http_version == HttpVersion(2, 0)) {
    http_version_ = HttpVersion(2, 0);
    raw_headers_ = "HTTP/2.0";
  } else if (parsed_http_version >= HttpVersion(1, 1)) {
    http_version_ = HttpVersion(1, 1);
    raw_headers_ = "HTTP/1.1";
  } else {
    // Treat everything else like HTTP 1.0
    http_version_ = HttpVersion(1, 0);
    raw_headers_ = "HTTP/1.0";
  }
  if (parsed_http_version != http_version_) {
    DVLOG(1) << "assuming HTTP/" << http_version_.major_value() << "."
             << http_version_.minor_value();
  }

  // TODO(eroman): this doesn't make sense if ParseVersion failed.
  std::string::const_iterator p = std::find(line_begin, line_end, ' ');

  if (p == line_end) {
    DVLOG(1) << "missing response status; assuming 200 OK";
    raw_headers_.append(" 200 OK");
    response_code_ = HTTP_OK;
    return;
  }

  response_code_ =
      ParseStatus(base::MakeStringPiece(p + 1, line_end), raw_headers_);
}

size_t HttpResponseHeaders::FindHeader(size_t from,
                                       std::string_view search) const {
  for (size_t i = from; i < parsed_.size(); ++i) {
    if (parsed_[i].is_continuation())
      continue;
    auto name =
        base::MakeStringPiece(parsed_[i].name_begin, parsed_[i].name_end);
    if (base::EqualsCaseInsensitiveASCII(search, name))
      return i;
  }

  return std::string::npos;
}

std::optional<base::TimeDelta> HttpResponseHeaders::GetCacheControlDirective(
    std::string_view directive) const {
  static constexpr std::string_view name("cache-control");
  std::optional<std::string_view> value;

  size_t directive_size = directive.size();

  size_t iter = 0;
  while ((value = EnumerateHeader(&iter, name))) {
    if (!base::StartsWith(*value, directive,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      continue;
    }
    if (value->size() == directive_size || (*value)[directive_size] != '=') {
      continue;
    }
    // 1*DIGIT with leading and trailing spaces, as described at
    // https://datatracker.ietf.org/doc/html/rfc7234#section-1.2.1.
    auto start = value->cbegin() + directive_size + 1;
    auto end = value->cend();
    while (start < end && *start == ' ') {
      // leading spaces
      ++start;
    }
    while (start < end - 1 && *(end - 1) == ' ') {
      // trailing spaces
      --end;
    }
    if (start == end ||
        !std::all_of(start, end, [](char c) { return '0' <= c && c <= '9'; })) {
      continue;
    }
    int64_t seconds = 0;
    base::StringToInt64(base::MakeStringPiece(start, end), &seconds);
    // We ignore the return value because we've already checked the input
    // string. For the overflow case we use
    // base::TimeDelta::FiniteMax().InSeconds().
    seconds = std::min(seconds, base::TimeDelta::FiniteMax().InSeconds());
    return base::Seconds(seconds);
  }

  return std::nullopt;
}

void HttpResponseHeaders::AddHeader(std::string::const_iterator name_begin,
                                    std::string::const_iterator name_end,
                                    std::string::const_iterator values_begin,
                                    std::string::const_iterator values_end,
                                    ContainsCommas contains_commas) {
  // If the header can be coalesced, then we should split it up.
  if (values_begin == values_end ||
      HttpUtil::IsNonCoalescingHeader(
          base::MakeStringPiece(name_begin, name_end)) ||
      contains_commas == ContainsCommas::kNo) {
    AddToParsed(name_begin, name_end, values_begin, values_end);
  } else {
    std::string_view values = base::MakeStringPiece(values_begin, values_end);
    HttpUtil::ValuesIterator it(values, ',', /*ignore_empty_values=*/false);
    while (it.GetNext()) {
      // Convert from a string_view back to a string iterator. To do this,
      // find the offset of the start of `it.value()` relative to to the start
      // of `values`, and add it to to the start of values.
      //
      // TODO(crbug.com/369533090): Converting from a string_view back to a
      // string iterator is awkward. Switch this class to using string_views.
      std::string::const_iterator sub_value_begin =
          values_begin + (it.value().data() - values.data());
      std::string::const_iterator sub_value_end =
          sub_value_begin + it.value().length();

      AddToParsed(name_begin, name_end, sub_value_begin, sub_value_end);
      // clobber these so that subsequent values are treated as continuations
      name_begin = name_end = values_end;
    }
  }
}

void HttpResponseHeaders::AddToParsed(std::string::const_iterator name_begin,
                                      std::string::const_iterator name_end,
                                      std::string::const_iterator value_begin,
                                      std::string::const_iterator value_end) {
  ParsedHeader header;
  header.name_begin = name_begin;
  header.name_end = name_end;
  header.value_begin = value_begin;
  header.value_end = value_end;
  parsed_.push_back(header);
}

void HttpResponseHeaders::AddNonCacheableHeaders(HeaderSet* result) const {
  // Add server specified transients.  Any 'cache-control: no-cache="foo,bar"'
  // headers present in the response specify additional headers that we should
  // not store in the cache.
  const char kCacheControl[] = "cache-control";
  const char kPrefix[] = "no-cache=\"";
  const size_t kPrefixLen = sizeof(kPrefix) - 1;

  std::optional<std::string_view> value;
  size_t iter = 0;
  while ((value = EnumerateHeader(&iter, kCacheControl))) {
    // If the value is smaller than the prefix and a terminal quote, skip
    // it.
    if (value->size() <= kPrefixLen ||
        value->compare(0, kPrefixLen, kPrefix) != 0) {
      continue;
    }
    // if it doesn't end with a quote, then treat as malformed
    if (value->back() != '\"') {
      continue;
    }

    // process the value as a comma-separated list of items. Each
    // item can be wrapped by linear white space.

    // Remove the prefix and close quote.
    std::string_view remaining =
        value->substr(kPrefixLen, value->size() - kPrefixLen - 1);
    // Use base::KEEP_WHITESPACE despite trimming each item so can use the HTTP
    // definition of whitespace.
    std::vector<std::string_view> items = base::SplitStringPiece(
        remaining, /*separators=*/",", base::KEEP_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);
    for (std::string_view item : items) {
      // Trim off leading and trailing whitespace in this item.
      item = HttpUtil::TrimLWS(item);

      // If the header is not empty, lowercase and insert into set.
      if (!item.empty()) {
        result->insert(base::ToLowerASCII(item));
      }
    }
  }
}

void HttpResponseHeaders::AddHopByHopHeaders(HeaderSet* result) {
  for (const auto* header : kHopByHopResponseHeaders)
    result->insert(std::string(header));
}

void HttpResponseHeaders::AddCookieHeaders(HeaderSet* result) {
  for (const auto* header : kCookieResponseHeaders)
    result->insert(std::string(header));
}

void HttpResponseHeaders::AddChallengeHeaders(HeaderSet* result) {
  for (const auto* header : kChallengeResponseHeaders)
    result->insert(std::string(header));
}

void HttpResponseHeaders::AddHopContentRangeHeaders(HeaderSet* result) {
  result->insert(kContentRange);
}

void HttpResponseHeaders::AddSecurityStateHeaders(HeaderSet* result) {
  for (const auto* header : kSecurityStateHeaders)
    result->insert(std::string(header));
}

void HttpResponseHeaders::GetMimeTypeAndCharset(std::string* mime_type,
                                                std::string* charset) const {
  mime_type->clear();
  charset->clear();

  std::optional<std::string_view> value;
  bool had_charset = false;
  size_t iter = 0;
  while ((value = EnumerateHeader(&iter, "content-type"))) {
    HttpUtil::ParseContentType(*value, mime_type, charset, &had_charset,
                               /*boundary=*/nullptr);
  }
}

bool HttpResponseHeaders::GetMimeType(std::string* mime_type) const {
  std::string unused;
  GetMimeTypeAndCharset(mime_type, &unused);
  return !mime_type->empty();
}

bool HttpResponseHeaders::GetCharset(std::string* charset) const {
  std::string unused;
  GetMimeTypeAndCharset(&unused, charset);
  return !charset->empty();
}

bool HttpResponseHeaders::IsRedirect(std::string* location) const {
  if (!IsRedirectResponseCode(response_code_))
    return false;

  // If we lack a Location header, then we can't treat this as a redirect.
  // We assume that the first non-empty location value is the target URL that
  // we want to follow.  TODO(darin): Is this consistent with other browsers?
  size_t i = std::string::npos;
  do {
    i = FindHeader(++i, "location");
    if (i == std::string::npos)
      return false;
    // If the location value is empty, then it doesn't count.
  } while (parsed_[i].value_begin == parsed_[i].value_end);

  if (location) {
    auto location_strpiece =
        base::MakeStringPiece(parsed_[i].value_begin, parsed_[i].value_end);
    // Escape any non-ASCII characters to preserve them.  The server should
    // only be returning ASCII here, but for compat we need to do this.
    //
    // The URL parser escapes things internally, but it expect the bytes to be
    // valid UTF-8, so encoding errors turn into replacement characters before
    // escaping. Escaping here preserves the bytes as-is. See
    // https://crbug.com/942073#c14.
    *location = base::EscapeNonASCII(location_strpiece);
  }

  return true;
}

bool HttpResponseHeaders::HasStorageAccessRetryHeader(
    const std::string* expected_origin) const {
  size_t iter = 0;
  std::optional<std::string_view> header_value;
  while (
      (header_value = EnumerateHeader(&iter, kActivateStorageAccessHeader))) {
    const std::optional<structured_headers::ParameterizedItem> item =
        structured_headers::ParseItem(*header_value);
    if (!item || !item->item.is_token() || item->item.GetString() != "retry") {
      continue;
    }
    if (base::ranges::any_of(
            item->params, [&](const auto& key_and_value) -> bool {
              const auto [key, value] = key_and_value;
              if (key != "allowed-origin") {
                return false;
              }
              if (value.is_token() && value.GetString() == "*") {
                return true;
              }
              return expected_origin && value.is_string() &&
                     value.GetString() == *expected_origin;
            })) {
      return true;
    }
  }
  return false;
}

// static
bool HttpResponseHeaders::IsRedirectResponseCode(int response_code) {
  // Users probably want to see 300 (multiple choice) pages, so we don't count
  // them as redirects that need to be followed.
  return (response_code == HTTP_MOVED_PERMANENTLY ||
          response_code == HTTP_FOUND || response_code == HTTP_SEE_OTHER ||
          response_code == HTTP_TEMPORARY_REDIRECT ||
          response_code == HTTP_PERMANENT_REDIRECT);
}

// From RFC 2616 section 13.2.4:
//
// The calculation to determine if a response has expired is quite simple:
//
//   response_is_fresh = (freshness_lifetime > current_age)
//
// Of course, there are other factors that can force a response to always be
// validated or re-fetched.
//
// From RFC 5861 section 3, a stale response may be used while revalidation is
// performed in the background if
//
//   freshness_lifetime + stale_while_revalidate > current_age
//
ValidationType HttpResponseHeaders::RequiresValidation(
    const Time& request_time,
    const Time& response_time,
    const Time& current_time) const {
  FreshnessLifetimes lifetimes = GetFreshnessLifetimes(response_time);
  if (lifetimes.freshness.is_zero() && lifetimes.staleness.is_zero())
    return VALIDATION_SYNCHRONOUS;

  base::TimeDelta age =
      GetCurrentAge(request_time, response_time, current_time);

  if (lifetimes.freshness > age)
    return VALIDATION_NONE;

  if (lifetimes.freshness + lifetimes.staleness > age)
    return VALIDATION_ASYNCHRONOUS;

  return VALIDATION_SYNCHRONOUS;
}

// From RFC 2616 section 13.2.4:
//
// The max-age directive takes priority over Expires, so if max-age is present
// in a response, the calculation is simply:
//
//   freshness_lifetime = max_age_value
//
// Otherwise, if Expires is present in the response, the calculation is:
//
//   freshness_lifetime = expires_value - date_value
//
// Note that neither of these calculations is vulnerable to clock skew, since
// all of the information comes from the origin server.
//
// Also, if the response does have a Last-Modified time, the heuristic
// expiration value SHOULD be no more than some fraction of the interval since
// that time. A typical setting of this fraction might be 10%:
//
//   freshness_lifetime = (date_value - last_modified_value) * 0.10
//
// If the stale-while-revalidate directive is present, then it is used to set
// the |staleness| time, unless it overridden by another directive.
//
HttpResponseHeaders::FreshnessLifetimes
HttpResponseHeaders::GetFreshnessLifetimes(const Time& response_time) const {
  FreshnessLifetimes lifetimes;
  // Check for headers that force a response to never be fresh.  For backwards
  // compat, we treat "Pragma: no-cache" as a synonym for "Cache-Control:
  // no-cache" even though RFC 2616 does not specify it.
  if (HasHeaderValue("cache-control", "no-cache") ||
      HasHeaderValue("cache-control", "no-store") ||
      HasHeaderValue("pragma", "no-cache")) {
    return lifetimes;
  }

  // Cache-Control directive must_revalidate overrides stale-while-revalidate.
  bool must_revalidate = HasHeaderValue("cache-control", "must-revalidate");

  lifetimes.staleness =
      must_revalidate
          ? base::TimeDelta()
          : GetStaleWhileRevalidateValue().value_or(base::TimeDelta());

  // NOTE: "Cache-Control: max-age" overrides Expires, so we only check the
  // Expires header after checking for max-age in GetFreshnessLifetimes.  This
  // is important since "Expires: <date in the past>" means not fresh, but
  // it should not trump a max-age value.
  std::optional<base::TimeDelta> max_age_value = GetMaxAgeValue();
  if (max_age_value) {
    lifetimes.freshness = max_age_value.value();
    return lifetimes;
  }

  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value = GetDateValue().value_or(response_time);

  std::optional<Time> expires_value = GetExpiresValue();
  if (expires_value) {
    // The expires value can be a date in the past!
    if (expires_value > date_value) {
      lifetimes.freshness = expires_value.value() - date_value;
      return lifetimes;
    }

    DCHECK_EQ(base::TimeDelta(), lifetimes.freshness);
    return lifetimes;
  }

  // From RFC 2616 section 13.4:
  //
  //   A response received with a status code of 200, 203, 206, 300, 301 or 410
  //   MAY be stored by a cache and used in reply to a subsequent request,
  //   subject to the expiration mechanism, unless a cache-control directive
  //   prohibits caching.
  //   ...
  //   A response received with any other status code (e.g. status codes 302
  //   and 307) MUST NOT be returned in a reply to a subsequent request unless
  //   there are cache-control directives or another header(s) that explicitly
  //   allow it.
  //
  // From RFC 2616 section 14.9.4:
  //
  //   When the must-revalidate directive is present in a response received by
  //   a cache, that cache MUST NOT use the entry after it becomes stale to
  //   respond to a subsequent request without first revalidating it with the
  //   origin server. (I.e., the cache MUST do an end-to-end revalidation every
  //   time, if, based solely on the origin server's Expires or max-age value,
  //   the cached response is stale.)
  //
  // https://datatracker.ietf.org/doc/draft-reschke-http-status-308/ is an
  // experimental RFC that adds 308 permanent redirect as well, for which "any
  // future references ... SHOULD use one of the returned URIs."
  if ((response_code_ == HTTP_OK ||
       response_code_ == HTTP_NON_AUTHORITATIVE_INFORMATION ||
       response_code_ == HTTP_PARTIAL_CONTENT) &&
      !must_revalidate) {
    // TODO(darin): Implement a smarter heuristic.
    std::optional<Time> last_modified_value = GetLastModifiedValue();
    if (last_modified_value) {
      // The last-modified value can be a date in the future!
      if (last_modified_value.value() <= date_value) {
        lifetimes.freshness = (date_value - last_modified_value.value()) / 10;
        return lifetimes;
      }
    }
  }

  // These responses are implicitly fresh (unless otherwise overruled):
  if (response_code_ == HTTP_MULTIPLE_CHOICES ||
      response_code_ == HTTP_MOVED_PERMANENTLY ||
      response_code_ == HTTP_PERMANENT_REDIRECT ||
      response_code_ == HTTP_GONE) {
    lifetimes.freshness = base::TimeDelta::Max();
    lifetimes.staleness = base::TimeDelta();  // It should never be stale.
    return lifetimes;
  }

  // Our heuristic freshness estimate for this resource is 0 seconds, in
  // accordance with common browser behaviour. However, stale-while-revalidate
  // may still apply.
  DCHECK_EQ(base::TimeDelta(), lifetimes.freshness);
  return lifetimes;
}

// From RFC 7234 section 4.2.3:
//
// The following data is used for the age calculation:
//
//    age_value
//
//       The term "age_value" denotes the value of the Age header field
//       (Section 5.1), in a form appropriate for arithmetic operation; or
//       0, if not available.
//
//    date_value
//
//       The term "date_value" denotes the value of the Date header field,
//       in a form appropriate for arithmetic operations.  See Section
//       7.1.1.2 of [RFC7231] for the definition of the Date header field,
//       and for requirements regarding responses without it.
//
//    now
//
//       The term "now" means "the current value of the clock at the host
//       performing the calculation".  A host ought to use NTP ([RFC5905])
//       or some similar protocol to synchronize its clocks to Coordinated
//       Universal Time.
//
//    request_time
//
//       The current value of the clock at the host at the time the request
//       resulting in the stored response was made.
//
//    response_time
//
//       The current value of the clock at the host at the time the
//       response was received.
//
//    The age is then calculated as
//
//     apparent_age = max(0, response_time - date_value);
//     response_delay = response_time - request_time;
//     corrected_age_value = age_value + response_delay;
//     corrected_initial_age = max(apparent_age, corrected_age_value);
//     resident_time = now - response_time;
//     current_age = corrected_initial_age + resident_time;
//
base::TimeDelta HttpResponseHeaders::GetCurrentAge(
    const Time& request_time,
    const Time& response_time,
    const Time& current_time) const {
  // If there is no Date header, then assume that the server response was
  // generated at the time when we received the response.
  Time date_value = GetDateValue().value_or(response_time);

  // If there is no Age header, then assume age is zero.
  base::TimeDelta age_value = GetAgeValue().value_or(base::TimeDelta());

  base::TimeDelta apparent_age =
      std::max(base::TimeDelta(), response_time - date_value);
  base::TimeDelta response_delay = response_time - request_time;
  base::TimeDelta corrected_age_value = age_value + response_delay;
  base::TimeDelta corrected_initial_age =
      std::max(apparent_age, corrected_age_value);
  base::TimeDelta resident_time = current_time - response_time;
  base::TimeDelta current_age = corrected_initial_age + resident_time;

  return current_age;
}

std::optional<base::TimeDelta> HttpResponseHeaders::GetMaxAgeValue() const {
  return GetCacheControlDirective("max-age");
}

std::optional<base::TimeDelta> HttpResponseHeaders::GetAgeValue() const {
  std::optional<std::string> value;
  if (!(value = EnumerateHeader(nullptr, "Age"))) {
    return std::nullopt;
  }

  // Parse the delta-seconds as 1*DIGIT.
  uint32_t seconds;
  ParseIntError error;
  if (!ParseUint32(*value, ParseIntFormat::NON_NEGATIVE, &seconds, &error)) {
    if (error == ParseIntError::FAILED_OVERFLOW) {
      // If the Age value cannot fit in a uint32_t, saturate it to a maximum
      // value. This is similar to what RFC 2616 says in section 14.6 for how
      // caches should transmit values that overflow.
      seconds = std::numeric_limits<decltype(seconds)>::max();
    } else {
      return std::nullopt;
    }
  }

  return base::Seconds(seconds);
}

std::optional<Time> HttpResponseHeaders::GetDateValue() const {
  return GetTimeValuedHeader("Date");
}

std::optional<Time> HttpResponseHeaders::GetLastModifiedValue() const {
  return GetTimeValuedHeader("Last-Modified");
}

std::optional<Time> HttpResponseHeaders::GetExpiresValue() const {
  return GetTimeValuedHeader("Expires");
}

std::optional<base::TimeDelta>
HttpResponseHeaders::GetStaleWhileRevalidateValue() const {
  return GetCacheControlDirective("stale-while-revalidate");
}

std::optional<Time> HttpResponseHeaders::GetTimeValuedHeader(
    const std::string& name) const {
  std::optional<std::string_view> value;
  if (!(value = EnumerateHeader(nullptr, name))) {
    return std::nullopt;
  }

  // In case of parsing the Expires header value, an invalid string 0 should be
  // treated as expired according to the RFC 9111 section 5.3 as below:
  //
  // > A cache recipient MUST interpret invalid date formats, especially the
  // > value "0", as representing a time in the past (i.e., "already expired").
  if (base::FeatureList::IsEnabled(
          features::kTreatHTTPExpiresHeaderValueZeroAsExpired) &&
      name == "Expires" && *value == "0") {
    return Time::Min();
  }

  // When parsing HTTP dates it's beneficial to default to GMT because:
  // 1. RFC2616 3.3.1 says times should always be specified in GMT
  // 2. Only counter-example incorrectly appended "UTC" (crbug.com/153759)
  // 3. When adjusting cookie expiration times for clock skew
  //    (crbug.com/135131) this better matches our cookie expiration
  //    time parser which ignores timezone specifiers and assumes GMT.
  // 4. This is exactly what Firefox does.
  // TODO(pauljensen): The ideal solution would be to return std::nullopt if the
  // timezone could not be understood so as to avoid making other calculations
  // based on an incorrect time.  This would require modifying the time
  // library or duplicating the code. (http://crbug.com/158327)
  Time result;
  return Time::FromUTCString(std::string(*value).c_str(), &result)
             ? std::make_optional(result)
             : std::nullopt;
}

// We accept the first value of "close" or "keep-alive" in a Connection or
// Proxy-Connection header, in that order. Obeying "keep-alive" in HTTP/1.1 or
// "close" in 1.0 is not strictly standards-compliant, but we'd like to
// avoid looking at the Proxy-Connection header whenever it is reasonable to do
// so.
// TODO(ricea): Measure real-world usage of the "Proxy-Connection" header,
// with a view to reducing support for it in order to make our Connection header
// handling more RFC 7230 compliant.
bool HttpResponseHeaders::IsKeepAlive() const {
  // NOTE: It is perhaps risky to assume that a Proxy-Connection header is
  // meaningful when we don't know that this response was from a proxy, but
  // Mozilla also does this, so we'll do the same.
  static const char* const kConnectionHeaders[] = {"connection",
                                                   "proxy-connection"};
  struct KeepAliveToken {
    const char* const token;
    bool keep_alive;
  };
  static const KeepAliveToken kKeepAliveTokens[] = {{"keep-alive", true},
                                                    {"close", false}};

  if (http_version_ < HttpVersion(1, 0))
    return false;

  for (const char* header : kConnectionHeaders) {
    size_t iterator = 0;
    std::optional<std::string_view> token;
    while ((token = EnumerateHeader(&iterator, header))) {
      for (const KeepAliveToken& keep_alive_token : kKeepAliveTokens) {
        if (base::EqualsCaseInsensitiveASCII(*token, keep_alive_token.token)) {
          return keep_alive_token.keep_alive;
        }
      }
    }
  }
  return http_version_ != HttpVersion(1, 0);
}

bool HttpResponseHeaders::HasStrongValidators() const {
  return HttpUtil::HasStrongValidators(
      GetHttpVersion(), EnumerateHeader(nullptr, "etag"),
      EnumerateHeader(nullptr, "Last-Modified"),
      EnumerateHeader(nullptr, "Date"));
}

bool HttpResponseHeaders::HasValidators() const {
  return HttpUtil::HasValidators(GetHttpVersion(),
                                 EnumerateHeader(nullptr, "etag"),
                                 EnumerateHeader(nullptr, "Last-Modified"));
}

// From RFC 2616:
// Content-Length = "Content-Length" ":" 1*DIGIT
int64_t HttpResponseHeaders::GetContentLength() const {
  return GetInt64HeaderValue("content-length");
}

int64_t HttpResponseHeaders::GetInt64HeaderValue(
    const std::string& header) const {
  size_t iter = 0;
  std::optional<std::string_view> content_length =
      EnumerateHeader(&iter, header);
  if (!content_length || content_length->empty()) {
    return -1;
  }

  if ((*content_length)[0] == '+') {
    return -1;
  }

  int64_t result;
  bool ok = base::StringToInt64(*content_length, &result);
  if (!ok || result < 0) {
    return -1;
  }

  return result;
}

bool HttpResponseHeaders::GetContentRangeFor206(
    int64_t* first_byte_position,
    int64_t* last_byte_position,
    int64_t* instance_length) const {
  size_t iter = 0;
  std::optional<std::string_view> content_range =
      EnumerateHeader(&iter, kContentRange);
  if (!content_range) {
    *first_byte_position = *last_byte_position = *instance_length = -1;
    return false;
  }

  return HttpUtil::ParseContentRangeHeaderFor206(
      *content_range, first_byte_position, last_byte_position, instance_length);
}

base::Value::Dict HttpResponseHeaders::NetLogParams(
    NetLogCaptureMode capture_mode) const {
  base::Value::Dict dict;
  base::Value::List headers;
  headers.Append(NetLogStringValue(GetStatusLine()));
  size_t iterator = 0;
  std::string name;
  std::string value;
  while (EnumerateHeaderLines(&iterator, &name, &value)) {
    std::string log_value =
        ElideHeaderValueForNetLog(capture_mode, name, value);
    headers.Append(NetLogStringValue(base::StrCat({name, ": ", log_value})));
  }
  dict.Set("headers", std::move(headers));
  return dict;
}

bool HttpResponseHeaders::IsChunkEncoded() const {
  // Ignore spurious chunked responses from HTTP/1.0 servers and proxies.
  return GetHttpVersion() >= HttpVersion(1, 1) &&
         HasHeaderValue("Transfer-Encoding", "chunked");
}

bool HttpResponseHeaders::IsCookieResponseHeader(std::string_view name) {
  for (const char* cookie_header : kCookieResponseHeaders) {
    if (base::EqualsCaseInsensitiveASCII(cookie_header, name))
      return true;
  }
  return false;
}

void HttpResponseHeaders::WriteIntoTrace(perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
  dict.Add("response_code", response_code_);
  dict.Add("headers", parsed_);
}

bool HttpResponseHeaders::StrictlyEquals(
    const HttpResponseHeaders& other) const {
  if (http_version_ != other.http_version_ ||
      response_code_ != other.response_code_ ||
      raw_headers_ != other.raw_headers_ ||
      parsed_.size() != other.parsed_.size()) {
    return false;
  }

  auto offsets_match = [&](std::string::const_iterator this_offset,
                           std::string::const_iterator other_offset) {
    return this_offset - raw_headers_.begin() ==
           other_offset - other.raw_headers_.begin();
  };
  return std::mismatch(parsed_.begin(), parsed_.end(), other.parsed_.begin(),
                       [&](const ParsedHeader& lhs, const ParsedHeader& rhs) {
                         return offsets_match(lhs.name_begin, rhs.name_begin) &&
                                offsets_match(lhs.name_end, rhs.name_end) &&
                                offsets_match(lhs.value_begin,
                                              rhs.value_begin) &&
                                offsets_match(lhs.value_end, rhs.value_end);
                       }) == std::pair(parsed_.end(), other.parsed_.end());
}

}  // namespace net
