// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/content_security_policy.h"

#include <sstream>
#include <string>
#include "base/base64url.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "services/network/public/cpp/content_security_policy/csp_source_list.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace network {

using CSPDirectiveName = mojom::CSPDirectiveName;
using DirectivesMap =
    std::vector<std::pair<base::StringPiece, base::StringPiece>>;

namespace {

bool IsDirectiveNameCharacter(char c) {
  return base::IsAsciiAlpha(c) || c == '-';
}

bool IsDirectiveValueCharacter(char c) {
  // Whitespace + VCHAR, but not ',' and ';'
  return base::IsAsciiWhitespace(c) ||
         (base::IsAsciiPrintable(c) && c != ',' && c != ';');
}

std::string ElideURLForReportViolation(const GURL& url) {
  // TODO(arthursonzogni): the url length should be limited to 1024 char. Find
  // a function that will not break the utf8 encoding while eliding the string.
  return url.spec();
}

bool SupportedInReportOnly(CSPDirectiveName directive) {
  switch (directive) {
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::UpgradeInsecureRequests:
    case CSPDirectiveName::TreatAsPublicAddress:
      return false;

    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::BlockAllMixedContent:
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::FormAction:
    case CSPDirectiveName::FrameAncestors:
    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::NavigateTo:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::RequireTrustedTypesFor:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
    case CSPDirectiveName::TrustedTypes:
    case CSPDirectiveName::Unknown:
    case CSPDirectiveName::WorkerSrc:
      return true;
  };
}

// Return the error message specific to one CSP |directive|.
// $1: Blocked URL.
// $2: Blocking policy.
const char* ErrorMessage(CSPDirectiveName directive) {
  switch (directive) {
    case CSPDirectiveName::FormAction:
      return "Refused to send form data to '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::FrameAncestors:
      return "Refused to frame '$1' because an ancestor violates the following "
             "Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::FrameSrc:
      return "Refused to frame '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";
    case CSPDirectiveName::NavigateTo:
      return "Refused to navigate to '$1' because it violates the "
             "following Content Security Policy directive: \"$2\".";

    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::BlockAllMixedContent:
    case CSPDirectiveName::ChildSrc:
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::RequireTrustedTypesFor:
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
    case CSPDirectiveName::StyleSrc:
    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
    case CSPDirectiveName::TreatAsPublicAddress:
    case CSPDirectiveName::TrustedTypes:
    case CSPDirectiveName::UpgradeInsecureRequests:
    case CSPDirectiveName::WorkerSrc:
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return nullptr;
  };
}

void ReportViolation(CSPContext* context,
                     const mojom::ContentSecurityPolicyPtr& policy,
                     const CSPDirectiveName effective_directive_name,
                     const CSPDirectiveName directive_name,
                     const GURL& url,
                     bool has_followed_redirect,
                     const mojom::SourceLocationPtr& source_location) {
  // For security reasons, some urls must not be disclosed. This includes the
  // blocked url and the source location of the error. Care must be taken to
  // ensure that these are not transmitted between different cross-origin
  // renderers.
  GURL blocked_url = (directive_name == CSPDirectiveName::FrameAncestors)
                         ? GURL(ToString(*policy->self_origin))
                         : url;
  auto safe_source_location =
      source_location ? source_location->Clone() : mojom::SourceLocation::New();
  context->SanitizeDataForUseInCspViolation(has_followed_redirect,
                                            directive_name, &blocked_url,
                                            safe_source_location.get());

  std::stringstream message;

  if (policy->header->type == mojom::ContentSecurityPolicyType::kReport)
    message << "[Report Only] ";

  message << base::ReplaceStringPlaceholders(
      ErrorMessage(directive_name),
      {ElideURLForReportViolation(blocked_url),
       ToString(effective_directive_name) + " " +
           ToString(policy->directives[effective_directive_name])},
      nullptr);

  if (effective_directive_name != directive_name) {
    message << " Note that '" << ToString(directive_name)
            << "' was not explicitly set, so '"
            << ToString(effective_directive_name) << "' is used as a fallback.";
  }

  message << "\n";

  context->ReportContentSecurityPolicyViolation(mojom::CSPViolation::New(
      ToString(effective_directive_name), ToString(directive_name),
      message.str(), blocked_url, policy->report_endpoints,
      policy->use_reporting_api, policy->header->header_value,
      policy->header->type, has_followed_redirect,
      std::move(safe_source_location)));
}

const GURL ExtractInnerURL(const GURL& url) {
  if (const GURL* inner_url = url.inner_url())
    return *inner_url;
  else
    // TODO(arthursonzogni): revisit this once GURL::inner_url support blob-URL.
    return GURL(url.path());
}

std::string InnermostScheme(const GURL& url) {
  if (url.SchemeIsFileSystem() || url.SchemeIsBlob())
    return ExtractInnerURL(url).scheme();
  return url.scheme();
}

// Extensions can load their own internal content into the document. They
// shouldn't be blocked by the document's CSP.
//
// There is an exception: CSP:frame-ancestors. This one is not about allowing a
// document to embed other resources. This is about being embedded. As such
// this shouldn't be bypassed. A document should be able to deny being embedded
// inside an extension.
// See https://crbug.com/1115590
bool ShouldBypassContentSecurityPolicy(CSPContext* context,
                                       CSPDirectiveName directive,
                                       const GURL& url) {
  if (directive == CSPDirectiveName::FrameAncestors)
    return false;

  return context->SchemeShouldBypassCSP(InnermostScheme(url));
}

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

