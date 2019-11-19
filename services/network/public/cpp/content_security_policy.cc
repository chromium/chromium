// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace network {

using DirectivesMap = base::flat_map<base::StringPiece, base::StringPiece>;

namespace {

// Parses a "Content-Security-Policy" header.
// Returns a map to the directives found.
DirectivesMap ParseHeaderValue(base::StringPiece header) {
  DirectivesMap result;

  // For each token returned by strictly splitting serialized on the
  // U+003B SEMICOLON character (;):
  // 1. Strip leading and trailing ASCII whitespace from token.
  // 2. If token is an empty string, continue.
  for (const auto& directive : base::SplitStringPiece(
           header, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // 3. Let directive name be the result of collecting a sequence of
    // code points from token which are not ASCII whitespace.
    // 4. Set directive name to be the result of running ASCII lowercase
    // on directive name.
    size_t pos = directive.find_first_of(base::kWhitespaceASCII);
    base::StringPiece name = directive.substr(0, pos);

    // 5. If policy's directive set contains a directive whose name is
    // directive name, continue.
    if (result.find(name) != result.end())
      continue;

    // 6. Let directive value be the result of splitting token on ASCII
    // whitespace.
    base::StringPiece value;
    if (pos != std::string::npos)
      value = directive.substr(pos + 1);

    // 7. Let directive be a new directive whose name is directive name,
    // and value is directive value.
    // 8. Append directive to policy's directive set.
    result.insert({name, value});
  }

  return result;
}

// https://www.w3.org/TR/CSP3/#grammardef-scheme-part
bool ParseScheme(base::StringPiece scheme, mojom::CSPSource* csp_source) {
  if (scheme.empty())
    return false;

  if (!base::IsAsciiAlpha(scheme[0]))
    return false;

  auto is_scheme_character = [](auto c) {
    return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '+' ||
           c == '-' || c == '.';
  };

  if (!std::all_of(scheme.begin() + 1, scheme.end(), is_scheme_character))
    return false;

  csp_source->scheme = scheme.as_string();

  return true;
}

// https://www.w3.org/TR/CSP3/#grammardef-host-part
bool ParseHost(base::StringPiece host, mojom::CSPSource* csp_source) {
  if (host.size() == 0)
    return false;

  // * || *.
  if (host[0] == '*') {
    if (host.size() == 1) {
      csp_source->is_host_wildcard = true;
      return true;
    }

    if (host[1] != '.')
      return false;

    csp_source->is_host_wildcard = true;
    host = host.substr(2);
  }

  if (host.empty())
    return false;

  for (const base::StringPiece& piece : base::SplitStringPiece(
           host, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (piece.empty() || !std::all_of(piece.begin(), piece.end(), [](auto c) {
          return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '-';
        }))
      return false;
  }
  csp_source->host = host.as_string();

  return true;
}

// https://www.w3.org/TR/CSP3/#grammardef-port-part
bool ParsePort(base::StringPiece port, mojom::CSPSource* csp_source) {
  if (port.empty())
    return false;

  if (base::EqualsCaseInsensitiveASCII(port, "*")) {
    csp_source->is_port_wildcard = true;
    return true;
  }

  if (!std::all_of(port.begin(), port.end(),
                   base::IsAsciiDigit<base::StringPiece::value_type>)) {
    return false;
  }

  return base::StringToInt(port, &csp_source->port);
}

// https://www.w3.org/TR/CSP3/#grammardef-path-part
bool ParsePath(base::StringPiece path, mojom::CSPSource* csp_source) {
  DCHECK_NE(0U, path.size());
  if (path[0] != '/')
    return false;

  // TODO(lfg): Emit a warning to the user when a path containing # or ? is
  // seen.
  path = path.substr(0, path.find_first_of("#?"));

  url::RawCanonOutputT<base::char16> unescaped;
  url::DecodeURLEscapeSequences(path.data(), path.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped);
  base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &csp_source->path);

  return true;
}

// Parses an ancestor source expression.
// https://www.w3.org/TR/CSP3/#grammardef-ancestor-source
//
// Return false on errors.
bool ParseAncestorSource(base::StringPiece expression,
                         mojom::CSPSource* csp_source) {
  // TODO(arthursonzogni): Blink reports an invalid source expression when
  // 'none' is parsed here.
  if (base::EqualsCaseInsensitiveASCII(expression, "'none'"))
    return false;

  size_t position = expression.find_first_of(":/");
  if (position != std::string::npos && expression[position] == ':') {
    // scheme:
    //       ^
    if (position + 1 == expression.size())
      return ParseScheme(expression.substr(0, position), csp_source);

    if (expression[position + 1] == '/') {
      // scheme://
      //       ^
      if (position + 2 >= expression.size() || expression[position + 2] != '/')
        return false;
      if (!ParseScheme(expression.substr(0, position), csp_source))
        return false;
      expression = expression.substr(position + 3);
      position = expression.find_first_of(":/");
    }
  }

  // host
  //     ^
  if (!ParseHost(expression.substr(0, position), csp_source))
    return false;

  // If there's nothing more to parse (no port or path specified), return.
  if (position == std::string::npos)
    return true;

  expression = expression.substr(position);

  // :\d*
  // ^
  if (expression[0] == ':') {
    size_t port_end = expression.find_first_of("/");
    base::StringPiece port = expression.substr(
        1, port_end == std::string::npos ? std::string::npos : port_end - 1);
    if (!ParsePort(port, csp_source))
      return false;
    if (port_end == std::string::npos)
      return true;

    expression = expression.substr(port_end);
  }

  // /
  // ^
  return expression.empty() || ParsePath(expression, csp_source);
}

// Parse ancestor-source-list grammar.
// https://www.w3.org/TR/CSP3/#directive-frame-ancestors
mojom::CSPSourceListPtr ParseFrameAncestorsDirective(
    base::StringPiece frame_ancestors_value) {
  base::StringPiece value = base::TrimString(
      frame_ancestors_value, base::kWhitespaceASCII, base::TRIM_ALL);

  std::vector<mojom::CSPSourcePtr> sources;

  if (frame_ancestors_value.empty())
    return {};

  auto directive = mojom::CSPSourceList::New();

  if (base::EqualsCaseInsensitiveASCII(value, "'none'"))
    return directive;

  for (const auto& expression : base::SplitStringPiece(
           value, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(expression, "'self'")) {
      directive->allow_self = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "*")) {
      directive->allow_star = true;
      continue;
    }

    auto csp_source = mojom::CSPSource::New();
    if (ParseAncestorSource(expression, csp_source.get())) {
      directive->sources.push_back(std::move(csp_source));
      continue;
    }

    // Parsing error.
    return {};
  }

  return directive;
}

// Parses a reporting directive.
// https://w3c.github.io/webappsec-csp/#directives-reporting
// TODO(lfg): The report-to should be treated as a single token according to the
// spec, but this implementation accepts multiple endpoints
// https://crbug.com/916265.
base::Optional<std::vector<GURL>> ParseReportDirective(
    const GURL& request_url,
    base::StringPiece value) {
  std::vector<GURL> report_endpoints;
  for (const auto& uri : base::SplitStringPiece(
           value, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    report_endpoints.push_back(request_url.Resolve(uri));
    if (!report_endpoints.back().is_valid())
      return base::nullopt;
  }
  return report_endpoints;
}

}  // namespace

ContentSecurityPolicy::ContentSecurityPolicy() = default;
ContentSecurityPolicy::~ContentSecurityPolicy() = default;

ContentSecurityPolicy::ContentSecurityPolicy(
    mojom::ContentSecurityPolicyPtr content_security_policy_ptr)
    : content_security_policy_ptr_(std::move(content_security_policy_ptr)) {}

ContentSecurityPolicy::ContentSecurityPolicy(const ContentSecurityPolicy& other)
    : content_security_policy_ptr_(other.content_security_policy_ptr_.Clone()) {
}

ContentSecurityPolicy::ContentSecurityPolicy(ContentSecurityPolicy&& other) =
    default;

ContentSecurityPolicy& ContentSecurityPolicy::operator=(
    const ContentSecurityPolicy& other) {
  content_security_policy_ptr_ = other.content_security_policy_ptr_.Clone();

  return *this;
}

ContentSecurityPolicy::operator mojom::ContentSecurityPolicyPtr() const {
  return content_security_policy_ptr_.Clone();
}

bool ContentSecurityPolicy::Parse(const GURL& base_url,
                                  const net::HttpResponseHeaders& headers) {
  size_t iter = 0;
  std::string header_value;
  while (headers.EnumerateHeader(&iter, "content-security-policy",
                                 &header_value)) {
    if (!Parse(base_url, header_value))
      return false;
  }
  return true;
}

bool ContentSecurityPolicy::Parse(const GURL& base_url,
                                  base::StringPiece header_value) {
  if (!content_security_policy_ptr_) {
    content_security_policy_ptr_ = mojom::ContentSecurityPolicy::New();
  }

  // RFC7230, section 3.2.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  for (const auto& header :
       base::SplitStringPiece(header_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    DirectivesMap directives = ParseHeaderValue(header);

    auto frame_ancestors = directives.find("frame-ancestors");
    if (frame_ancestors != directives.end()) {
      if (!ParseFrameAncestors(frame_ancestors->second)) {
        content_security_policy_ptr_.reset();
        return false;
      }
    }

    auto report_endpoints = directives.find("report-to");
    if (report_endpoints != directives.end()) {
      if (!content_security_policy_ptr_->use_reporting_api) {
        content_security_policy_ptr_->use_reporting_api = true;
        content_security_policy_ptr_->report_endpoints.clear();
      }
    } else {
      report_endpoints = directives.find("report-uri");
    }

    if (report_endpoints != directives.end()) {
      if (!ParseReportEndpoint(base_url, report_endpoints->second)) {
        content_security_policy_ptr_.reset();
        return false;
      }
    }
  }

  return true;
}

bool ContentSecurityPolicy::ParseFrameAncestors(
    base::StringPiece frame_ancestors_value) {
  // A frame-ancestors directive has already been parsed. Skip further
  // frame-ancestors directives per
  // https://www.w3.org/TR/CSP3/#parse-serialized-policy.
  if (content_security_policy_ptr_->frame_ancestors)
    return true;

  if (auto directive = ParseFrameAncestorsDirective(frame_ancestors_value)) {
    content_security_policy_ptr_->frame_ancestors = std::move(directive);
    return true;
  }

  // TODO(lfg): Emit a warning to the user when parsing an invalid
  // expression.
  return false;
}

bool ContentSecurityPolicy::ParseReportEndpoint(
    const GURL& base_url,
    base::StringPiece header_value) {
  // A report-uri directive has already been parsed. Skip further directives per
  // https://www.w3.org/TR/CSP3/#parse-serialized-policy.
  if (!content_security_policy_ptr_->report_endpoints.empty())
    return true;

  if (auto parsed_report_directive =
          ParseReportDirective(base_url, header_value)) {
    content_security_policy_ptr_->report_endpoints =
        std::move(*parsed_report_directive);
    return true;
  }

  // TODO(lfg): Emit a warning to the user when parsing an invalid
  // expression.
  return false;
}

}  // namespace network
