/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/network/http_parsers.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/no_vary_search_header_parser.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/no_vary_search.mojom-blink-forward.h"
#include "services/network/public/mojom/no_vary_search.mojom-blink.h"
#include "services/network/public/mojom/parsed_headers.mojom-blink.h"
#include "services/network/public/mojom/supports_loading_mode.mojom-blink.h"
#include "services/network/public/mojom/timing_allow_origin.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/header_field_tokenizer.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

// We would like finding a way to convert from/to blink type automatically.
// The following attempt has been withdrawn:
// https://chromium-review.googlesource.com/c/chromium/src/+/2126933/7
//
// Note: nesting these helpers inside network::mojom bypasses warnings from
// audit_non_blink_style.py, as well as saving a bunch of typing to qualify the
// types below.
namespace network {
namespace mojom {

// When adding a new conversion, define a new `ConvertToBlink` overload to map
// the non-Blink type (passing by value for primitive types or passing by const
// reference otherwise). The generic converters for container types relies on
// the presence of `ConvertToBlink` overloads to determine the correct return
// type.

// ===== Identity converters =====
// Converts where the input type and output type are identical(-ish).
uint8_t ConvertToBlink(uint8_t in) {
  return in;
}

// Note: for identity enum conversions, there should be `static_assert`s that
// the input enumerator and the output enumerator define matching values.
blink::CSPDirectiveName ConvertToBlink(CSPDirectiveName name) {
  return static_cast<blink::CSPDirectiveName>(name);
}

// `in` is a Mojo enum type, which is type aliased to the same underlying type
// by both the non-Blink Mojo variant and the Blink Mojo variant.
blink::WebClientHintsType ConvertToBlink(WebClientHintsType in) {
  return in;
}

blink::LoadingMode ConvertToBlink(LoadingMode in) {
  return static_cast<blink::LoadingMode>(in);
}

// ===== Converters for other basic Blink types =====
String ConvertToBlink(const std::string& in) {
  return String::FromUTF8(in);
}

String ConvertToBlink(const std::optional<std::string>& in) {
  return in ? String::FromUTF8(*in) : String();
}

::blink::KURL ConvertToBlink(const GURL& in) {
  return ::blink::KURL(in);
}

scoped_refptr<const ::blink::SecurityOrigin> ConvertToBlink(
    const url::Origin& in) {
  return ::blink::SecurityOrigin::CreateFromUrlOrigin(in);
}

// ====== Generic container converters =====
template <
    typename InElement,
    typename OutElement = decltype(ConvertToBlink(std::declval<InElement>()))>
Vector<OutElement> ConvertToBlink(const std::vector<InElement>& in) {
  Vector<OutElement> out;
  out.reserve(base::checked_cast<wtf_size_t>(in.size()));
  for (const auto& element : in) {
    out.push_back(ConvertToBlink(element));
  }
  return out;
}

template <typename InKey,
          typename InValue,
          typename OutKey = decltype(ConvertToBlink(std::declval<InKey>())),
          typename OutValue = decltype(ConvertToBlink(std::declval<InValue>()))>
HashMap<OutKey, OutValue> ConvertToBlink(
    const base::flat_map<InKey, InValue>& in) {
  HashMap<OutKey, OutValue> out;
  for (const auto& element : in) {
    out.insert(ConvertToBlink(element.first), ConvertToBlink(element.second));
  }
  return out;
}

// ===== Converters from non-Blink to Blink variant of Mojo structs =====
blink::CSPSourcePtr ConvertToBlink(const CSPSourcePtr& in) {
  DCHECK(in);
  return blink::CSPSource::New(
      ConvertToBlink(in->scheme), ConvertToBlink(in->host), in->port,
      ConvertToBlink(in->path), in->is_host_wildcard, in->is_port_wildcard);
}

blink::CSPHashSourcePtr ConvertToBlink(const CSPHashSourcePtr& in) {
  DCHECK(in);
  Vector<uint8_t> hash_value = ConvertToBlink(in->value);

  return blink::CSPHashSource::New(in->algorithm, std::move(hash_value));
}

blink::CSPSourceListPtr ConvertToBlink(const CSPSourceListPtr& source_list) {
  DCHECK(source_list);

  Vector<blink::CSPSourcePtr> sources = ConvertToBlink(source_list->sources);
  Vector<String> nonces = ConvertToBlink(source_list->nonces);
  Vector<blink::CSPHashSourcePtr> hashes = ConvertToBlink(source_list->hashes);

  return blink::CSPSourceList::New(
      std::move(sources), std::move(nonces), std::move(hashes),
      source_list->allow_self, source_list->allow_star,
      source_list->allow_inline, source_list->allow_inline_speculation_rules,
      source_list->allow_eval, source_list->allow_wasm_eval,
      source_list->allow_wasm_unsafe_eval, source_list->allow_dynamic,
      source_list->allow_unsafe_hashes, source_list->report_sample);
}

blink::ContentSecurityPolicyHeaderPtr ConvertToBlink(
    const ContentSecurityPolicyHeaderPtr& in) {
  DCHECK(in);
  return blink::ContentSecurityPolicyHeader::New(
      ConvertToBlink(in->header_value), in->type, in->source);
}

blink::CSPTrustedTypesPtr ConvertToBlink(const CSPTrustedTypesPtr& in) {
  if (!in)
    return nullptr;
  return blink::CSPTrustedTypes::New(ConvertToBlink(in->list), in->allow_any,
                                     in->allow_duplicates);
}

blink::ContentSecurityPolicyPtr ConvertToBlink(
    const ContentSecurityPolicyPtr& in) {
  DCHECK(in);
  return blink::ContentSecurityPolicy::New(
      ConvertToBlink(in->self_origin), ConvertToBlink(in->raw_directives),
      ConvertToBlink(in->directives), in->upgrade_insecure_requests,
      in->treat_as_public_address, in->block_all_mixed_content, in->sandbox,
      ConvertToBlink(in->header), in->use_reporting_api,
      ConvertToBlink(in->report_endpoints), in->require_trusted_types_for,
      ConvertToBlink(in->trusted_types), ConvertToBlink(in->parsing_errors));
}

blink::AllowCSPFromHeaderValuePtr ConvertToBlink(
    const AllowCSPFromHeaderValuePtr& allow_csp_from) {
  if (!allow_csp_from)
    return nullptr;
  switch (allow_csp_from->which()) {
    case AllowCSPFromHeaderValue::Tag::kAllowStar:
      return blink::AllowCSPFromHeaderValue::NewAllowStar(
          allow_csp_from->get_allow_star());
    case AllowCSPFromHeaderValue::Tag::kOrigin:
      return blink::AllowCSPFromHeaderValue::NewOrigin(
          ConvertToBlink(allow_csp_from->get_origin()));
    case AllowCSPFromHeaderValue::Tag::kErrorMessage:
      return blink::AllowCSPFromHeaderValue::NewErrorMessage(
          ConvertToBlink(allow_csp_from->get_error_message()));
  }
}

blink::LinkHeaderPtr ConvertToBlink(const LinkHeaderPtr& in) {
  DCHECK(in);
  return blink::LinkHeader::New(
      ConvertToBlink(in->href),
      // TODO(dcheng): Make these use ConvertToBlink
      static_cast<blink::LinkRelAttribute>(in->rel),
      static_cast<blink::LinkAsAttribute>(in->as),
      static_cast<blink::CrossOriginAttribute>(in->cross_origin),
      static_cast<blink::FetchPriorityAttribute>(in->fetch_priority),
      ConvertToBlink(in->mime_type));
}

blink::TimingAllowOriginPtr ConvertToBlink(const TimingAllowOriginPtr& in) {
  if (!in) {
    return nullptr;
  }

  switch (in->which()) {
    case TimingAllowOrigin::Tag::kSerializedOrigins:
      return blink::TimingAllowOrigin::NewSerializedOrigins(
          ConvertToBlink(in->get_serialized_origins()));
    case TimingAllowOrigin::Tag::kAll:
      return blink::TimingAllowOrigin::NewAll(/*ignored=*/0);
  }
}

blink::NoVarySearchWithParseErrorPtr ConvertToBlink(
    const NoVarySearchWithParseErrorPtr& in) {
  if (!in)
    return nullptr;

  if (in->is_parse_error()) {
    return blink::NoVarySearchWithParseError::NewParseError(
        in->get_parse_error());
  }

  const NoVarySearchPtr& no_vary_search = in->get_no_vary_search();
  CHECK(no_vary_search);
  CHECK(no_vary_search->search_variance);
  if (no_vary_search->search_variance->is_no_vary_params()) {
    return blink::NoVarySearchWithParseError::NewNoVarySearch(
        blink::NoVarySearch::New(
            blink::SearchParamsVariance::NewNoVaryParams(ConvertToBlink(
                no_vary_search->search_variance->get_no_vary_params())),
            no_vary_search->vary_on_key_order));
  }

  CHECK(no_vary_search->search_variance->is_vary_params());
  return blink::NoVarySearchWithParseError::NewNoVarySearch(
      blink::NoVarySearch::New(
          blink::SearchParamsVariance::NewVaryParams(ConvertToBlink(
              no_vary_search->search_variance->get_vary_params())),
          no_vary_search->vary_on_key_order));
}

blink::ParsedHeadersPtr ConvertToBlink(const ParsedHeadersPtr& in) {
  DCHECK(in);
  return blink::ParsedHeaders::New(
      ConvertToBlink(in->content_security_policy),
      ConvertToBlink(in->allow_csp_from), in->cross_origin_embedder_policy,
      in->cross_origin_opener_policy, in->document_isolation_policy,
      in->origin_agent_cluster,
      in->accept_ch.has_value()
          ? std::make_optional(ConvertToBlink(in->accept_ch.value()))
          : std::nullopt,
      in->critical_ch.has_value()
          ? std::make_optional(ConvertToBlink(in->critical_ch.value()))
          : std::nullopt,
      in->client_hints_ignored_due_to_clear_site_data_header, in->xfo,
      ConvertToBlink(in->link_headers), ConvertToBlink(in->timing_allow_origin),
      ConvertToBlink(in->supports_loading_mode),
      in->reporting_endpoints.has_value()
          ? std::make_optional(ConvertToBlink(in->reporting_endpoints.value()))
          : std::nullopt,
      in->cookie_indices.has_value()
          ? std::make_optional(ConvertToBlink(in->cookie_indices.value()))
          : std::nullopt,
      in->avail_language.has_value()
          ? std::make_optional(ConvertToBlink(in->avail_language.value()))
          : std::nullopt,
      in->content_language.has_value()
          ? std::make_optional(ConvertToBlink(in->content_language.value()))
          : std::nullopt,
      ConvertToBlink(in->no_vary_search_with_parse_error),
      in->observe_browsing_topics, in->allow_cross_origin_event_reporting);
}

}  // namespace mojom
}  // namespace network