    // 5. Let directive value be the result of splitting token on ASCII
    // whitespace.
    base::StringPiece value;
    if (pos != std::string::npos) {
      value = base::TrimString(directive.substr(pos + 1),
                               base::kWhitespaceASCII, base::TRIM_ALL);
    }

    // 6. Let directive be a new directive whose name is directive name,
    // and value is directive value.
    // 7. Append directive to policy's directive set.
    result.emplace_back(std::make_pair(name, value));
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

  url::RawCanonOutputT<base::char16> unescaped;
  url::DecodeURLEscapeSequences(path.data(), path.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped);
  base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &csp_source->path);

  return true;
}

// Parses a CSP source expression.
// https://w3c.github.io/webappsec-csp/#source-lists
//
// Return false on errors.
// Adds parsing error messages to |parsing_errors|.
// Notice that this can return true and still add some parsing error message
// (for example, if there is a url with a non-empty query part).
bool ParseSource(CSPDirectiveName directive_name,
                 base::StringPiece expression,
                 mojom::CSPSource* csp_source,
                 std::vector<std::string>& parsing_errors) {
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
  if (expression.empty())
    return true;

  // Emit a warning to the user when a url contains a # or ?.
  position = expression.find_first_of("#?");
  bool path_parsed = ParsePath(expression.substr(0, position), csp_source);
  if (path_parsed && position != std::string::npos) {
    const char* ignoring =
        expression[position] == '?'
            ? "The query component, including the '?', will be ignored."
            : "The fragment identifier, including the '#', will be ignored.";
    parsing_errors.emplace_back(base::StringPrintf(
        "The source list for Content-Security-Policy directive '%s' "
        "contains a source with an invalid path: '%s'. %s",
        ToString(directive_name).c_str(), expression.as_string().c_str(),
        ignoring));
  }

  return path_parsed;
}

bool IsBase64Char(char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '+' ||
         c == '-' || c == '_' || c == '/';
}

int EatChar(const char** it, const char* end, bool (*predicate)(char)) {
  int count = 0;
  while (*it != end) {
    if (!predicate(**it))
      break;
    ++count;
    ++(*it);
  }
  return count;
}

// Checks whether |expression| is a valid base64-encoded string.
// Cf. https://w3c.github.io/webappsec-csp/#framework-directive-source-list.
bool IsBase64(base::StringPiece expression) {
  if (expression.empty())
    return false;

  auto* it = expression.begin();
  auto* end = expression.end();

  int count_1 = EatChar(&it, end, IsBase64Char);
  int count_2 = EatChar(&it, end, [](char c) -> bool { return c == '='; });

  // At least one non '=' char at the beginning, at most two '=' at the end.
  return count_1 >= 1 && count_2 <= 2 && it == end;
}

