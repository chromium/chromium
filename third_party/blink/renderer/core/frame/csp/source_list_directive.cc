// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/source_list_directive.h"

#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_source.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {

struct SupportedPrefixesStruct {
  const char* prefix;
  network::mojom::blink::CSPHashAlgorithm type;
};

}  // namespace

namespace blink {

namespace {

bool IsSourceListNone(const UChar* begin, const UChar* end) {
  SkipWhile<UChar, IsASCIISpace>(begin, end);

  const UChar* position = begin;
  SkipWhile<UChar, IsSourceCharacter>(position, end);
  if (!EqualIgnoringASCIICase(
          "'none'",
          StringView(begin, static_cast<wtf_size_t>(position - begin))))
    return false;

  SkipWhile<UChar, IsASCIISpace>(position, end);
  if (position != end)
    return false;

  return true;
}

// nonce-source      = "'nonce-" nonce-value "'"
// nonce-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool ParseNonce(const UChar* begin, const UChar* end, String* nonce) {
  size_t nonce_length = end - begin;
  StringView prefix("'nonce-");

  // TODO(esprehn): Should be StringView(begin, nonceLength).startsWith(prefix).
  if (nonce_length <= prefix.length() ||
      !EqualIgnoringASCIICase(prefix, StringView(begin, prefix.length()))) {
      return true;
  }

  const UChar* position = begin + prefix.length();
  const UChar* nonce_begin = position;

  DCHECK(position < end);
  SkipWhile<UChar, IsNonceCharacter>(position, end);
  DCHECK(nonce_begin <= position);

  if (position + 1 != end || *position != '\'' || position == nonce_begin)
    return false;