namespace blink {

namespace {

const Vector<AtomicString>& ReplaceHeaders() {
  // The list of response headers that we do not copy from the original
  // response when generating a ResourceResponse for a MIME payload.
  // Note: this is called only on the main thread.
  DEFINE_STATIC_LOCAL(
      Vector<AtomicString>, headers,
      ({http_names::kLowerContentType, http_names::kLowerContentLength,
        http_names::kLowerContentDisposition, http_names::kLowerContentRange,
        http_names::kLowerRange, http_names::kLowerSetCookie}));
  return headers;
}

bool IsWhitespace(UChar chr) {
  return (chr == ' ') || (chr == '\t');
}

// true if there is more to parse, after incrementing pos past whitespace.
// Note: Might return pos == str.length()
// if |matcher| is nullptr, isWhitespace() is used.
inline bool SkipWhiteSpace(const String& str,
                           unsigned& pos,
                           WTF::CharacterMatchFunctionPtr matcher = nullptr) {
  unsigned len = str.length();

  if (matcher) {
    while (pos < len && matcher(str[pos]))
      ++pos;
  } else {
    while (pos < len && IsWhitespace(str[pos]))
      ++pos;
  }

  return pos < len;
}

template <typename CharType>
inline bool IsASCIILowerAlphaOrDigit(CharType c) {
  return IsASCIILower(c) || IsASCIIDigit(c);
}

template <typename CharType>
inline bool IsASCIILowerAlphaOrDigitOrHyphen(CharType c) {
  return IsASCIILowerAlphaOrDigit(c) || c == '-';
}

// Parse a number with ignoring trailing [0-9.].
// Returns false if the source contains invalid characters.
bool ParseRefreshTime(const String& source, base::TimeDelta& delay) {
  int full_stop_count = 0;
  unsigned number_end = source.length();
  for (unsigned i = 0; i < source.length(); ++i) {
    UChar ch = source[i];
    if (ch == kFullstopCharacter) {
      if (++full_stop_count == 2)
        number_end = i;
    } else if (!IsASCIIDigit(ch)) {
      return false;
    }
  }
  bool ok;
  double time = source.Left(number_end).ToDouble(&ok);
  if (RuntimeEnabledFeatures::MetaRefreshNoFractionalEnabled()) {
    time = floor(time);
  }
  if (!ok)
    return false;
  delay = base::Seconds(time);
  return true;
}

}  // namespace

bool IsValidHTTPHeaderValue(const String& name) {
  // FIXME: This should really match name against
  // field-value in section 4.2 of RFC 2616.

  return name.ContainsOnlyLatin1OrEmpty() && !name.Contains('\r') &&
         !name.Contains('\n') && !name.Contains('\0');
}

// See RFC 7230, Section 3.2.6.
bool IsValidHTTPToken(const String& characters) {
  if (characters.empty())
    return false;
  for (unsigned i = 0; i < characters.length(); ++i) {
    UChar c = characters[i];
    if (c > 0x7F || !net::HttpUtil::IsTokenChar(c))
      return false;
  }
  return true;
}

bool IsContentDispositionAttachment(const String& content_disposition) {
  return net::HttpContentDisposition(content_disposition.Utf8(), std::string())
      .is_attachment();
}

// https://html.spec.whatwg.org/C/#attr-meta-http-equiv-refresh
bool ParseHTTPRefresh(const String& refresh,
                      WTF::CharacterMatchFunctionPtr matcher,
                      base::TimeDelta& delay,
                      String& url) {
  unsigned len = refresh.length();
  unsigned pos = 0;
  matcher = matcher ? matcher : IsWhitespace;

  if (!SkipWhiteSpace(refresh, pos, matcher))
    return false;

  while (pos != len && refresh[pos] != ',' && refresh[pos] != ';' &&
         !matcher(refresh[pos]))
    ++pos;

  if (pos == len) {  // no URL
    url = String();
    return ParseRefreshTime(refresh.StripWhiteSpace(), delay);
  } else {
    if (!ParseRefreshTime(refresh.Left(pos).StripWhiteSpace(), delay))
      return false;

    SkipWhiteSpace(refresh, pos, matcher);
    if (pos < len && (refresh[pos] == ',' || refresh[pos] == ';'))
      ++pos;
    SkipWhiteSpace(refresh, pos, matcher);
    unsigned url_start_pos = pos;
    if (refresh.FindIgnoringASCIICase("url", url_start_pos) == url_start_pos) {
      url_start_pos += 3;
      SkipWhiteSpace(refresh, url_start_pos, matcher);
      if (refresh[url_start_pos] == '=') {
        ++url_start_pos;
        SkipWhiteSpace(refresh, url_start_pos, matcher);
      } else {
        url_start_pos = pos;  // e.g. "Refresh: 0; url.html"
      }
    }

    unsigned url_end_pos = len;

    if (refresh[url_start_pos] == '"' || refresh[url_start_pos] == '\'') {
      UChar quotation_mark = refresh[url_start_pos];
      url_start_pos++;
      while (url_end_pos > url_start_pos) {
        url_end_pos--;
        if (refresh[url_end_pos] == quotation_mark)
          break;
      }

      // https://bugs.webkit.org/show_bug.cgi?id=27868
      // Sometimes there is no closing quote for the end of the URL even though
      // there was an opening quote.  If we looped over the entire alleged URL
      // string back to the opening quote, just go ahead and use everything
      // after the opening quote instead.
      if (url_end_pos == url_start_pos)
        url_end_pos = len;
    }

    url = refresh.Substring(url_start_pos, url_end_pos - url_start_pos)
              .StripWhiteSpace();
    return true;
  }
}

std::optional<base::Time> ParseDate(const String& value) {
  return ParseDateFromNullTerminatedCharacters(value.Utf8().c_str());
}

AtomicString ExtractMIMETypeFromMediaType(const AtomicString& media_type) {
  unsigned length = media_type.length();

  unsigned pos = 0;

  while (pos < length) {
    UChar c = media_type[pos];
    if (c != '\t' && c != ' ')
      break;
    ++pos;
  }

  if (pos == length)
    return media_type;

  unsigned type_start = pos;

  unsigned type_end = pos;
  while (pos < length) {
    UChar c = media_type[pos];

    // While RFC 2616 does not allow it, other browsers allow multiple values in
    // the HTTP media type header field, Content-Type. In such cases, the media
    // type string passed here may contain the multiple values separated by
    // commas. For now, this code ignores text after the first comma, which
    // prevents it from simply failing to parse such types altogether.  Later
    // for better compatibility we could consider using the first or last valid
    // MIME type instead.
    // See https://bugs.webkit.org/show_bug.cgi?id=25352 for more discussion.
    if (c == ',' || c == ';')
      break;

    if (c != '\t' && c != ' ')
      type_end = pos + 1;

    ++pos;
  }

  // Use a StringView to create an AtomicString here so we do not allocate an
  // intermediate string.
  return AtomicString(
      StringView(media_type, type_start, type_end - type_start));
}

bool IsHTTPTabOrSpace(UChar c) {
  // https://fetch.spec.whatwg.org/#http-tab-or-space
  return c == kSpaceCharacter || c == kTabulationCharacter;
}

// https://mimesniff.spec.whatwg.org/#minimize-a-supported-mime-type
// Note that `mime_type` should already have been stripped of parameters by
// `ExtractMIMETypeFromMediaType`.
AtomicString MinimizedMIMEType(const AtomicString& mime_type) {
  StringUTF8Adaptor mime_utf8(mime_type);

  if (IsSupportedJavascriptMimeType(mime_utf8.AsStringView())) {
    return AtomicString("text/javascript");
  }

  if (IsJSONMimeType(mime_utf8.AsStringView())) {
    return AtomicString("application/json");
  }

  if (IsSVGMimeType(mime_utf8.AsStringView())) {
    return AtomicString("image/svg+xml");
  }

  if (IsXMLMimeType(mime_utf8.AsStringView())) {
    return AtomicString("application/xml");
  }

  if (IsSupportedMimeType(mime_utf8.AsStringView())) {
    return mime_type;
  }

  return g_empty_atom;
}

ContentTypeOptionsDisposition ParseContentTypeOptionsHeader(
    const String& value) {
  // The spec prescribes how to split the header value, and wants to include
  // empty entries and to strip only particular type of whitespace.
  // Spec: https://fetch.spec.whatwg.org/#x-content-type-options-header
  // Test: external/wpt/fetch/nosniff/parsing-nosniff.window.html

  if (value.empty())
    return kContentTypeOptionsNone;

  String decoded_and_split_header_value;
  if (base::FeatureList::IsEnabled(
          features::kLegacyParsingOfXContentTypeOptions)) {
    // Header parsing, as used until M120.
    Vector<String> results;
    value.Split(",", results);
    if (results.size()) {
      decoded_and_split_header_value = results[0].StripWhiteSpace();
    }
  } else {
    // Header parsing, as demanded by the spec.
    Vector<String> results;
    value.Split(",", /* allow_empty_entries */ true, results);
    CHECK(results.size());  // allow_empty_entries guarantees >= 1 results.
    decoded_and_split_header_value =
        results[0].StripWhiteSpace(IsHTTPTabOrSpace);
  }

  if (EqualIgnoringASCIICase(decoded_and_split_header_value, "nosniff")) {
    return kContentTypeOptionsNosniff;
  }
  return kContentTypeOptionsNone;
}

static bool IsCacheHeaderSeparator(UChar c) {
  // See RFC 2616, Section 2.2
  switch (c) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
      return true;
    default:
      return false;
  }
}