// Parse a nonce-source, return false on error.
bool ParseNonce(base::StringPiece expression, std::string* nonce) {
  if (!base::StartsWith(expression, "'nonce-",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  base::StringPiece subexpression =
      expression.substr(7, expression.length() - 8);

  if (!IsBase64(subexpression))
    return false;

  if (expression[expression.length() - 1] != '\'') {
    return false;
  }

  *nonce = subexpression.as_string();
  return true;
}

struct SupportedPrefixesStruct {
  const char* prefix;
  int prefix_length;
  mojom::CSPHashAlgorithm type;
};

// Parse a hash-source, return false on error.
bool ParseHash(base::StringPiece expression, mojom::CSPHashSource* hash) {
  static const SupportedPrefixesStruct SupportedPrefixes[] = {
      {"'sha256-", 8, mojom::CSPHashAlgorithm::SHA256},
      {"'sha384-", 8, mojom::CSPHashAlgorithm::SHA384},
      {"'sha512-", 8, mojom::CSPHashAlgorithm::SHA512},
      {"'sha-256-", 9, mojom::CSPHashAlgorithm::SHA256},
      {"'sha-384-", 9, mojom::CSPHashAlgorithm::SHA384},
      {"'sha-512-", 9, mojom::CSPHashAlgorithm::SHA512}};

  for (auto item : SupportedPrefixes) {
    if (base::StartsWith(expression, item.prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      base::StringPiece subexpression = expression.substr(
          item.prefix_length, expression.length() - item.prefix_length - 1);
      if (!IsBase64(subexpression))
        return false;

      if (expression[expression.length() - 1] != '\'')
        return false;

      hash->algorithm = item.type;

      // We lazily accept both base64url and base64-encoded data.
      std::string normalized_value;
      base::ReplaceChars(subexpression, "+", "-", &normalized_value);
      base::ReplaceChars(normalized_value, "/", "_", &normalized_value);

      std::string out;
      if (!base::Base64UrlDecode(normalized_value,
                                 base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                 &out))
        return false;
      hash->value = std::vector<uint8_t>(out.begin(), out.end());
      return true;
    }
  }

  return false;
}

// Parse source-list grammar.
// https://www.w3.org/TR/CSP3/#grammardef-serialized-source-list
// Append parsing errors to |parsing_errors|.
mojom::CSPSourceListPtr ParseSourceList(
    CSPDirectiveName directive_name,
    base::StringPiece directive_value,
    std::vector<std::string>& parsing_errors) {
  base::StringPiece value =
      base::TrimString(directive_value, base::kWhitespaceASCII, base::TRIM_ALL);

  auto directive = mojom::CSPSourceList::New();

  if (base::EqualsCaseInsensitiveASCII(value, "'none'"))
    return directive;

  for (const auto& expression : base::SplitStringPiece(
           value, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(expression, "'none'")) {
      parsing_errors.emplace_back(base::StringPrintf(
          "The Content-Security-Policy directive '%s' contains the keyword "
          "'none' alongside with other source expressions. The keyword 'none' "
          "must be the only source expression in the directive value, "
          "otherwise it is ignored.",
          ToString(directive_name).c_str()));
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'self'")) {
      directive->allow_self = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "*")) {
      directive->allow_star = true;
      continue;
    }

    if (ToCSPDirectiveName(expression.as_string()) !=
        CSPDirectiveName::Unknown) {
      parsing_errors.emplace_back(base::StringPrintf(
          "The Content-Security-Policy directive '%s' contains '%s' as a "
          "source expression. Did you want to add it as a directive and forget "
          "a semicolon?",
          ToString(directive_name).c_str(), expression.as_string().c_str()));
    }

    auto csp_source = mojom::CSPSource::New();
    if (ParseSource(directive_name, expression, csp_source.get(),
                    parsing_errors)) {
      directive->sources.push_back(std::move(csp_source));
      continue;
    }

    if (directive_name == CSPDirectiveName::FrameAncestors) {
      // The frame-ancestors directive does not support anything else
      // https://w3c.github.io/webappsec-csp/#directive-frame-ancestors
      parsing_errors.emplace_back(base::StringPrintf(
          "The Content-Security-Policy directive 'frame-ancestors' does not "
          "support the source expression '%s'",
          expression.as_string().c_str()));
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-inline'")) {
      directive->allow_inline = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-eval'")) {
      directive->allow_eval = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'wasm-eval'")) {
      directive->allow_wasm_eval = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression,
                                         "'unsafe-allow-redirects'") &&
        directive_name == CSPDirectiveName::NavigateTo) {
      directive->allow_response_redirects = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'strict-dynamic'")) {
      directive->allow_dynamic = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'unsafe-hashes'")) {
      directive->allow_unsafe_hashes = true;
      continue;
    }

    if (base::EqualsCaseInsensitiveASCII(expression, "'report-sample'")) {
      directive->report_sample = true;
      continue;
    }

    std::string nonce;
    if (ParseNonce(expression, &nonce)) {
      directive->nonces.push_back(std::move(nonce));
      continue;
    }

    auto hash = mojom::CSPHashSource::New();
    if (ParseHash(expression, hash.get())) {
      directive->hashes.push_back(std::move(hash));
      continue;
    }

    // Parsing error.
    // Ignore this source-expression.
    parsing_errors.emplace_back(base::StringPrintf(
        "The source list for the Content-Security-Policy directive '%s' "
        "contains an invalid source: '%s'.",
        ToString(directive_name).c_str(), expression.as_string().c_str()));
  }

  return directive;
}

// Parse the 'required-trusted-types-for' directive.
// https://w3c.github.io/webappsec-trusted-types/dist/spec/#require-trusted-types-for-csp-directive
network::mojom::CSPRequireTrustedTypesFor ParseRequireTrustedTypesFor(
    base::StringPiece value,
    std::vector<std::string>& parsing_errors) {
  network::mojom::CSPRequireTrustedTypesFor out =
      network::mojom::CSPRequireTrustedTypesFor::None;
  for (const auto expression : base::SplitStringPiece(
           value, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (expression == "'script'") {
      out = network::mojom::CSPRequireTrustedTypesFor::Script;
    } else {
      const char* hint = nullptr;
      if (expression == "script" || expression == "scripts" ||
          expression == "'scripts'") {
        hint = " Did you mean 'script'?";
      }

      parsing_errors.emplace_back(base::StringPrintf(
          "Invalid expression in 'require-trusted-types-for' "
          "Content Security Policy directive: %s.%s\n",
          expression.as_string().c_str(), hint));
    }
  }
  if (out == network::mojom::CSPRequireTrustedTypesFor::None)
    parsing_errors.emplace_back(base::StringPrintf(
        "'require-trusted-types-for' Content Security Policy "
        "directive is empty; The directive has no effect.\n"));
  return out;
}

// This implements tt-policy-name from
// https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive/
bool IsValidTrustedTypesPolicyName(base::StringPiece value) {
  return base::ranges::all_of(value, [](char c) {
    return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) ||
           base::Contains("-#=_/@.%", c);
  });
}

// Parse the 'trusted-types' directive.
// https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-csp-directive
network::mojom::CSPTrustedTypesPtr ParseTrustedTypes(
    base::StringPiece value,
    std::vector<std::string>& parsing_errors) {
  auto out = network::mojom::CSPTrustedTypes::New();
  std::vector<base::StringPiece> pieces =
      base::SplitStringPiece(value, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (pieces.size() == 1 && pieces[0] == "'none'")
    return out;

  for (const auto expression : pieces) {
    if (expression == "*")
      out->allow_any = true;
    else if (expression == "'allow-duplicates'")
      out->allow_duplicates = true;
    else if (expression == "'none'") {
      parsing_errors.emplace_back(
          "The value of the Content Security Policy directive "
          "'trusted_types' contains an invalid policy: 'none'. "
          "It will be ignored. "
          "Note that 'none' has no effect unless it is the only "
          "expression in the directive value.");
    } else if (IsValidTrustedTypesPolicyName(expression))
      out->list.emplace_back(expression);
    else {
      parsing_errors.emplace_back(base::StringPrintf(
          "The value of the Content Security Policy directive "
          "'trusted_types' contains an invalid policy: '%s'. "
          "It will be ignored.",
          expression.as_string().c_str()));
    }
  }
  return out;
}

// Parses a reporting directive.
// https://w3c.github.io/webappsec-csp/#directives-reporting
// TODO(lfg): The report-to should be treated as a single token according to the
// spec, but this implementation accepts multiple endpoints
// https://crbug.com/916265.
void ParseReportDirective(const GURL& request_url,
                          base::StringPiece value,
                          bool using_reporting_api,
                          std::vector<std::string>* report_endpoints) {
  for (const auto& uri : base::SplitStringPiece(value, base::kWhitespaceASCII,
                                                base::TRIM_WHITESPACE,
                                                base::SPLIT_WANT_NONEMPTY)) {
    // There are two types of reporting directive:
    //
    // - "report-uri (uri)+"
    //   |uri| must be resolved relatively to the requested URL.
    //
    // - "report-to (endpoint)+"
    //   |endpoint| is an arbitrary string. It refers to an endpoint declared in
    //   the "Report-To" header. See https://w3c.github.io/reporting
    if (using_reporting_api) {
      report_endpoints->push_back(uri.as_string());

      // 'report-to' only allows for a single token. The following ones are
      // ignored. A console error warning is displayed from blink's CSP parser.
      break;
    } else {
      GURL url = request_url.Resolve(uri);

      // TODO(lfg): Emit a warning when parsing an invalid reporting URL.
      if (!url.is_valid())
        continue;
      report_endpoints->push_back(url.spec());
    }
  }
}

void WarnIfDirectiveValueNotEmpty(
    const std::pair<base::StringPiece, base::StringPiece>& directive,
    std::vector<std::string>& parsing_errors) {
  if (!directive.second.empty()) {
    parsing_errors.emplace_back(base::StringPrintf(
        "The Content Security Policy directive '%s' should be empty, but was "
        "delivered with a value of '%s'. The directive has been applied, and "
        "the value ignored.",
        directive.first.as_string().c_str(),
        directive.second.as_string().c_str()));
  }
}

mojom::CSPSourcePtr ComputeSelfOrigin(const GURL& url) {
  if (url.scheme() == url::kFileScheme) {
    // Forget the host for file schemes. Host can anyway only be `localhost` or
    // empty and this is platform dependent.
    //
    // TODO(antoniosartori): Consider returning mojom::CSPSource::New() for
    // file: urls, so that 'self' for file: would match nothing.
    return mojom::CSPSource::New(url::kFileScheme, "", url::PORT_UNSPECIFIED,
                                 "", false, false);
  }
  return mojom::CSPSource::New(url.scheme(), url.host(), url.EffectiveIntPort(),
                               "", false, false);
}

std::string UnrecognizedDirectiveErrorMessage(
    const std::string& directive_name) {
  if (base::EqualsCaseInsensitiveASCII(directive_name, "plugin-types")) {
    return "The Content-Security-Policy directive 'plugin-types' has been "
           "removed from the "
           "specification. If you want to block plugins, consider specifying "
           "\"object-src 'none'\" instead.";
  }

  return base::StringPrintf(
      "Unrecognized Content-Security-Policy directive '%s'.",
      directive_name.c_str());
}

void AddContentSecurityPolicyFromHeader(base::StringPiece header,
                                        mojom::ContentSecurityPolicyType type,
                                        const GURL& base_url,
                                        mojom::ContentSecurityPolicyPtr& out) {
  DirectivesMap directives = ParseHeaderValue(header);
  out->header = mojom::ContentSecurityPolicyHeader::New(
      header.as_string(), type, mojom::ContentSecurityPolicySource::kHTTP);
  out->self_origin = ComputeSelfOrigin(base_url);

  for (auto directive : directives) {
    if (!base::ranges::all_of(directive.first, IsDirectiveNameCharacter)) {
      out->parsing_errors.emplace_back(base::StringPrintf(
          "The Content-Security-Policy directive name '%s' contains one or "
          "more invalid characters. Only ASCII alphanumeric characters or "
          "dashes '-' are allowed in directive names.",
          directive.first.as_string().c_str()));
      continue;
    }

    CSPDirectiveName directive_name =
        ToCSPDirectiveName(directive.first.as_string());

    if (directive_name == CSPDirectiveName::Unknown) {
      out->parsing_errors.emplace_back(
          UnrecognizedDirectiveErrorMessage(directive.first.as_string()));
      continue;
    }

    // A directive with this name has already been parsed. Skip further
    // directives per
    // https://www.w3.org/TR/CSP3/#parse-serialized-policy.
    if (out->raw_directives.count(directive_name)) {
      out->parsing_errors.emplace_back(base::StringPrintf(
          "Ignoring duplicate Content-Security-Policy directive '%s'.",
          directive.first.as_string().c_str()));
      continue;
    }
    out->raw_directives[directive_name] = directive.second.as_string();

    if (!base::ranges::all_of(directive.second, IsDirectiveValueCharacter)) {
      out->parsing_errors.emplace_back(base::StringPrintf(
          "The value for the Content-Security-Policy directive '%s' contains "
          "one or more invalid characters. In a source expression, "
          "non-whitespace characters outside ASCII "
          "0x21-0x7E must be Punycode-encoded, as described in RFC 3492 "
          "(https://tools.ietf.org/html/rfc3492), if part of the hostname and "
          "percent-encoded, as described in RFC 3986, section 2.1 "
          "(http://tools.ietf.org/html/rfc3986#section-2.1), if part of the "
          "path.",
          directive.first.as_string().c_str()));
      continue;
    }

    if (type == mojom::ContentSecurityPolicyType::kReport &&
        !SupportedInReportOnly(directive_name)) {
      out->parsing_errors.emplace_back(
          base::StringPrintf("The Content Security Policy directive '%s' is "
                             "ignored when delivered in a report-only policy.",
                             directive.first.as_string().c_str()));
      continue;
    }

    switch (directive_name) {
      case CSPDirectiveName::BaseURI:
      case CSPDirectiveName::ChildSrc:
      case CSPDirectiveName::ConnectSrc:
      case CSPDirectiveName::DefaultSrc:
      case CSPDirectiveName::FontSrc:
      case CSPDirectiveName::FormAction:
      case CSPDirectiveName::FrameAncestors:
      case CSPDirectiveName::FrameSrc:
      case CSPDirectiveName::ImgSrc:
      case CSPDirectiveName::ManifestSrc:
      case CSPDirectiveName::MediaSrc:
      case CSPDirectiveName::NavigateTo:
      case CSPDirectiveName::ObjectSrc:
      case CSPDirectiveName::PrefetchSrc:
      case CSPDirectiveName::ScriptSrc:
      case CSPDirectiveName::ScriptSrcAttr:
      case CSPDirectiveName::ScriptSrcElem:
      case CSPDirectiveName::StyleSrc:
      case CSPDirectiveName::StyleSrcAttr:
      case CSPDirectiveName::StyleSrcElem:
      case CSPDirectiveName::WorkerSrc:
        out->directives[directive_name] = ParseSourceList(
            directive_name, directive.second, out->parsing_errors);
        break;
      case CSPDirectiveName::Sandbox:
        // Note: Outside of CSP embedded enforcement,
        // |ParseSandboxPolicy(...).error_message| isn't displayed to the user.
        // Blink's CSP parser is already in charge of it.
        {
          auto sandbox = ParseWebSandboxPolicy(directive.second,
                                               mojom::WebSandboxFlags::kNone);
          out->sandbox = sandbox.flags;
          if (!sandbox.error_message.empty())
            out->parsing_errors.emplace_back(std::move(sandbox.error_message));
        }
        break;
      case CSPDirectiveName::UpgradeInsecureRequests:
        out->upgrade_insecure_requests = true;
        WarnIfDirectiveValueNotEmpty(directive, out->parsing_errors);
        break;
      case CSPDirectiveName::TreatAsPublicAddress:
        out->treat_as_public_address = true;
        WarnIfDirectiveValueNotEmpty(directive, out->parsing_errors);
        break;
      case CSPDirectiveName::RequireTrustedTypesFor:
        out->require_trusted_types_for =
            ParseRequireTrustedTypesFor(directive.second, out->parsing_errors);
        break;

      case CSPDirectiveName::TrustedTypes:
        out->trusted_types =
            ParseTrustedTypes(directive.second, out->parsing_errors);
        break;

      case CSPDirectiveName::BlockAllMixedContent:
        out->block_all_mixed_content = true;
        WarnIfDirectiveValueNotEmpty(directive, out->parsing_errors);
        break;

      case CSPDirectiveName::ReportTo:
        out->use_reporting_api = true;
        out->report_endpoints.clear();
        ParseReportDirective(base_url, directive.second, out->use_reporting_api,
                             &(out->report_endpoints));
        break;
      case CSPDirectiveName::ReportURI:
        if (!out->use_reporting_api)
          ParseReportDirective(base_url, directive.second,
                               out->use_reporting_api,
                               &(out->report_endpoints));
        break;
      case CSPDirectiveName::Unknown:
        break;
    }
  }
}

std::pair<CSPDirectiveName, const mojom::CSPSourceList*> GetSourceList(
    CSPDirectiveName directive,
    const mojom::ContentSecurityPolicy& policy) {
  for (CSPDirectiveName effective_directive = directive;
       effective_directive != CSPDirectiveName::Unknown;
       effective_directive =
           CSPFallbackDirective(effective_directive, directive)) {
    auto value = policy.directives.find(effective_directive);
    if (value != policy.directives.end())
      return std::make_pair(effective_directive, value->second.get());
  }
  return std::make_pair(CSPDirectiveName::Unknown, nullptr);
}

}  // namespace

CSPDirectiveName CSPFallbackDirective(CSPDirectiveName directive,
                                      CSPDirectiveName original_directive) {
  switch (directive) {
    case CSPDirectiveName::ConnectSrc:
    case CSPDirectiveName::FontSrc:
    case CSPDirectiveName::ImgSrc:
    case CSPDirectiveName::ManifestSrc:
    case CSPDirectiveName::MediaSrc:
    case CSPDirectiveName::PrefetchSrc:
    case CSPDirectiveName::ObjectSrc:
    case CSPDirectiveName::ScriptSrc:
    case CSPDirectiveName::StyleSrc:
      return CSPDirectiveName::DefaultSrc;

    case CSPDirectiveName::ScriptSrcAttr:
    case CSPDirectiveName::ScriptSrcElem:
      return CSPDirectiveName::ScriptSrc;

    case CSPDirectiveName::StyleSrcAttr:
    case CSPDirectiveName::StyleSrcElem:
      return CSPDirectiveName::StyleSrc;

    case CSPDirectiveName::FrameSrc:
    case CSPDirectiveName::WorkerSrc:
      return CSPDirectiveName::ChildSrc;

    // Because the fallback chain of child-src can be different if we are
    // checking a worker or a frame request, we need to know the original type
    // of the request to decide. These are the fallback chains for worker-src
    // and frame-src specifically.

    // worker-src > child-src > script-src > default-src
    // frame-src > child-src > default-src

    // Since there are some situations and tests that will operate on the
    // `child-src` directive directly (like for example the EE subsumption
    // algorithm), we consider the child-src > default-src fallback path as the
    // "default" and the worker-src fallback path as an exception.
    case CSPDirectiveName::ChildSrc:
      if (original_directive == CSPDirectiveName::WorkerSrc)
        return CSPDirectiveName::ScriptSrc;

      return CSPDirectiveName::DefaultSrc;

    case CSPDirectiveName::BaseURI:
    case CSPDirectiveName::BlockAllMixedContent:
    case CSPDirectiveName::DefaultSrc:
    case CSPDirectiveName::FormAction:
    case CSPDirectiveName::FrameAncestors:
    case CSPDirectiveName::NavigateTo:
    case CSPDirectiveName::ReportTo:
    case CSPDirectiveName::ReportURI:
    case CSPDirectiveName::RequireTrustedTypesFor:
    case CSPDirectiveName::Sandbox:
    case CSPDirectiveName::TreatAsPublicAddress:
    case CSPDirectiveName::TrustedTypes:
    case CSPDirectiveName::UpgradeInsecureRequests:
      return CSPDirectiveName::Unknown;
    case CSPDirectiveName::Unknown:
      NOTREACHED();
      return CSPDirectiveName::Unknown;
  }
}

void AddContentSecurityPolicyFromHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out) {
  size_t iter = 0;
  std::string header_value;
  while (headers.EnumerateHeader(&iter, "content-security-policy",
                                 &header_value)) {
    AddContentSecurityPolicyFromHeaders(
        header_value, mojom::ContentSecurityPolicyType::kEnforce, base_url,
        out);
  }
  iter = 0;
  while (headers.EnumerateHeader(&iter, "content-security-policy-report-only",
                                 &header_value)) {
    AddContentSecurityPolicyFromHeaders(
        header_value, mojom::ContentSecurityPolicyType::kReport, base_url, out);
  }
}