  *nonce = String(nonce_begin, static_cast<wtf_size_t>(position - nonce_begin));
  return true;
}

// hash-source       = "'" hash-algorithm "-" hash-value "'"
// hash-algorithm    = "sha1" / "sha256" / "sha384" / "sha512"
// hash-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool ParseHash(const UChar* begin,
               const UChar* end,
               Vector<uint8_t>& hash,
               network::mojom::blink::CSPHashAlgorithm* hash_algorithm) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kAlgorithmMap array in
  // ContentSecurityPolicy::FillInCSPHashValues().

  constexpr SupportedPrefixesStruct kSupportedPrefixes[] = {
      {"'sha256-", network::mojom::blink::CSPHashAlgorithm::SHA256},
      {"'sha384-", network::mojom::blink::CSPHashAlgorithm::SHA384},
      {"'sha512-", network::mojom::blink::CSPHashAlgorithm::SHA512},
      {"'sha-256-", network::mojom::blink::CSPHashAlgorithm::SHA256},
      {"'sha-384-", network::mojom::blink::CSPHashAlgorithm::SHA384},
      {"'sha-512-", network::mojom::blink::CSPHashAlgorithm::SHA512}};

  StringView prefix;
  size_t hash_length = end - begin;

  DCHECK_EQ(*hash_algorithm, network::mojom::blink::CSPHashAlgorithm::None);

  for (auto supported_prefix : kSupportedPrefixes) {
    prefix = supported_prefix.prefix;
    // TODO(esprehn): Should be StringView(begin, end -
    // begin).startsWith(prefix).
    if (hash_length > prefix.length() &&
        EqualIgnoringASCIICase(prefix, StringView(begin, prefix.length()))) {
      *hash_algorithm = supported_prefix.type;
      break;
    }
  }

  if (*hash_algorithm == network::mojom::blink::CSPHashAlgorithm::None)
    return true;

  const UChar* position = begin + prefix.length();
  const UChar* hash_begin = position;

  DCHECK(position < end);
  SkipWhile<UChar, IsBase64EncodedCharacter>(position, end);
  DCHECK(hash_begin <= position);

  // Base64 encodings may end with exactly one or two '=' characters
  if (position < end)
    SkipExactly<UChar>(position, position + 1, '=');
  if (position < end)
    SkipExactly<UChar>(position, position + 1, '=');

  if (position + 1 != end || *position != '\'' || position == hash_begin)
    return false;

  // We accept base64url-encoded data here by normalizing it to base64.
  Vector<char> out;
  Base64Decode(NormalizeToBase64(String(
                   hash_begin, static_cast<wtf_size_t>(position - hash_begin))),
               out);
  if (out.size() > kMaxDigestSize)
    return false;

  DCHECK(hash.IsEmpty());
  for (char el : out)
    hash.push_back(el);
  return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool ParseScheme(const UChar* begin, const UChar* end, String* scheme) {
  DCHECK(begin <= end);
  DCHECK(scheme->IsEmpty());

  if (begin == end)
    return false;

  const UChar* position = begin;

  if (!SkipExactly<UChar, IsASCIIAlpha>(position, end))
    return false;

  SkipWhile<UChar, IsSchemeContinuationCharacter>(position, end);

  if (position != end)
    return false;

  *scheme = String(begin, static_cast<wtf_size_t>(end - begin));
  return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
// static
bool ParseHost(const UChar* begin,
               const UChar* end,
               String* host,
               bool* host_wildcard) {
  DCHECK(begin <= end);
  DCHECK(host->IsEmpty());
  DCHECK(!*host_wildcard);

  if (begin == end)
    return false;

  const UChar* position = begin;

  // Parse "*" or [ "*." ].
  if (SkipExactly<UChar>(position, end, '*')) {
    *host_wildcard = true;

    if (position == end) {
      // "*"
      return true;
    }

    if (!SkipExactly<UChar>(position, end, '.'))
      return false;
  }
  const UChar* host_begin = position;

  // Parse 1*host-hcar.
  if (!SkipExactly<UChar, IsHostCharacter>(position, end))
    return false;
  SkipWhile<UChar, IsHostCharacter>(position, end);

  // Parse *( "." 1*host-char ).
  while (position < end) {
    if (!SkipExactly<UChar>(position, end, '.'))
      return false;
    if (!SkipExactly<UChar, IsHostCharacter>(position, end))
      return false;
    SkipWhile<UChar, IsHostCharacter>(position, end);
  }

  *host = String(host_begin, static_cast<wtf_size_t>(end - host_begin));
  return true;
}

bool ParsePath(const UChar* begin,
               const UChar* end,
               String* path,
               ContentSecurityPolicy* policy,
               const String& directive_name) {
  DCHECK(begin <= end);
  DCHECK(path->IsEmpty());

  const UChar* position = begin;
  SkipWhile<UChar, IsPathComponentCharacter>(position, end);
  // path/to/file.js?query=string || path/to/file.js#anchor
  //                ^                               ^
  if (position < end) {
    policy->ReportInvalidPathCharacter(
        directive_name, String(begin, static_cast<wtf_size_t>(end - begin)),
        *position);
  }

  *path = DecodeURLEscapeSequences(
      String(begin, static_cast<wtf_size_t>(position - begin)),
      DecodeURLMode::kUTF8OrIsomorphic);

  DCHECK(position <= end);
  DCHECK(position == end || (*position == '#' || *position == '?'));
  return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool ParsePort(const UChar* begin,
               const UChar* end,
               int* port,
               bool* port_wildcard) {
  DCHECK(begin <= end);
  DCHECK_EQ(*port, url::PORT_UNSPECIFIED);
  DCHECK(!*port_wildcard);

  if (!SkipExactly<UChar>(begin, end, ':'))
    NOTREACHED();

  if (begin == end)
    return false;

  if (end - begin == 1 && *begin == '*') {
    *port = url::PORT_UNSPECIFIED;
    *port_wildcard = true;
    return true;
  }

  const UChar* position = begin;
  SkipWhile<UChar, IsASCIIDigit>(position, end);

  if (position != end)
    return false;

  bool ok;
  *port = CharactersToInt(begin, end - begin, WTF::NumberParsingOptions::kNone,
                          &ok);
  return ok;
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool ParseSourceExpression(const UChar* begin,
                           const UChar* end,
                           ContentSecurityPolicy* policy,
                           const String& directive_name,
                           network::mojom::blink::CSPSource& parsed_source) {
  const UChar* position = begin;
  const UChar* begin_host = begin;
  const UChar* begin_path = end;
  const UChar* begin_port = nullptr;

  SkipWhile<UChar, IsNotColonOrSlash>(position, end);

  if (position == end) {
    // host
    //     ^
    return ParseHost(begin_host, position, &parsed_source.host,
                     &parsed_source.is_host_wildcard);
  }

  if (position < end && *position == '/') {
    // host/path || host/ || /
    //     ^            ^    ^
    return ParseHost(begin_host, position, &parsed_source.host,
                     &parsed_source.is_host_wildcard) &&
           ParsePath(position, end, &parsed_source.path, policy,
                     directive_name);
  }

  if (position < end && *position == ':') {
    if (end - position == 1) {
      // scheme:
      //       ^
      return ParseScheme(begin, position, &parsed_source.scheme);
    }

    if (position[1] == '/') {
      // scheme://host || scheme://
      //       ^                ^
      if (!ParseScheme(begin, position, &parsed_source.scheme) ||
          !SkipExactly<UChar>(position, end, ':') ||
          !SkipExactly<UChar>(position, end, '/') ||
          !SkipExactly<UChar>(position, end, '/'))
        return false;
      if (position == end)
        return false;
      begin_host = position;
      SkipWhile<UChar, IsNotColonOrSlash>(position, end);
    }

    if (position < end && *position == ':') {
      // host:port || scheme://host:port
      //     ^                     ^
      begin_port = position;
      SkipUntil<UChar>(position, end, '/');
    }
  }

  if (position < end && *position == '/') {
    // scheme://host/path || scheme://host:port/path
    //              ^                          ^
    if (position == begin_host)
      return false;
    begin_path = position;
  }

  if (!ParseHost(begin_host, begin_port ? begin_port : begin_path,
                 &parsed_source.host, &parsed_source.is_host_wildcard))
    return false;

  if (begin_port) {
    if (!ParsePort(begin_port, begin_path, &parsed_source.port,
                   &parsed_source.is_port_wildcard))
      return false;
  } else {
    parsed_source.port = url::PORT_UNSPECIFIED;
  }

  if (begin_path != end) {
    if (!ParsePath(begin_path, end, &parsed_source.path, policy,
                   directive_name))
      return false;
  }

  return true;
}

bool ParseSource(const UChar* begin,
                 const UChar* end,
                 network::mojom::blink::CSPSourceList& source_list,
                 ContentSecurityPolicy* policy,
                 const String& directive_name) {
  if (begin == end)
    return false;

  StringView token(begin, static_cast<wtf_size_t>(end - begin));

  if (EqualIgnoringASCIICase("'none'", token))
    return false;

  if (end - begin == 1 && *begin == '*') {
    source_list.allow_star = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'self'", token)) {
    source_list.allow_self = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-inline'", token)) {
    source_list.allow_inline = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-eval'", token)) {
    source_list.allow_eval = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-allow-redirects'", token)) {
    source_list.allow_response_redirects = true;
    return true;
  }

  if (policy->SupportsWasmEval() &&
      EqualIgnoringASCIICase("'wasm-eval'", token)) {
    source_list.allow_wasm_eval = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'strict-dynamic'", token)) {
    source_list.allow_dynamic = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-hashes'", token)) {
    source_list.allow_unsafe_hashes = true;
    return true;
  }

  if (EqualIgnoringASCIICase("'report-sample'", token)) {
    source_list.report_sample = true;
    return true;
  }

  String nonce;
  if (!ParseNonce(begin, end, &nonce))
    return false;

  if (!nonce.IsNull()) {
    source_list.nonces.push_back(nonce);
    return true;
  }

  Vector<uint8_t> hash;
  network::mojom::blink::CSPHashAlgorithm algorithm =
      network::mojom::blink::CSPHashAlgorithm::None;
  if (!ParseHash(begin, end, hash, &algorithm))
    return false;

  if (hash.size() > 0) {
    source_list.hashes.push_back(
        network::mojom::blink::CSPHashSource::New(algorithm, hash));
    return true;
  }

  // We must initialize all fields of |source_expression| so that it is always
  // valid for serialization by mojo, even if scheme, host or path are not
  // provided.
  auto source_expression =
      network::mojom::blink::CSPSource::New("", "", -1, "", false, false);
  if (ParseSourceExpression(begin, end, policy, directive_name,
                            *source_expression)) {
    source_list.sources.push_back(std::move(source_expression));
    return true;
  }

  return false;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
network::mojom::blink::CSPSourceListPtr Parse(const UChar* begin,
                                              const UChar* end,
                                              ContentSecurityPolicy* policy,
                                              const String& directive_name) {
  network::mojom::blink::CSPSourceListPtr source_list =
      network::mojom::blink::CSPSourceList::New();
  if (IsSourceListNone(begin, end))
    return source_list;

  const UChar* position = begin;
  while (position < end) {
    SkipWhile<UChar, IsASCIISpace>(position, end);
    if (position == end)
      return source_list;

    const UChar* begin_source = position;
    SkipWhile<UChar, IsSourceCharacter>(position, end);

    if (ParseSource(begin_source, position, *source_list, policy,
                    directive_name)) {
      String token(begin_source,
                   static_cast<wtf_size_t>(position - begin_source));
      if (ContentSecurityPolicy::GetDirectiveType(token) !=
          CSPDirectiveName::Unknown) {
        policy->ReportDirectiveAsSourceExpression(
            directive_name,
            source_list->sources[source_list->sources.size() - 1]->host);
      }
    } else {
      policy->ReportInvalidSourceExpression(
          directive_name, String(begin_source, static_cast<wtf_size_t>(
                                                   position - begin_source)));
    }

    DCHECK(position == end || IsASCIISpace(*position));
  }
  return source_list;
}

bool HasSourceMatchInList(
    const Vector<network::mojom::blink::CSPSourcePtr>& list,
    const String& self_protocol,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  for (const auto& source : list) {
    if (CSPSourceMatches(*source, self_protocol, url, redirect_status)) {
      return true;
    }
  }
  return false;
}

}  // namespace

network::mojom::blink::CSPSourceListPtr CSPSourceListParse(
    const String& name,
    const String& value,
    ContentSecurityPolicy* policy) {
  Vector<UChar> characters;
  value.AppendTo(characters);
  return Parse(characters.data(), characters.data() + characters.size(), policy,
               name);
}

bool CSPSourceListAllows(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPSource& self_source,
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list.allow_star) {
    if (url.ProtocolIsInHTTPFamily() || url.ProtocolIs("ftp") ||
        url.ProtocolIs("ws") || url.ProtocolIs("wss") ||
        (!url.Protocol().IsEmpty() &&
         EqualIgnoringASCIICase(url.Protocol(), self_source.scheme)))
      return true;

    return HasSourceMatchInList(source_list.sources, self_source.scheme, url,
                                redirect_status);
  }
  if (source_list.allow_self &&
      CSPSourceMatchesAsSelf(self_source, self_source.scheme, url)) {
    return true;
  }

  return HasSourceMatchInList(source_list.sources, self_source.scheme, url,
                              redirect_status);
}

bool CSPSourceListAllowNonce(
    const network::mojom::blink::CSPSourceList& source_list,
    const String& nonce) {
  String nonce_stripped = nonce.StripWhiteSpace();
  return !nonce_stripped.IsNull() &&
         source_list.nonces.Contains(nonce_stripped);
}

bool CSPSourceListAllowHash(
    const network::mojom::blink::CSPSourceList& source_list,
    const network::mojom::blink::CSPHashSource& hash_value) {
  for (const network::mojom::blink::CSPHashSourcePtr& hash :
       source_list.hashes) {
    if (*hash == hash_value)
      return true;
  }
  return false;
}

bool CSPSourceListIsNone(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.sources.size() && !source_list.allow_self &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_dynamic &&
         !source_list.nonces.size() && !source_list.hashes.size();
}

bool CSPSourceListIsSelf(
    const network::mojom::blink::CSPSourceList& source_list) {
  return source_list.allow_self && !source_list.sources.size() &&
         !source_list.allow_star && !source_list.allow_inline &&
         !source_list.allow_unsafe_hashes && !source_list.allow_eval &&
         !source_list.allow_wasm_eval && !source_list.allow_dynamic &&
         !source_list.nonces.size() && !source_list.hashes.size();
}

bool CSPSourceListIsHashOrNoncePresent(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.nonces.IsEmpty() || !source_list.hashes.IsEmpty();
}

bool CSPSourceListAllowsURLBasedMatching(
    const network::mojom::blink::CSPSourceList& source_list) {
  return !source_list.allow_dynamic &&
         (source_list.sources.size() || source_list.allow_star ||
          source_list.allow_self);
}

bool CSPSourceListAllowAllInline(
    CSPDirectiveName directive_type,
    const network::mojom::blink::CSPSourceList& source_list) {
  if (directive_type != CSPDirectiveName::DefaultSrc &&
      !ContentSecurityPolicy::IsScriptDirective(directive_type) &&
      !ContentSecurityPolicy::IsStyleDirective(directive_type)) {
    return false;
  }

  return source_list.allow_inline &&
         !CSPSourceListIsHashOrNoncePresent(source_list) &&
         (!ContentSecurityPolicy::IsScriptDirective(directive_type) ||
          !source_list.allow_dynamic);
}

}  // namespace blink