static bool IsControlCharacter(UChar c) {
  return c < ' ' || c == 127;
}

static inline String TrimToNextSeparator(const String& str) {
  return str.Substring(0, str.Find(IsCacheHeaderSeparator));
}

static void ParseCacheHeader(const String& header,
                             Vector<std::pair<String, String>>& result) {
  const String safe_header = header.RemoveCharacters(IsControlCharacter);
  wtf_size_t max = safe_header.length();
  for (wtf_size_t pos = 0; pos < max; /* pos incremented in loop */) {
    wtf_size_t next_comma_position = safe_header.find(',', pos);
    wtf_size_t next_equal_sign_position = safe_header.find('=', pos);
    if (next_equal_sign_position != kNotFound &&
        (next_equal_sign_position < next_comma_position ||
         next_comma_position == kNotFound)) {
      // Get directive name, parse right hand side of equal sign, then add to
      // map
      String directive = TrimToNextSeparator(
          safe_header.Substring(pos, next_equal_sign_position - pos)
              .StripWhiteSpace());
      pos += next_equal_sign_position - pos + 1;

      String value = safe_header.Substring(pos, max - pos).StripWhiteSpace();
      if (value[0] == '"') {
        // The value is a quoted string
        wtf_size_t next_double_quote_position = value.find('"', 1);
        if (next_double_quote_position != kNotFound) {
          // Store the value as a quoted string without quotes
          result.push_back(std::pair<String, String>(
              directive, value.Substring(1, next_double_quote_position - 1)
                             .StripWhiteSpace()));
          pos += (safe_header.find('"', pos) - pos) +
                 next_double_quote_position + 1;
          // Move past next comma, if there is one
          wtf_size_t next_comma_position2 = safe_header.find(',', pos);
          if (next_comma_position2 != kNotFound)
            pos += next_comma_position2 - pos + 1;
          else
            return;  // Parse error if there is anything left with no comma
        } else {
          // Parse error; just use the rest as the value
          result.push_back(std::pair<String, String>(
              directive,
              TrimToNextSeparator(
                  value.Substring(1, value.length() - 1).StripWhiteSpace())));
          return;
        }
      } else {
        // The value is a token until the next comma
        wtf_size_t next_comma_position2 = value.find(',');
        if (next_comma_position2 != kNotFound) {
          // The value is delimited by the next comma
          result.push_back(std::pair<String, String>(
              directive,
              TrimToNextSeparator(
                  value.Substring(0, next_comma_position2).StripWhiteSpace())));
          pos += (safe_header.find(',', pos) - pos) + 1;
        } else {
          // The rest is the value; no change to value needed
          result.push_back(
              std::pair<String, String>(directive, TrimToNextSeparator(value)));
          return;
        }
      }
    } else if (next_comma_position != kNotFound &&
               (next_comma_position < next_equal_sign_position ||
                next_equal_sign_position == kNotFound)) {
      // Add directive to map with empty string as value
      result.push_back(std::pair<String, String>(
          TrimToNextSeparator(
              safe_header.Substring(pos, next_comma_position - pos)
                  .StripWhiteSpace()),
          ""));
      pos += next_comma_position - pos + 1;
    } else {
      // Add last directive to map with empty string as value
      result.push_back(std::pair<String, String>(
          TrimToNextSeparator(
              safe_header.Substring(pos, max - pos).StripWhiteSpace()),
          ""));
      return;
    }
  }
}