void AddContentSecurityPolicyFromHeaders(
    base::StringPiece header_value,
    network::mojom::ContentSecurityPolicyType type,
    const GURL& base_url,
    std::vector<mojom::ContentSecurityPolicyPtr>* out) {
  // RFC7230, section 3.2.2 specifies that headers appearing multiple times can
  // be combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  for (const auto& header :
       base::SplitStringPiece(header_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    auto policy = mojom::ContentSecurityPolicy::New();
    AddContentSecurityPolicyFromHeader(header, type, base_url, policy);

    out->push_back(std::move(policy));
  }
}

mojom::AllowCSPFromHeaderValuePtr ParseAllowCSPFromHeader(
    const net::HttpResponseHeaders& headers) {
  std::string allow_csp_from;
  if (!headers.GetNormalizedHeader("Allow-CSP-From", &allow_csp_from))
    return nullptr;

  base::StringPiece trimmed =
      base::TrimWhitespaceASCII(allow_csp_from, base::TRIM_ALL);

  if (trimmed == "*")
    return mojom::AllowCSPFromHeaderValue::NewAllowStar(true);

  GURL parsed_url = GURL(trimmed);
  if (!parsed_url.is_valid()) {
    return mojom::AllowCSPFromHeaderValue::NewErrorMessage(
        "The 'Allow-CSP-From' header contains neither '*' nor a valid origin.");
  }
  return mojom::AllowCSPFromHeaderValue::NewOrigin(
      url::Origin::Create(parsed_url));
}

