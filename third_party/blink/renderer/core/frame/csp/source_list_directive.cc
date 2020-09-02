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
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace {
struct SupportedPrefixesStruct {
  const char* prefix;
  blink::ContentSecurityPolicyHashAlgorithm type;
};
}  // namespace

namespace blink {

SourceListDirective::SourceListDirective(const String& name,
                                         const String& value,
                                         ContentSecurityPolicy* policy)
    : CSPDirective(name, value, policy),
      policy_(policy),
      directive_name_(name),
      allow_self_(false),
      allow_star_(false),
      allow_inline_(false),
      allow_eval_(false),
      allow_wasm_eval_(false),
      allow_dynamic_(false),
      allow_unsafe_hashes_(false),
      report_sample_(false),
      hash_algorithms_used_(0) {
  Vector<UChar> characters;
  value.AppendTo(characters);
  Parse(characters.data(), characters.data() + characters.size());
}

static bool IsSourceListNone(const UChar* begin, const UChar* end) {
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

bool SourceListDirective::Allows(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (allow_star_) {
    if (url.ProtocolIsInHTTPFamily() || url.ProtocolIs("ftp") ||
        url.ProtocolIs("ws") || url.ProtocolIs("wss") ||
        policy_->ProtocolEqualsSelf(url.Protocol()))
      return true;

    return HasSourceMatchInList(url, redirect_status);
  }

  if (allow_self_ && policy_->UrlMatchesSelf(url))
    return true;

  return HasSourceMatchInList(url, redirect_status);
}

bool SourceListDirective::AllowInline() const {
  return allow_inline_;
}

bool SourceListDirective::AllowEval() const {
  return allow_eval_;
}

bool SourceListDirective::AllowWasmEval() const {
  return allow_wasm_eval_;
}

bool SourceListDirective::AllowDynamic() const {
  return allow_dynamic_;
}

bool SourceListDirective::AllowNonce(const String& nonce) const {
  String nonce_stripped = nonce.StripWhiteSpace();
  return !nonce_stripped.IsNull() && nonces_.Contains(nonce_stripped);
}

bool SourceListDirective::AllowHash(const CSPHashValue& hash_value) const {
  return hashes_.Contains(hash_value);
}

bool SourceListDirective::AllowUnsafeHashes() const {
  return allow_unsafe_hashes_;
}

bool SourceListDirective::AllowReportSample() const {
  return report_sample_;
}

bool SourceListDirective::IsNone() const {
  return !list_.size() && !allow_self_ && !allow_star_ && !allow_inline_ &&
         !allow_unsafe_hashes_ && !allow_eval_ && !allow_wasm_eval_ &&
         !allow_dynamic_ && !nonces_.size() && !hashes_.size();
}

bool SourceListDirective::IsSelf() const {
  return allow_self_ && !list_.size() && !allow_star_ && !allow_inline_ &&
         !allow_unsafe_hashes_ && !allow_eval_ && !allow_wasm_eval_ &&
         !allow_dynamic_ && !nonces_.size() && !hashes_.size();
}

uint8_t SourceListDirective::HashAlgorithmsUsed() const {
  return hash_algorithms_used_;
}

bool SourceListDirective::IsHashOrNoncePresent() const {
  return !nonces_.IsEmpty() ||
         hash_algorithms_used_ != kContentSecurityPolicyHashAlgorithmNone;
}

bool SourceListDirective::AllowsURLBasedMatching() const {
  return !allow_dynamic_ && (list_.size() || allow_star_ || allow_self_);
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void SourceListDirective::Parse(const UChar* begin, const UChar* end) {
  // We represent 'none' as an empty m_list.
  if (IsSourceListNone(begin, end))
    return;

  const UChar* position = begin;
  while (position < end) {
    SkipWhile<UChar, IsASCIISpace>(position, end);
    if (position == end)
      return;

    const UChar* begin_source = position;
    SkipWhile<UChar, IsSourceCharacter>(position, end);

    String scheme, host, path;
    int port = 0;
    CSPSource::WildcardDisposition host_wildcard = CSPSource::kNoWildcard;
    CSPSource::WildcardDisposition port_wildcard = CSPSource::kNoWildcard;

    if (ParseSource(begin_source, position, &scheme, &host, &port, &path,
                    &host_wildcard, &port_wildcard)) {
      // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
      // etc.) aren't stored in m_list, but as attributes on the source
      // list itself.
      if (scheme.IsEmpty() && host.IsEmpty())
        continue;
      if (ContentSecurityPolicy::GetDirectiveType(host) !=
          ContentSecurityPolicy::DirectiveType::kUndefined)
        policy_->ReportDirectiveAsSourceExpression(directive_name_, host);
      list_.push_back(MakeGarbageCollected<CSPSource>(
          policy_, scheme, host, port, path, host_wildcard, port_wildcard));
    } else {
      policy_->ReportInvalidSourceExpression(
          directive_name_, String(begin_source, static_cast<wtf_size_t>(
                                                    position - begin_source)));
    }

    DCHECK(position == end || IsASCIISpace(*position));
  }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool SourceListDirective::ParseSource(
    const UChar* begin,
    const UChar* end,
    String* scheme,
    String* host,
    int* port,
    String* path,
    CSPSource::WildcardDisposition* host_wildcard,
    CSPSource::WildcardDisposition* port_wildcard) {
  if (begin == end)
    return false;

  StringView token(begin, static_cast<wtf_size_t>(end - begin));

  if (EqualIgnoringASCIICase("'none'", token))
    return false;

  if (end - begin == 1 && *begin == '*') {
    AddSourceStar();
    return true;
  }

  if (EqualIgnoringASCIICase("'self'", token)) {
    AddSourceSelf();
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-inline'", token)) {
    AddSourceUnsafeInline();
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-eval'", token)) {
    AddSourceUnsafeEval();
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-allow-redirects'", token) &&
      DirectiveName() == "navigate-to") {
    AddSourceUnsafeAllowRedirects();
    return true;
  }

  if (policy_->SupportsWasmEval() &&
      EqualIgnoringASCIICase("'wasm-eval'", token)) {
    AddSourceWasmEval();
    return true;
  }

  if (EqualIgnoringASCIICase("'strict-dynamic'", token) ||
      (RuntimeEnabledFeatures::
           ExperimentalContentSecurityPolicyFeaturesEnabled() &&
       EqualIgnoringASCIICase("'csp3-strict-dynamic'", token))) {
    AddSourceStrictDynamic();
    return true;
  }

  if (EqualIgnoringASCIICase("'unsafe-hashes'", token)) {
    AddSourceUnsafeHashes();
    return true;
  }

  if (EqualIgnoringASCIICase("'report-sample'", token)) {
    AddReportSample();
    return true;
  }

  String nonce;
  if (!ParseNonce(begin, end, &nonce))
    return false;

  if (!nonce.IsNull()) {
    AddSourceNonce(nonce);
    return true;
  }

  DigestValue hash;
  ContentSecurityPolicyHashAlgorithm algorithm =
      kContentSecurityPolicyHashAlgorithmNone;
  if (!ParseHash(begin, end, &hash, &algorithm))
    return false;

  if (hash.size() > 0) {
    AddSourceHash(algorithm, hash);
    return true;
  }

  const UChar* position = begin;
  const UChar* begin_host = begin;
  const UChar* begin_path = end;
  const UChar* begin_port = nullptr;

  SkipWhile<UChar, IsNotColonOrSlash>(position, end);

  if (position == end) {
    // host
    //     ^
    return ParseHost(begin_host, position, host, host_wildcard);
  }

  if (position < end && *position == '/') {
    // host/path || host/ || /
    //     ^            ^    ^
    return ParseHost(begin_host, position, host, host_wildcard) &&
           ParsePath(position, end, path);
  }

  if (position < end && *position == ':') {
    if (end - position == 1) {
      // scheme:
      //       ^
      return ParseScheme(begin, position, scheme);
    }

    if (position[1] == '/') {
      // scheme://host || scheme://
      //       ^                ^
      if (!ParseScheme(begin, position, scheme) ||
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

  if (!ParseHost(begin_host, begin_port ? begin_port : begin_path, host,
                 host_wildcard))
    return false;

  if (begin_port) {
    if (!ParsePort(begin_port, begin_path, port, port_wildcard))
      return false;
  } else {
    *port = 0;
  }

  if (begin_path != end) {
    if (!ParsePath(begin_path, end, path))
      return false;
  }

  return true;
}

// nonce-source      = "'nonce-" nonce-value "'"
// nonce-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceListDirective::ParseNonce(const UChar* begin,
                                     const UChar* end,
                                     String* nonce) {
  size_t nonce_length = end - begin;
  StringView prefix("'nonce-");

  // TODO(esprehn): Should be StringView(begin, nonceLength).startsWith(prefix).
  if (nonce_length <= prefix.length() ||
      !EqualIgnoringASCIICase(prefix, StringView(begin, prefix.length()))) {
    // Experimentally the prefix could also be "'csp3-nonce-"
    prefix = "'csp3-nonce-";
    if (!RuntimeEnabledFeatures::
            ExperimentalContentSecurityPolicyFeaturesEnabled() ||
        nonce_length <= prefix.length() ||
        !EqualIgnoringASCIICase(prefix, StringView(begin, prefix.length()))) {
      return true;
    }
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
bool SourceListDirective::ParseHash(
    const UChar* begin,
    const UChar* end,
    DigestValue* hash,
    ContentSecurityPolicyHashAlgorithm* hash_algorithm) {
  // Any additions or subtractions from this struct should also modify the
  // respective entries in the kAlgorithmMap array in
  // ContentSecurityPolicy::FillInCSPHashValues().

  static const SupportedPrefixesStruct kSupportedPrefixes[] = {
      {"'sha256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'sha384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'sha512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'sha-256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'sha-384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'sha-512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'ed25519-", kContentSecurityPolicyHashAlgorithmEd25519}};

  static const SupportedPrefixesStruct kSupportedPrefixesExperimental[] = {
      {"'sha256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'sha384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'sha512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'sha-256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'sha-384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'sha-512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'ed25519-", kContentSecurityPolicyHashAlgorithmEd25519},
      {"'csp3-sha256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'csp3-sha384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'csp3-sha512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'csp3-sha-256-", kContentSecurityPolicyHashAlgorithmSha256},
      {"'csp3-sha-384-", kContentSecurityPolicyHashAlgorithmSha384},
      {"'csp3-sha-512-", kContentSecurityPolicyHashAlgorithmSha512},
      {"'csp3-ed25519-", kContentSecurityPolicyHashAlgorithmEd25519}};

  auto* const supportedPrefixes =
      RuntimeEnabledFeatures::ExperimentalContentSecurityPolicyFeaturesEnabled()
          ? kSupportedPrefixesExperimental
          : kSupportedPrefixes;

  const size_t supportedPrefixesLength =
      RuntimeEnabledFeatures::ExperimentalContentSecurityPolicyFeaturesEnabled()
          ? sizeof(kSupportedPrefixesExperimental) /
                sizeof(kSupportedPrefixesExperimental[0])
          : sizeof(kSupportedPrefixes) / sizeof(kSupportedPrefixes[0]);

  StringView prefix;
  *hash_algorithm = kContentSecurityPolicyHashAlgorithmNone;
  size_t hash_length = end - begin;

  for (size_t i = 0; i < supportedPrefixesLength; i++) {
    prefix = supportedPrefixes[i].prefix;
    // TODO(esprehn): Should be StringView(begin, end -
    // begin).startsWith(prefix).
    if (hash_length > prefix.length() &&
        EqualIgnoringASCIICase(prefix, StringView(begin, prefix.length()))) {
      *hash_algorithm = supportedPrefixes[i].type;
      break;
    }
  }

  if (*hash_algorithm == kContentSecurityPolicyHashAlgorithmNone)
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

  Vector<char> hash_vector;
  // We accept base64url-encoded data here by normalizing it to base64.
  Base64Decode(NormalizeToBase64(String(
                   hash_begin, static_cast<wtf_size_t>(position - hash_begin))),
               hash_vector);
  if (hash_vector.size() > kMaxDigestSize)
    return false;
  hash->Append(reinterpret_cast<uint8_t*>(hash_vector.data()),
               hash_vector.size());
  return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool SourceListDirective::ParseScheme(const UChar* begin,
                                      const UChar* end,
                                      String* scheme) {
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
bool SourceListDirective::ParseHost(
    const UChar* begin,
    const UChar* end,
    String* host,
    CSPSource::WildcardDisposition* host_wildcard) {
  DCHECK(begin <= end);
  DCHECK(host->IsEmpty());
  DCHECK(*host_wildcard == CSPSource::kNoWildcard);

  if (begin == end)
    return false;

  const UChar* position = begin;

  // Parse "*" or [ "*." ].
  if (SkipExactly<UChar>(position, end, '*')) {
    *host_wildcard = CSPSource::kHasWildcard;

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

bool SourceListDirective::ParsePath(const UChar* begin,
                                    const UChar* end,
                                    String* path) {
  DCHECK(begin <= end);
  DCHECK(path->IsEmpty());

  const UChar* position = begin;
  SkipWhile<UChar, IsPathComponentCharacter>(position, end);
  // path/to/file.js?query=string || path/to/file.js#anchor
  //                ^                               ^
  if (position < end) {
    policy_->ReportInvalidPathCharacter(
        directive_name_, String(begin, static_cast<wtf_size_t>(end - begin)),
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
bool SourceListDirective::ParsePort(
    const UChar* begin,
    const UChar* end,
    int* port,
    CSPSource::WildcardDisposition* port_wildcard) {
  DCHECK(begin <= end);
  DCHECK_EQ(*port, 0);
  DCHECK(*port_wildcard == CSPSource::kNoWildcard);

  if (!SkipExactly<UChar>(begin, end, ':'))
    NOTREACHED();

  if (begin == end)
    return false;

  if (end - begin == 1 && *begin == '*') {
    *port = 0;
    *port_wildcard = CSPSource::kHasWildcard;
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

void SourceListDirective::AddSourceSelf() {
  allow_self_ = true;
}

void SourceListDirective::AddSourceStar() {
  allow_star_ = true;
}

void SourceListDirective::AddSourceUnsafeAllowRedirects() {
  allow_redirects_ = true;
}

void SourceListDirective::AddSourceUnsafeInline() {
  allow_inline_ = true;
}

void SourceListDirective::AddSourceUnsafeEval() {
  allow_eval_ = true;
}

void SourceListDirective::AddSourceWasmEval() {
  allow_wasm_eval_ = true;
}

void SourceListDirective::AddSourceStrictDynamic() {
  allow_dynamic_ = true;
}

void SourceListDirective::AddSourceUnsafeHashes() {
  allow_unsafe_hashes_ = true;
}

void SourceListDirective::AddReportSample() {
  report_sample_ = true;
}

void SourceListDirective::AddSourceNonce(const String& nonce) {
  nonces_.insert(nonce);
}

void SourceListDirective::AddSourceHash(
    const ContentSecurityPolicyHashAlgorithm& algorithm,
    const DigestValue& hash) {
  hashes_.insert(CSPHashValue(algorithm, hash));
  hash_algorithms_used_ |= algorithm;
}

void SourceListDirective::AddSourceToMap(
    HeapHashMap<String, Member<CSPSource>>& hash_map,
    CSPSource* source) {
  hash_map.insert(source->GetScheme(), source);
  if (source->GetScheme() == "http")
    hash_map.insert("https", source);
  else if (source->GetScheme() == "ws")
    hash_map.insert("wss", source);
}

bool SourceListDirective::HasSourceMatchInList(
    const KURL& url,
    ResourceRequest::RedirectStatus redirect_status) const {
  for (wtf_size_t i = 0; i < list_.size(); ++i) {
    if (list_[i]->Matches(url, redirect_status))
      return true;
  }

  return false;
}

bool SourceListDirective::AllowAllInline() const {
  const ContentSecurityPolicy::DirectiveType& type =
      ContentSecurityPolicy::GetDirectiveType(directive_name_);
  if (type != ContentSecurityPolicy::DirectiveType::kDefaultSrc &&
      !ContentSecurityPolicy::IsScriptDirective(type) &&
      !ContentSecurityPolicy::IsStyleDirective(type)) {
    return false;
  }

  return allow_inline_ && !IsHashOrNoncePresent() &&
         (!ContentSecurityPolicy::IsScriptDirective(type) || !allow_dynamic_);
}

HeapVector<Member<CSPSource>> SourceListDirective::GetSources(
    Member<CSPSource> self) const {
  HeapVector<Member<CSPSource>> sources = list_;
  if (allow_star_) {
    sources.push_back(MakeGarbageCollected<CSPSource>(
        policy_, "ftp", String(), 0, String(), CSPSource::kNoWildcard,
        CSPSource::kNoWildcard));
    sources.push_back(MakeGarbageCollected<CSPSource>(
        policy_, "ws", String(), 0, String(), CSPSource::kNoWildcard,
        CSPSource::kNoWildcard));
    sources.push_back(MakeGarbageCollected<CSPSource>(
        policy_, "http", String(), 0, String(), CSPSource::kNoWildcard,
        CSPSource::kNoWildcard));
    if (self) {
      sources.push_back(MakeGarbageCollected<CSPSource>(
          policy_, self->GetScheme(), String(), 0, String(),
          CSPSource::kNoWildcard, CSPSource::kNoWildcard));
    }
  } else if (allow_self_ && self) {
    sources.push_back(self);
  }

  return sources;
}

bool SourceListDirective::Subsumes(
    const HeapVector<Member<SourceListDirective>>& other) const {
  if (!other.size() || other[0]->IsNone())
    return other.size();

  bool allow_inline_other = other[0]->allow_inline_;
  bool allow_eval_other = other[0]->allow_eval_;
  bool allow_wasm_eval_other = other[0]->allow_wasm_eval_;
  bool allow_dynamic_other = other[0]->allow_dynamic_;
  bool allow_unsafe_hashes = other[0]->allow_unsafe_hashes_;
  bool is_hash_or_nonce_present_other = other[0]->IsHashOrNoncePresent();
  HashSet<String> nonces_b = other[0]->nonces_;
  HashSet<CSPHashValue> hashes_b = other[0]->hashes_;

  HeapVector<Member<CSPSource>> normalized_b =
      other[0]->GetSources(other[0]->policy_->GetSelfSource());
  for (wtf_size_t i = 1; i < other.size(); i++) {
    allow_inline_other = allow_inline_other && other[i]->allow_inline_;
    allow_eval_other = allow_eval_other && other[i]->allow_eval_;
    allow_wasm_eval_other = allow_wasm_eval_other && other[i]->allow_wasm_eval_;
    allow_dynamic_other = allow_dynamic_other && other[i]->allow_dynamic_;
    allow_unsafe_hashes = allow_unsafe_hashes && other[i]->allow_unsafe_hashes_;
    is_hash_or_nonce_present_other =
        is_hash_or_nonce_present_other && other[i]->IsHashOrNoncePresent();
    nonces_b = other[i]->GetIntersectNonces(nonces_b);
    hashes_b = other[i]->GetIntersectHashes(hashes_b);
    normalized_b = other[i]->GetIntersectCSPSources(normalized_b);
  }

  if (!SubsumesNoncesAndHashes(nonces_b, hashes_b))
    return false;

  const ContentSecurityPolicy::DirectiveType type =
      ContentSecurityPolicy::GetDirectiveType(directive_name_);
  if (ContentSecurityPolicy::IsScriptDirective(type) ||
      ContentSecurityPolicy::IsStyleDirective(type)) {
    if (!allow_eval_ && allow_eval_other)
      return false;
    if (!allow_wasm_eval_ && allow_wasm_eval_other)
      return false;
    if (!allow_unsafe_hashes_ && allow_unsafe_hashes)
      return false;
    bool allow_all_inline_other =
        allow_inline_other && !is_hash_or_nonce_present_other &&
        (!ContentSecurityPolicy::IsScriptDirective(type) ||
         !allow_dynamic_other);
    if (!AllowAllInline() && allow_all_inline_other)
      return false;
  }

  if (ContentSecurityPolicy::IsScriptDirective(type) &&
      (allow_dynamic_ || allow_dynamic_other)) {
    // If `this` does not allow `strict-dynamic`, then it must be that `other`
    // does allow, so the result is `false`.
    if (!allow_dynamic_)
      return false;
    // All keyword source expressions have been considered so only CSPSource
    // subsumption is left. However, `strict-dynamic` ignores all CSPSources so
    // for subsumption to be true either `other` must allow `strict-dynamic` or
    // have no allowed CSPSources.
    return allow_dynamic_other || !normalized_b.size();
  }

  // If embedding CSP specifies `self`, `self` refers to the embedee's origin.
  HeapVector<Member<CSPSource>> normalized_a =
      GetSources(other[0]->policy_->GetSelfSource());
  return CSPSource::FirstSubsumesSecond(normalized_a, normalized_b);
}

network::mojom::blink::CSPSourceListPtr
SourceListDirective::ExposeForNavigationalChecks() const {
  WTF::Vector<network::mojom::blink::CSPSourcePtr> sources;
  for (const auto& source : list_)
    sources.push_back(source->ExposeForNavigationalChecks());

  // We do not need nonces and hashes for navigational checks
  WTF::Vector<WTF::String> nonces;
  WTF::Vector<network::mojom::blink::CSPHashSourcePtr> hashes;

  return network::mojom::blink::CSPSourceList::New(
      std::move(sources), std::move(nonces), std::move(hashes), allow_self_,
      allow_star_, allow_redirects_, allow_inline_, allow_eval_,
      allow_wasm_eval_, allow_dynamic_, allow_unsafe_hashes_, report_sample_);
}

bool SourceListDirective::SubsumesNoncesAndHashes(
    const HashSet<String>& nonces,
    const HashSet<CSPHashValue> hashes) const {
  if (!nonces.IsEmpty() && nonces_.IsEmpty())
    return false;

  for (const auto& hash : hashes) {
    if (!hashes_.Contains(hash))
      return false;
  }

  return true;
}

HashSet<String> SourceListDirective::GetIntersectNonces(
    const HashSet<String>& other) const {
  if (!nonces_.size() || !other.size())
    return !nonces_.size() ? nonces_ : other;

  HashSet<String> normalized;
  for (const auto& nonce : nonces_) {
    if (other.Contains(nonce))
      normalized.insert(nonce);
  }

  return normalized;
}

HashSet<CSPHashValue> SourceListDirective::GetIntersectHashes(
    const HashSet<CSPHashValue>& other) const {
  if (!hashes_.size() || !other.size())
    return !hashes_.size() ? hashes_ : other;

  HashSet<CSPHashValue> normalized;
  for (const auto& hash : hashes_) {
    if (other.Contains(hash))
      normalized.insert(hash);
  }

  return normalized;
}

HeapHashMap<String, Member<CSPSource>>
SourceListDirective::GetIntersectSchemesOnly(
    const HeapVector<Member<CSPSource>>& other) const {
  HeapHashMap<String, Member<CSPSource>> schemes_a;
  for (const auto& source_a : list_) {
    if (source_a->IsSchemeOnly())
      AddSourceToMap(schemes_a, source_a);
  }
  // Add schemes only sources if they are present in both `this` and `other`,
  // allowing upgrading `http` to `https` and `ws` to `wss`.
  HeapHashMap<String, Member<CSPSource>> intersect;
  for (const auto& source_b : other) {
    if (source_b->IsSchemeOnly()) {
      if (schemes_a.Contains(source_b->GetScheme()))
        AddSourceToMap(intersect, source_b);
      else if (source_b->GetScheme() == "http" && schemes_a.Contains("https"))
        intersect.insert("https", schemes_a.at("https"));
      else if (source_b->GetScheme() == "ws" && schemes_a.Contains("wss"))
        intersect.insert("wss", schemes_a.at("wss"));
    }
  }

  return intersect;
}

HeapVector<Member<CSPSource>> SourceListDirective::GetIntersectCSPSources(
    const HeapVector<Member<CSPSource>>& other) const {
  auto schemes_map = GetIntersectSchemesOnly(other);
  HeapVector<Member<CSPSource>> normalized;
  // Add all normalized scheme source expressions.
  for (const auto& it : schemes_map) {
    // We do not add secure versions if insecure schemes are present.
    if ((it.key != "https" || !schemes_map.Contains("http")) &&
        (it.key != "wss" || !schemes_map.Contains("ws"))) {
      normalized.push_back(it.value);
    }
  }

  HeapVector<Member<CSPSource>> this_vector =
      GetSources(policy_->GetSelfSource());
  for (const auto& source_a : this_vector) {
    if (schemes_map.Contains(source_a->GetScheme()))
      continue;

    for (const auto& source_b : other) {
      // No need to add a host source expression if it is subsumed by the
      // matching scheme source expression.
      if (schemes_map.Contains(source_b->GetScheme()))
        continue;
      if (CSPSource* match = source_b->Intersect(source_a))
        normalized.push_back(match);
    }
  }
  return normalized;
}

void SourceListDirective::Trace(Visitor* visitor) const {
  visitor->Trace(policy_);
  visitor->Trace(list_);
  CSPDirective::Trace(visitor);
}

}  // namespace blink