CacheControlHeader ParseCacheControlDirectives(
    const AtomicString& cache_control_value,
    const AtomicString& pragma_value) {
  CacheControlHeader cache_control_header;
  cache_control_header.parsed = true;
  cache_control_header.max_age = std::nullopt;
  cache_control_header.stale_while_revalidate = std::nullopt;

  static const char kNoCacheDirective[] = "no-cache";
  static const char kNoStoreDirective[] = "no-store";
  static const char kMustRevalidateDirective[] = "must-revalidate";
  static const char kMaxAgeDirective[] = "max-age";
  static const char kStaleWhileRevalidateDirective[] = "stale-while-revalidate";

  if (!cache_control_value.empty()) {
    Vector<std::pair<String, String>> directives;
    ParseCacheHeader(cache_control_value, directives);

    wtf_size_t directives_size = directives.size();
    for (wtf_size_t i = 0; i < directives_size; ++i) {
      // RFC2616 14.9.1: A no-cache directive with a value is only meaningful
      // for proxy caches.  It should be ignored by a browser level cache.
      if (EqualIgnoringASCIICase(directives[i].first, kNoCacheDirective) &&
          directives[i].second.empty()) {
        cache_control_header.contains_no_cache = true;
      } else if (EqualIgnoringASCIICase(directives[i].first,
                                        kNoStoreDirective)) {
        cache_control_header.contains_no_store = true;
      } else if (EqualIgnoringASCIICase(directives[i].first,
                                        kMustRevalidateDirective)) {
        cache_control_header.contains_must_revalidate = true;
      } else if (EqualIgnoringASCIICase(directives[i].first,
                                        kMaxAgeDirective)) {
        if (cache_control_header.max_age) {
          // First max-age directive wins if there are multiple ones.
          continue;
        }
        bool ok;
        double max_age = directives[i].second.ToDouble(&ok);
        if (ok)
          cache_control_header.max_age = base::Seconds(max_age);
      } else if (EqualIgnoringASCIICase(directives[i].first,
                                        kStaleWhileRevalidateDirective)) {
        if (cache_control_header.stale_while_revalidate) {
          // First stale-while-revalidate directive wins if there are multiple
          // ones.
          continue;
        }
        bool ok;
        double stale_while_revalidate = directives[i].second.ToDouble(&ok);
        if (ok) {
          cache_control_header.stale_while_revalidate =
              base::Seconds(stale_while_revalidate);
        }
      }
    }
  }

  if (!cache_control_header.contains_no_cache) {
    // Handle Pragma: no-cache
    // This is deprecated and equivalent to Cache-control: no-cache
    // Don't bother tokenizing the value, it is not important
    cache_control_header.contains_no_cache =
        pragma_value.LowerASCII().Contains(kNoCacheDirective);
  }
  return cache_control_header;
}