bool CheckContentSecurityPolicy(const mojom::ContentSecurityPolicyPtr& policy,
                                CSPDirectiveName directive_name,
                                const GURL& url,
                                bool has_followed_redirect,
                                bool is_response_check,
                                CSPContext* context,
                                const mojom::SourceLocationPtr& source_location,
                                bool is_form_submission) {
  DCHECK(policy->self_origin);

  if (ShouldBypassContentSecurityPolicy(context, directive_name, url))
    return true;

  // 'navigate-to' has no effect when doing a form submission and a
  // 'form-action' directive is present.
  if (is_form_submission && directive_name == CSPDirectiveName::NavigateTo &&
      policy->directives.count(CSPDirectiveName::FormAction)) {
    return true;
  }

  for (CSPDirectiveName effective_directive_name = directive_name;
       effective_directive_name != CSPDirectiveName::Unknown;
       effective_directive_name =
           CSPFallbackDirective(effective_directive_name, directive_name)) {
    const auto& directive = policy->directives.find(effective_directive_name);
    if (directive == policy->directives.end())
      continue;

    const auto& source_list = directive->second;
    bool allowed = CheckCSPSourceList(*source_list, url, *(policy->self_origin),
                                      has_followed_redirect, is_response_check);

    if (!allowed) {
      ReportViolation(context, policy, effective_directive_name, directive_name,
                      url, has_followed_redirect, source_location);
    }

    return allowed ||
           policy->header->type == mojom::ContentSecurityPolicyType::kReport;
  }
  return true;
}