void ParseCommaDelimitedHeader(const String& header_value,
                               CommaDelimitedHeaderSet& header_set) {
  Vector<String> results;
  header_value.Split(",", results);
  for (auto& value : results)
    header_set.insert(value.StripWhiteSpace(IsWhitespace));
}

bool ParseMultipartHeadersFromBody(base::span<const uint8_t> bytes,
                                   ResourceResponse* response,
                                   wtf_size_t* end) {
  DCHECK(IsMainThread());

  size_t headers_end_pos =
      net::HttpUtil::LocateEndOfAdditionalHeaders(bytes, 0);

  if (headers_end_pos == std::string::npos)
    return false;

  *end = static_cast<wtf_size_t>(headers_end_pos);

  // Eat headers and prepend a status line as is required by
  // HttpResponseHeaders.
  std::string headers("HTTP/1.1 200 OK\r\n");
  headers.append(base::as_string_view(bytes.first(headers_end_pos)));

  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  std::string mime_type, charset;
  response_headers->GetMimeTypeAndCharset(&mime_type, &charset);
  response->SetMimeType(WebString::FromUTF8(mime_type));
  response->SetTextEncodingName(WebString::FromUTF8(charset));

  // Copy headers listed in replaceHeaders to the response.
  for (const AtomicString& header : ReplaceHeaders()) {
    std::string value;
    StringUTF8Adaptor adaptor(header);
    std::string_view header_string_piece(adaptor.AsStringView());
    size_t iterator = 0;

    response->ClearHttpHeaderField(header);
    Vector<AtomicString> values;
    while (response_headers->EnumerateHeader(&iterator, header_string_piece,
                                             &value)) {
      const AtomicString atomic_value = WebString::FromLatin1(value);
      values.push_back(atomic_value);
    }
    response->AddHttpHeaderFieldWithMultipleValues(header, values);
  }
  return true;
}

bool ParseMultipartFormHeadersFromBody(base::span<const uint8_t> bytes,
                                       HTTPHeaderMap* header_fields,
                                       wtf_size_t* end) {
  DCHECK_EQ(0u, header_fields->size());

  size_t headers_end_pos =
      net::HttpUtil::LocateEndOfAdditionalHeaders(bytes, 0);

  if (headers_end_pos == std::string::npos)
    return false;

  *end = static_cast<wtf_size_t>(headers_end_pos);

  // Eat headers and prepend a status line as is required by
  // HttpResponseHeaders.
  std::string headers("HTTP/1.1 200 OK\r\n");
  headers.append(base::as_string_view(bytes.first(headers_end_pos)));

  auto responseHeaders = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));

  // Copy selected header fields.
  const AtomicString* const headerNamePointers[] = {
      &http_names::kContentDisposition, &http_names::kContentType};
  for (const AtomicString* headerNamePointer : headerNamePointers) {
    StringUTF8Adaptor adaptor(*headerNamePointer);
    size_t iterator = 0;
    std::string_view headerNameStringPiece = adaptor.AsStringView();
    std::string value;
    while (responseHeaders->EnumerateHeader(&iterator, headerNameStringPiece,
                                            &value)) {
      header_fields->Add(*headerNamePointer, WebString::FromUTF8(value));
    }
  }

  return true;
}