bool ShouldUpgradeInsecureRequest(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies) {
  for (const auto& policy : policies) {
    if (policy->upgrade_insecure_requests)
      return true;
  }

  return false;
}

bool ShouldTreatAsPublicAddress(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policies) {
  for (const auto& policy : policies) {
    if (policy->treat_as_public_address)
      return true;
  }

  return false;
}

void UpgradeInsecureRequest(GURL* url) {
  // Only HTTP URL can be upgraded to HTTPS.
  if (!url->SchemeIs(url::kHttpScheme))
    return;

  // Some URL like http://127.0.0.0.1 are considered potentially trustworthy and
  // aren't upgraded, even if the protocol used is HTTP.
  if (IsUrlPotentiallyTrustworthy(*url))
    return;

  // Updating the URL's scheme also implicitly updates the URL's port from
  // 80 to 443 if needed.
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  *url = url->ReplaceComponents(replacements);
}

bool IsValidRequiredCSPAttr(
    const std::vector<mojom::ContentSecurityPolicyPtr>& policy,
    const mojom::ContentSecurityPolicy* context,
    std::string& error_message) {
  DCHECK(policy.size() == 1);
  if (!policy[0])
    return false;

  if (!policy[0]->report_endpoints.empty() ||
      // We really don't want any report directives, even with invalid/missing
      // endpoints.
      policy[0]->raw_directives.contains(mojom::CSPDirectiveName::ReportURI) ||
      policy[0]->raw_directives.contains(mojom::CSPDirectiveName::ReportTo)) {
    error_message =
        "The csp attribute cannot contain the directives 'report-to' or "
        "'report-uri'.";
    return false;
  }

  if (context && !Subsumes(*context, policy)) {
    error_message =
        "The csp attribute Content-Security-Policy is not subsumed by the "
        "frame's parent csp attribute Content-Security-Policy.";
    return false;
  }

  return true;
}