bool ParseContentRangeHeaderFor206(const String& content_range,
                                   int64_t* first_byte_position,
                                   int64_t* last_byte_position,
                                   int64_t* instance_length) {
  return net::HttpUtil::ParseContentRangeHeaderFor206(
      StringUTF8Adaptor(content_range).AsStringView(), first_byte_position,
      last_byte_position, instance_length);
}

std::unique_ptr<ServerTimingHeaderVector> ParseServerTimingHeader(
    const String& headerValue) {
  std::unique_ptr<ServerTimingHeaderVector> headers =
      std::make_unique<ServerTimingHeaderVector>();

  if (!headerValue.IsNull()) {
    DCHECK(headerValue.Is8Bit());

    HeaderFieldTokenizer tokenizer(headerValue);
    while (!tokenizer.IsConsumed()) {
      StringView name;
      if (!tokenizer.ConsumeToken(ParsedContentType::Mode::kNormal, name)) {
        break;
      }

      ServerTimingHeader header(name.ToString());

      tokenizer.ConsumeBeforeAnyCharMatch({',', ';'});

      while (tokenizer.Consume(';')) {
        StringView parameter_name;
        if (!tokenizer.ConsumeToken(ParsedContentType::Mode::kNormal,
                                    parameter_name)) {
          break;
        }

        String value = "";
        if (tokenizer.Consume('=')) {
          tokenizer.ConsumeTokenOrQuotedString(ParsedContentType::Mode::kNormal,
                                               value);
          tokenizer.ConsumeBeforeAnyCharMatch({',', ';'});
        }
        header.SetParameter(parameter_name, value);
      }

      headers->push_back(std::make_unique<ServerTimingHeader>(header));

      if (!tokenizer.Consume(',')) {
        break;
      }
    }
  }
  return headers;
}

// This function is simply calling network::ParseHeaders and convert from/to
// blink types. It is used for navigation requests served by a ServiceWorker. It
// is tested by FetchResponseDataTest.ContentSecurityPolicy.
network::mojom::blink::ParsedHeadersPtr ParseHeaders(const String& raw_headers,
                                                     const KURL& url) {
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers.Latin1()));
  return network::mojom::ConvertToBlink(
      network::PopulateParsedHeaders(headers.get(), GURL(url)));
}

// This function is simply calling network::ParseContentSecurityPolicies and
// converting from/to blink types.
Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ParseContentSecurityPolicies(
    const String& raw_policies,
    network::mojom::blink::ContentSecurityPolicyType type,
    network::mojom::blink::ContentSecurityPolicySource source,
    const KURL& base_url) {
  return network::mojom::ConvertToBlink(network::ParseContentSecurityPolicies(
      raw_policies.Utf8(), type, source, GURL(base_url)));
}

// This function is simply calling network::ParseContentSecurityPolicies and
// converting from/to blink types.
Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ParseContentSecurityPolicies(
    const String& raw_policies,
    network::mojom::blink::ContentSecurityPolicyType type,
    network::mojom::blink::ContentSecurityPolicySource source,
    const SecurityOrigin& self_origin) {
  const SecurityOrigin* precursor_origin =
      self_origin.GetOriginOrPrecursorOriginIfOpaque();
  KURL base_url;
  base_url.SetProtocol(precursor_origin->Protocol());
  base_url.SetHost(precursor_origin->Host());
  base_url.SetPort(precursor_origin->Port());
  return ParseContentSecurityPolicies(raw_policies, type, source, base_url);
}

Vector<network::mojom::blink::ContentSecurityPolicyPtr>
ParseContentSecurityPolicyHeaders(
    const ContentSecurityPolicyResponseHeaders& headers) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed_csps =
      ParseContentSecurityPolicies(
          headers.ContentSecurityPolicy(),
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          headers.ResponseUrl());
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> report_only_csps =
      ParseContentSecurityPolicies(
          headers.ContentSecurityPolicyReportOnly(),
          network::mojom::blink::ContentSecurityPolicyType::kReport,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          headers.ResponseUrl());
  parsed_csps.AppendRange(std::make_move_iterator(report_only_csps.begin()),
                          std::make_move_iterator(report_only_csps.end()));
  return parsed_csps;
}

network::mojom::blink::TimingAllowOriginPtr ParseTimingAllowOrigin(
    const String& header_value) {
  return network::mojom::ConvertToBlink(
      network::ParseTimingAllowOrigin(header_value.Latin1()));
}

network::mojom::blink::NoVarySearchWithParseErrorPtr ParseNoVarySearch(
    const String& header_value) {
  // Parse the No-Vary-Search hint value by making a header in order to
  // reuse existing code.
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  headers->AddHeader("No-Vary-Search", header_value.Utf8());

  auto parsed_nvs_with_error =
      ConvertToBlink(network::ParseNoVarySearch(*headers));
  // `parsed_nvs_with_error` cannot be null here. Because we know the header is
  // available, we will get a parse error or a No-Vary-Search.
  CHECK(parsed_nvs_with_error);
  return parsed_nvs_with_error;
}

String GetNoVarySearchHintConsoleMessage(
    const network::mojom::NoVarySearchParseError& error) {
  return network::mojom::ConvertToBlink(
      network::GetNoVarySearchHintConsoleMessage(error));
}
}  // namespace blink