bool Subsumes(const mojom::ContentSecurityPolicy& policy_a,
              const std::vector<mojom::ContentSecurityPolicyPtr>& policies_b) {
  if (policy_a.header->type == mojom::ContentSecurityPolicyType::kReport)
    return true;

  if (policy_a.directives.empty())
    return true;

  if (policies_b.empty())
    return false;

  // All policies in |policies_b| must have the same self_origin.
  mojom::CSPSource* origin_b = policies_b[0]->self_origin.get();

  // A list of directives that we consider for subsumption.
  // See more about source lists here:
  // https://w3c.github.io/webappsec-csp/#framework-directive-source-list
  static const CSPDirectiveName directives[] = {
      CSPDirectiveName::ChildSrc,       CSPDirectiveName::ConnectSrc,
      CSPDirectiveName::FontSrc,        CSPDirectiveName::FrameSrc,
      CSPDirectiveName::ImgSrc,         CSPDirectiveName::ManifestSrc,
      CSPDirectiveName::MediaSrc,       CSPDirectiveName::ObjectSrc,
      CSPDirectiveName::ScriptSrc,      CSPDirectiveName::ScriptSrcAttr,
      CSPDirectiveName::ScriptSrcElem,  CSPDirectiveName::StyleSrc,
      CSPDirectiveName::StyleSrcAttr,   CSPDirectiveName::StyleSrcElem,
      CSPDirectiveName::WorkerSrc,      CSPDirectiveName::BaseURI,
      CSPDirectiveName::FrameAncestors, CSPDirectiveName::FormAction,
      CSPDirectiveName::NavigateTo};

  return base::ranges::all_of(directives, [&](CSPDirectiveName directive) {
    auto required = GetSourceList(directive, policy_a);
    if (!required.second)
      return true;

    // Aggregate all serialized source lists of the returned CSP into a vector
    // based on a directive type, defaulting accordingly (for example, to
    // `default-src`).
    std::vector<const mojom::CSPSourceList*> returned;
    for (const auto& policy_b : policies_b) {
      // Ignore report-only returned policies.
      if (policy_b->header->type == mojom::ContentSecurityPolicyType::kReport)
        continue;

      auto source_list = GetSourceList(directive, *policy_b);
      if (source_list.second)
        returned.push_back(source_list.second);
    }
    // TODO(amalika): Add checks for sandbox, disown-opener,
    // navigation-to.
    return CSPSourceListSubsumes(*required.second, returned, required.first,
                                 origin_b);
  });
}

CSPDirectiveName ToCSPDirectiveName(const std::string& name) {
  if (name == "base-uri")
    return CSPDirectiveName::BaseURI;
  if (name == "block-all-mixed-content")
    return CSPDirectiveName::BlockAllMixedContent;
  if (name == "child-src")
    return CSPDirectiveName::ChildSrc;
  if (name == "connect-src")
    return CSPDirectiveName::ConnectSrc;
  if (name == "default-src")
    return CSPDirectiveName::DefaultSrc;
  if (name == "frame-ancestors")
    return CSPDirectiveName::FrameAncestors;
  if (name == "frame-src")
    return CSPDirectiveName::FrameSrc;
  if (name == "font-src")
    return CSPDirectiveName::FontSrc;
  if (name == "form-action")
    return CSPDirectiveName::FormAction;
  if (name == "img-src")
    return CSPDirectiveName::ImgSrc;
  if (name == "manifest-src")
    return CSPDirectiveName::ManifestSrc;
  if (name == "media-src")
    return CSPDirectiveName::MediaSrc;
  if (name == "object-src")
    return CSPDirectiveName::ObjectSrc;
  if (name == "prefetch-src")
    return CSPDirectiveName::PrefetchSrc;
  if (name == "report-uri")
    return CSPDirectiveName::ReportURI;
  if (name == "require-trusted-types-for")
    return CSPDirectiveName::RequireTrustedTypesFor;
  if (name == "sandbox")
    return CSPDirectiveName::Sandbox;
  if (name == "script-src")
    return CSPDirectiveName::ScriptSrc;
  if (name == "script-src-attr")
    return CSPDirectiveName::ScriptSrcAttr;
  if (name == "script-src-elem")
    return CSPDirectiveName::ScriptSrcElem;
  if (name == "style-src")
    return CSPDirectiveName::StyleSrc;
  if (name == "style-src-attr")
    return CSPDirectiveName::StyleSrcAttr;
  if (name == "style-src-elem")
    return CSPDirectiveName::StyleSrcElem;
  if (name == "treat-as-public-address")
    return CSPDirectiveName::TreatAsPublicAddress;
  if (name == "trusted-types")
    return CSPDirectiveName::TrustedTypes;
  if (name == "upgrade-insecure-requests")
    return CSPDirectiveName::UpgradeInsecureRequests;
  if (name == "worker-src")
    return CSPDirectiveName::WorkerSrc;
  if (name == "report-to")
    return CSPDirectiveName::ReportTo;
  if (name == "navigate-to")
    return CSPDirectiveName::NavigateTo;

  return CSPDirectiveName::Unknown;
}

std::string ToString(CSPDirectiveName name) {
  switch (name) {
    case CSPDirectiveName::BaseURI:
      return "base-uri";
    case CSPDirectiveName::BlockAllMixedContent:
      return "block-all-mixed-content";
    case CSPDirectiveName::ChildSrc:
      return "child-src";
    case CSPDirectiveName::ConnectSrc:
      return "connect-src";
    case CSPDirectiveName::DefaultSrc:
      return "default-src";
    case CSPDirectiveName::FrameAncestors:
      return "frame-ancestors";
    case CSPDirectiveName::FrameSrc:
      return "frame-src";
    case CSPDirectiveName::FontSrc:
      return "font-src";
    case CSPDirectiveName::FormAction:
      return "form-action";
    case CSPDirectiveName::ImgSrc:
      return "img-src";
    case CSPDirectiveName::ManifestSrc:
      return "manifest-src";
    case CSPDirectiveName::MediaSrc:
      return "media-src";
    case CSPDirectiveName::ObjectSrc:
      return "object-src";
    case CSPDirectiveName::PrefetchSrc:
      return "prefetch-src";
    case CSPDirectiveName::ReportURI:
      return "report-uri";
    case CSPDirectiveName::RequireTrustedTypesFor:
      return "require-trusted-types-for";
    case CSPDirectiveName::Sandbox:
      return "sandbox";
    case CSPDirectiveName::ScriptSrc:
      return "script-src";
    case CSPDirectiveName::ScriptSrcAttr:
      return "script-src-attr";
    case CSPDirectiveName::ScriptSrcElem:
      return "script-src-elem";
    case CSPDirectiveName::StyleSrc:
      return "style-src";
    case CSPDirectiveName::StyleSrcAttr:
      return "style-src-attr";
    case CSPDirectiveName::StyleSrcElem:
      return "style-src-elem";
    case CSPDirectiveName::UpgradeInsecureRequests:
      return "upgrade-insecure-requests";
    case CSPDirectiveName::TreatAsPublicAddress:
      return "treat-as-public-address";
    case CSPDirectiveName::TrustedTypes:
      return "trusted-types";
    case CSPDirectiveName::WorkerSrc:
      return "worker-src";
    case CSPDirectiveName::ReportTo:
      return "report-to";
    case CSPDirectiveName::NavigateTo:
      return "navigate-to";
    case CSPDirectiveName::Unknown:
      return "";
  }
  NOTREACHED();
  return "";
}

bool AllowsBlanketEnforcementOfRequiredCSP(
    const url::Origin& request_origin,
    const GURL& response_url,
    const network::mojom::AllowCSPFromHeaderValue* allow_csp_from) {
  if (response_url.SchemeIs(url::kAboutScheme) ||
      response_url.SchemeIs(url::kDataScheme) || response_url.SchemeIsFile() ||
      response_url.SchemeIsFileSystem() || response_url.SchemeIsBlob()) {
    return true;
  }

  if (request_origin.IsSameOriginWith(url::Origin::Create(response_url)))
    return true;

  if (!allow_csp_from)
    return false;

  if (allow_csp_from->is_allow_star())
    return true;

  if (allow_csp_from->is_origin() &&
      request_origin.IsSameOriginWith(allow_csp_from->get_origin())) {
    return true;
  }

  return false;
}

}  // namespace network
