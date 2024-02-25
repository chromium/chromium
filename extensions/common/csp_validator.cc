// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/csp_validator.h"

#include <stddef.h>

#include <initializer_list>
#include <iterator>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace extensions {

namespace csp_validator {

namespace {

const char kDefaultSrc[] = "default-src";
const char kScriptSrc[] = "script-src";
const char kObjectSrc[] = "object-src";
const char kFrameSrc[] = "frame-src";
const char kChildSrc[] = "child-src";
const char kWorkerSrc[] = "worker-src";
const char kSelfSource[] = "'self'";
const char kNoneSource[] = "'none'";
const char kWasmEvalSource[] = "'wasm-eval'";
const char kWasmUnsafeEvalSource[] = "'wasm-unsafe-eval'";

const char kDirectiveSeparator = ';';

const char kObjectSrcDefaultDirective[] = "object-src 'self';";
const char kScriptSrcDefaultDirective[] = "script-src 'self';";

const char kAppSandboxSubframeSrcDefaultDirective[] = "child-src 'self';";
const char kAppSandboxScriptSrcDefaultDirective[] =
    "script-src 'self' 'unsafe-inline' 'unsafe-eval';";

const char kSandboxDirectiveName[] = "sandbox";
const char kAllowSameOriginToken[] = "allow-same-origin";
const char kAllowTopNavigation[] = "allow-top-navigation";

// List of CSP hash-source prefixes that are accepted. Blink is a bit more
// lenient, but we only accept standard hashes to be forward-compatible.
// http://www.w3.org/TR/2015/CR-CSP2-20150721/#hash_algo
const char* const kHashSourcePrefixes[] = {
  "'sha256-",
  "'sha384-",
  "'sha512-"
};

// TODO(karandeepb): This is not the same list as used by the CSP spec. See
// https://infra.spec.whatwg.org/#ascii-whitespace.
const char kWhitespaceDelimiters[] = " \t\r\n";

using Directive = CSPParser::Directive;

// TODO(karandeepb): Rename this to DirectiveSet (as used in spec, see
// https://www.w3.org/TR/CSP/#policy-directive-set) once we ensure that this
// does not contain any duplicates.
using DirectiveList = CSPParser::DirectiveList;

bool IsLocalHostSource(const std::string& source_lower) {
  DCHECK_EQ(base::ToLowerASCII(source_lower), source_lower);

  constexpr char kLocalHost[] = "http://localhost";
  constexpr char kLocalHostIP[] = "http://127.0.0.1";

  // Subtracting 1 to exclude the null terminator '\0'.
  constexpr size_t kLocalHostLen = std::size(kLocalHost) - 1;
  constexpr size_t kLocalHostIPLen = std::size(kLocalHostIP) - 1;

  if (base::StartsWith(source_lower, kLocalHost,
                       base::CompareCase::SENSITIVE)) {
    return source_lower.length() == kLocalHostLen ||
           source_lower[kLocalHostLen] == ':';
  }

  if (base::StartsWith(source_lower, kLocalHostIP,
                       base::CompareCase::SENSITIVE)) {
    return source_lower.length() == kLocalHostIPLen ||
           source_lower[kLocalHostIPLen] == ':';
  }

  return false;
}

// Represents the status of a directive in a CSP string.
//
// Examples of directive:
// script source related: scrict-src
// subframe source related: child-src/frame-src.
class DirectiveStatus {
 public:
  // Subframe related directives can have multiple directive names: "child-src"
  // or "frame-src".
  explicit DirectiveStatus(std::vector<std::string> directives)
      : directive_names_(std::move(directives)) {}

  DirectiveStatus(const DirectiveStatus&) = delete;
  DirectiveStatus(DirectiveStatus&&) = default;

  DirectiveStatus& operator=(const DirectiveStatus&) = delete;
  DirectiveStatus& operator=(DirectiveStatus&&) = default;

  // Returns true if |directive_name| matches this DirectiveStatus.
  bool Matches(const std::string& directive_name) const {
    for (const auto& directive : directive_names_) {
      if (!base::CompareCaseInsensitiveASCII(directive_name, directive))
        return true;
    }
    return false;
  }

  bool seen_in_policy() const { return seen_in_policy_; }
  void set_seen_in_policy() { seen_in_policy_ = true; }

  const std::string& name() const {
    DCHECK(!directive_names_.empty());
    return directive_names_[0];
  }

 private:
  // The CSP directive names this DirectiveStatus cares about.
  std::vector<std::string> directive_names_;
  // Whether or not we've seen any directive name that matches |this|.
  bool seen_in_policy_ = false;
};

// Returns whether |url| starts with |scheme_and_separator| and does not have a
// too permissive wildcard host name. If |should_check_rcd| is true, then the
// Public suffix list is used to exclude wildcard TLDs such as "https://*.org".
bool isNonWildcardTLD(const std::string& url,
                      const std::string& scheme_and_separator,
                      bool should_check_rcd) {
  if (!base::StartsWith(url, scheme_and_separator,
                        base::CompareCase::SENSITIVE))
    return false;

  size_t start_of_host = scheme_and_separator.length();

  size_t end_of_host = url.find("/", start_of_host);
  if (end_of_host == std::string::npos)
    end_of_host = url.size();

  // Note: It is sufficient to only compare the first character against '*'
  // because the CSP only allows wildcards at the start of a directive, see
  // host-source and host-part at http://www.w3.org/TR/CSP2/#source-list-syntax
  bool is_wildcard_subdomain = end_of_host > start_of_host + 2 &&
      url[start_of_host] == '*' && url[start_of_host + 1] == '.';
  if (is_wildcard_subdomain)
    start_of_host += 2;

  size_t start_of_port = url.rfind(":", end_of_host);
  // The ":" check at the end of the following condition is used to avoid
  // treating the last part of an IPv6 address as a port.
  if (start_of_port > start_of_host && url[start_of_port - 1] != ':') {
    bool is_valid_port = false;
    // Do a quick sanity check. The following check could mistakenly flag
    // ":123456" or ":****" as valid, but that does not matter because the
    // relaxing CSP directive will just be ignored by Blink.
    for (size_t i = start_of_port + 1; i < end_of_host; ++i) {
      is_valid_port = base::IsAsciiDigit(url[i]) || url[i] == '*';
      if (!is_valid_port)
        break;
    }
    if (is_valid_port)
      end_of_host = start_of_port;
  }

  std::string host(url, start_of_host, end_of_host - start_of_host);
  // Global wildcards are not allowed.
  if (host.empty() || base::Contains(host, "*")) {
    return false;
  }

  if (!is_wildcard_subdomain || !should_check_rcd)
    return true;

  // Allow *.googleapis.com to be allowlisted for backwards-compatibility.
  // (crbug.com/409952)
  if (host == "googleapis.com")
    return true;

  // Wildcards on subdomains of a TLD are not allowed.
  return net::registry_controlled_domains::HostHasRegistryControlledDomain(
      host, net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// Checks whether the source is a syntactically valid hash.
bool IsHashSource(std::string_view source) {
  if (source.empty() || source.back() != '\'')
    return false;

  size_t hash_end = source.length() - 1;
  for (const char* prefix : kHashSourcePrefixes) {
    if (base::StartsWith(source, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      for (size_t i = strlen(prefix); i < hash_end; ++i) {
        const char c = source[i];
        // The hash must be base64-encoded. Do not allow any other characters.
        if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '+' &&
            c != '/' && c != '=') {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

std::string GetSecureDirectiveValues(
    int options,
    const std::string& directive_name,
    const std::vector<std::string_view>& directive_values,
    const std::string& manifest_key,
    std::vector<InstallWarning>* warnings) {
  std::vector<std::string_view> sane_csp_parts{directive_name};
  for (std::string_view source_literal : directive_values) {
    std::string source_lower = base::ToLowerASCII(source_literal);
    bool is_secure_csp_token = false;

    // We might need to relax this allowlist over time.
    if (source_lower == kSelfSource || source_lower == kNoneSource ||
        source_lower == kWasmEvalSource ||
        source_lower == kWasmUnsafeEvalSource || source_lower == "blob:" ||
        source_lower == "filesystem:" ||
        isNonWildcardTLD(source_lower, "https://", true) ||
        isNonWildcardTLD(source_lower, "chrome://", false) ||
        isNonWildcardTLD(source_lower,
                         std::string(extensions::kExtensionScheme) +
                             url::kStandardSchemeSeparator,
                         false) ||
        IsHashSource(source_literal) || IsLocalHostSource(source_lower)) {
      is_secure_csp_token = true;
    } else if ((options & OPTIONS_ALLOW_UNSAFE_EVAL) &&
               source_lower == "'unsafe-eval'") {
      is_secure_csp_token = true;
    }

    if (is_secure_csp_token) {
      sane_csp_parts.push_back(source_literal);
    } else if (warnings) {
      warnings->emplace_back(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kInvalidCSPInsecureValueIgnored, manifest_key,
              source_literal, directive_name),
          manifest_key);
    }
  }
  // End of CSP directive that was started at the beginning of this method. If
  // none of the values are secure, the policy will be empty and default to
  // 'none', which is secure.
  std::string last_part(sane_csp_parts.back());
  last_part.push_back(kDirectiveSeparator);
  sane_csp_parts.back() = last_part;
  return base::JoinString(sane_csp_parts, " ");
}

// Given a CSP directive-token for app sandbox, returns a secure value of that
// directive.
// The directive-token's name is |directive_name| and its values are splitted
// into |directive_values|.
std::string GetAppSandboxSecureDirectiveValues(
    const std::string& directive_name,
    const std::vector<std::string_view>& directive_values,
    const std::string& manifest_key,
    std::vector<InstallWarning>* warnings) {
  std::vector<std::string> sane_csp_parts{directive_name};
  bool seen_self_or_none = false;
  for (std::string_view source_literal : directive_values) {
    std::string source_lower = base::ToLowerASCII(source_literal);

    // Keyword directive sources are surrounded with quotes, e.g. 'self',
    // 'sha256-...', 'unsafe-eval', 'nonce-...'. These do not specify a remote
    // host or '*', so keep them and restrict the rest.
    if (source_lower.size() > 1u && source_lower[0] == '\'' &&
        source_lower.back() == '\'') {
      seen_self_or_none |= source_lower == "'none'" || source_lower == "'self'";
      sane_csp_parts.push_back(source_lower);
    } else if (warnings) {
      warnings->emplace_back(
          ErrorUtils::FormatErrorMessage(
              manifest_errors::kInvalidCSPInsecureValueIgnored, manifest_key,
              source_literal, directive_name),
          manifest_key);
    }
  }

  // If we haven't seen any of 'self' or 'none', that means this directive
  // value isn't secure. Specify 'self' to secure it.
  if (!seen_self_or_none)
    sane_csp_parts.push_back("'self'");

  sane_csp_parts.back().push_back(kDirectiveSeparator);
  return base::JoinString(sane_csp_parts, " ");
}

using SecureDirectiveValueFunction = base::RepeatingCallback<std::string(
    const std::string& directive_name,
    const std::vector<std::string_view>& directive_values,
    const std::string& manifest_key,
    std::vector<InstallWarning>* warnings)>;

// Represents a token in CSP string.
// Tokens are delimited by ";" CSP string.
class CSPDirectiveToken {
 public:
  // We hold a reference to |directive|, so it must remain alive longer than
  // |this|.
  explicit CSPDirectiveToken(const Directive& directive)
      : directive_(directive) {}

  CSPDirectiveToken(const CSPDirectiveToken&) = delete;
  CSPDirectiveToken& operator=(const CSPDirectiveToken&) = delete;

  // Returns true if this token affects |status|. In that case, the token's
  // directive values are secured by |secure_function|.
  bool MatchAndUpdateStatus(DirectiveStatus* status,
                            const SecureDirectiveValueFunction& secure_function,
                            const std::string& manifest_key,
                            std::vector<InstallWarning>* warnings) {
    if (!status->Matches(directive_->directive_name))
      return false;

    bool is_duplicate_directive = status->seen_in_policy();
    status->set_seen_in_policy();

    secure_value_ = secure_function.Run(
        directive_->directive_name, directive_->directive_values, manifest_key,
        // Don't show any errors for duplicate CSP directives, because it will
        // be ignored by the CSP parser
        // (http://www.w3.org/TR/CSP2/#policy-parsing). Therefore, set warnings
        // param to nullptr.
        is_duplicate_directive ? nullptr : warnings);
    return true;
  }

  std::string ToString() {
    if (secure_value_)
      return secure_value_.value();
    // This token didn't require modification.
    return std::string(directive_->directive_string) + kDirectiveSeparator;
  }

 private:
  const raw_ref<const Directive> directive_;
  std::optional<std::string> secure_value_;
};

// Class responsible for parsing a given CSP string |policy|, and enforcing
// secure directive-tokens within the policy.
//
// If a CSP directive's value is not secure, this class will use secure
// values (via |secure_function|). If a CSP directive-token is not present and
// as a result will fallback to default (possibly non-secure), this class
// will use default secure values (via GetDefaultCSPValue).
class CSPEnforcer {
 public:
  CSPEnforcer(std::string manifest_key,
              bool show_missing_csp_warnings,
              const SecureDirectiveValueFunction& secure_function)
      : manifest_key_(std::move(manifest_key)),
        show_missing_csp_warnings_(show_missing_csp_warnings),
        secure_function_(secure_function) {}

  CSPEnforcer(const CSPEnforcer&) = delete;
  CSPEnforcer& operator=(const CSPEnforcer&) = delete;

  virtual ~CSPEnforcer() {}

  // Returns the enforced CSP.
  // Emits warnings in |warnings| for insecure directive values. If
  // |show_missing_csp_warnings_| is true, these will also include missing CSP
  // directive warnings.
  std::string Enforce(const DirectiveList& directives,
                      std::vector<InstallWarning>* warnings);

 protected:
  virtual std::string GetDefaultCSPValue(const DirectiveStatus& status) = 0;

  // List of directives we care about.
  std::vector<DirectiveStatus> secure_directives_;

 private:
  const std::string manifest_key_;
  const bool show_missing_csp_warnings_;
  const SecureDirectiveValueFunction secure_function_;
};

std::string CSPEnforcer::Enforce(const DirectiveList& directives,
                                 std::vector<InstallWarning>* warnings) {
  DCHECK(!secure_directives_.empty());
  std::vector<std::string> enforced_csp_parts;

  // If any directive that we care about isn't explicitly listed in |policy|,
  // "default-src" fallback is used.
  DirectiveStatus default_src_status({kDefaultSrc});
  std::vector<InstallWarning> default_src_csp_warnings;

  for (const auto& directive : directives) {
    CSPDirectiveToken csp_directive_token(directive);
    bool matches_enforcing_directive = false;
    for (DirectiveStatus& status : secure_directives_) {
      if (csp_directive_token.MatchAndUpdateStatus(&status, secure_function_,
                                                   manifest_key_, warnings)) {
        matches_enforcing_directive = true;
        break;
      }
    }
    if (!matches_enforcing_directive) {
      csp_directive_token.MatchAndUpdateStatus(&default_src_status,
                                               secure_function_, manifest_key_,
                                               &default_src_csp_warnings);
    }

    enforced_csp_parts.push_back(csp_directive_token.ToString());
  }

  if (default_src_status.seen_in_policy()) {
    for (const DirectiveStatus& status : secure_directives_) {
      if (!status.seen_in_policy()) {
        // This |status| falls back to "default-src". So warnings from
        // "default-src" will apply.
        if (warnings) {
          warnings->insert(
              warnings->end(),
              std::make_move_iterator(default_src_csp_warnings.begin()),
              std::make_move_iterator(default_src_csp_warnings.end()));
        }
        break;
      }
    }
  } else {
    // Did not see "default-src".
    // Make sure we cover all sources from |secure_directives_|.
    for (const DirectiveStatus& status : secure_directives_) {
      if (status.seen_in_policy()) {  // Already covered.
        continue;
      }
      enforced_csp_parts.push_back(GetDefaultCSPValue(status));

      if (warnings && show_missing_csp_warnings_) {
        warnings->emplace_back(ErrorUtils::FormatErrorMessage(
                                   manifest_errors::kInvalidCSPMissingSecureSrc,
                                   manifest_key_, status.name()),
                               manifest_key_);
      }
    }
  }

  return base::JoinString(enforced_csp_parts, " ");
}

class ExtensionCSPEnforcer : public CSPEnforcer {
 public:
  ExtensionCSPEnforcer(std::string manifest_key,
                       bool allow_insecure_object_src,
                       int options)
      : CSPEnforcer(std::move(manifest_key),
                    true,
                    base::BindRepeating(&GetSecureDirectiveValues, options)) {
    secure_directives_.emplace_back(std::vector<std::string>({kScriptSrc}));
    if (!allow_insecure_object_src)
      secure_directives_.emplace_back(std::vector<std::string>({kObjectSrc}));
  }

  ExtensionCSPEnforcer(const ExtensionCSPEnforcer&) = delete;
  ExtensionCSPEnforcer& operator=(const ExtensionCSPEnforcer&) = delete;

 protected:
  std::string GetDefaultCSPValue(const DirectiveStatus& status) override {
    if (status.Matches(kObjectSrc))
      return kObjectSrcDefaultDirective;
    DCHECK(status.Matches(kScriptSrc));
    return kScriptSrcDefaultDirective;
  }
};

class AppSandboxPageCSPEnforcer : public CSPEnforcer {
 public:
  AppSandboxPageCSPEnforcer(std::string manifest_key)
      : CSPEnforcer(std::move(manifest_key),
                    false,
                    base::BindRepeating(&GetAppSandboxSecureDirectiveValues)) {
    secure_directives_.emplace_back(
        std::vector<std::string>({kChildSrc, kFrameSrc}));
    secure_directives_.emplace_back(std::vector<std::string>({kScriptSrc}));
  }

  AppSandboxPageCSPEnforcer(const AppSandboxPageCSPEnforcer&) = delete;
  AppSandboxPageCSPEnforcer& operator=(const AppSandboxPageCSPEnforcer&) =
      delete;

 protected:
  std::string GetDefaultCSPValue(const DirectiveStatus& status) override {
    if (status.Matches(kChildSrc))
      return kAppSandboxSubframeSrcDefaultDirective;
    DCHECK(status.Matches(kScriptSrc));
    return kAppSandboxScriptSrcDefaultDirective;
  }
};

}  //  namespace

bool ContentSecurityPolicyIsLegal(const std::string& policy) {
  // We block these characters to prevent HTTP header injection when
  // representing the content security policy as an HTTP header.
  const char kBadChars[] = {',', '\r', '\n', '\0'};

  return policy.find_first_of(kBadChars, 0, std::size(kBadChars)) ==
         std::string::npos;
}

Directive::Directive(std::string_view directive_string,
                     std::string directive_name,
                     std::vector<std::string_view> directive_values)
    : directive_string(directive_string),
      directive_name(std::move(directive_name)),
      directive_values(std::move(directive_values)) {
  // |directive_name| should be lower cased.
  // Note: Using |this->directive_name|, because |directive_name| refers to the
  // already-moved-from input parameter.
  DCHECK(base::ranges::none_of(this->directive_name, base::IsAsciiUpper<char>));
}

CSPParser::Directive::~Directive() = default;
CSPParser::Directive::Directive(Directive&&) = default;
CSPParser::Directive& Directive::operator=(Directive&&) = default;

CSPParser::CSPParser(std::string policy) : policy_(std::move(policy)) {
  Parse();
}
CSPParser::~CSPParser() = default;

void CSPParser::Parse() {
  // See http://www.w3.org/TR/CSP/#parse-a-csp-policy for parsing algorithm.
  for (const std::string_view directive_str : base::SplitStringPiece(
           policy_, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // Get whitespace separated tokens.
    std::vector<std::string_view> tokens = base::SplitStringPiece(
        directive_str, kWhitespaceDelimiters, base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);

    // |directive_str| is non-empty and has had whitespace trimmed. Hence, it
    // must contain some non-whitespace characters.
    DCHECK(!tokens.empty());

    // TODO(karandeepb): As per http://www.w3.org/TR/CSP/#parse-a-csp-policy,
    // we should ignore duplicate directive names. We should raise an install
    // warning for them.
    std::string directive_name = base::ToLowerASCII(tokens[0]);

    // Erase the first token, the directive name.
    tokens.erase(tokens.begin());

    directives_.emplace_back(directive_str, std::move(directive_name),
                             std::move(tokens));
  }
}

std::string SanitizeContentSecurityPolicy(
    const std::string& policy,
    std::string manifest_key,
    int options,
    std::vector<InstallWarning>* warnings) {
  CSPParser csp_parser(policy);

  bool allow_insecure_object_src = options & OPTIONS_ALLOW_INSECURE_OBJECT_SRC;
  ExtensionCSPEnforcer csp_enforcer(std::move(manifest_key),
                                    allow_insecure_object_src, options);
  return csp_enforcer.Enforce(csp_parser.directives(), warnings);
}

std::string GetSandboxedPageCSPDisallowingRemoteSources(
    const std::string& policy,
    std::string manifest_key,
    std::vector<InstallWarning>* warnings) {
  CSPParser csp_parser(policy);
  AppSandboxPageCSPEnforcer csp_enforcer(std::move(manifest_key));
  return csp_enforcer.Enforce(csp_parser.directives(), warnings);
}

bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Manifest::Type type) {
  bool seen_sandbox = false;
  CSPParser parser(policy);
  for (const auto& directive : parser.directives()) {
    if (directive.directive_name != kSandboxDirectiveName)
      continue;

    seen_sandbox = true;

    for (std::string_view token : directive.directive_values) {
      std::string token_lower_case = base::ToLowerASCII(token);

      // The same origin token negates the sandboxing.
      if (token_lower_case == kAllowSameOriginToken)
        return false;

      // Platform apps don't allow navigation.
      if (type == Manifest::TYPE_PLATFORM_APP &&
          token_lower_case == kAllowTopNavigation) {
        return false;
      }
    }
  }

  return seen_sandbox;
}

bool DoesCSPDisallowRemoteCode(const std::string& content_security_policy,
                               std::string_view manifest_key,
                               std::u16string* error) {
  DCHECK(error);

  struct DirectiveMapping {
    DirectiveMapping(DirectiveStatus status) : status(std::move(status)) {}

    DirectiveStatus status;
    raw_ptr<const CSPParser::Directive, DanglingUntriaged> directive = nullptr;
    bool required = true;
  };

  DirectiveMapping script_src_mapping(DirectiveStatus({kScriptSrc}));
  DirectiveMapping worker_src_mapping(DirectiveStatus({kWorkerSrc}));
  DirectiveMapping default_src_mapping(DirectiveStatus({kDefaultSrc}));

  DirectiveMapping object_src_mapping(DirectiveStatus({kObjectSrc}));
  object_src_mapping.required = false;

  DirectiveMapping* directive_mappings[] = {
      &script_src_mapping,
      &object_src_mapping,
      &worker_src_mapping,
      &default_src_mapping,
  };

  // Populate |directive_mappings|.
  CSPParser csp_parser(content_security_policy);
  for (DirectiveMapping* mapping : directive_mappings) {
    // Find the first matching directive. As per
    // http://www.w3.org/TR/CSP/#parse-a-csp-policy, duplicate directive names
    // are ignored.
    auto it = base::ranges::find_if(
        csp_parser.directives(),
        [mapping](const CSPParser::Directive& directive) {
          return mapping->status.Matches(directive.directive_name);
        });

    if (it != csp_parser.directives().end())
      mapping->directive = &(*it);
  }

  auto fallback_if_necessary = [](DirectiveMapping* from,
                                  const DirectiveMapping& to) {
    DCHECK(from);

    // No fallback necessary.
    if (from->directive)
      return;

    from->directive = to.directive;
  };

  // "script-src" fallbacks to "default-src".
  fallback_if_necessary(&script_src_mapping, default_src_mapping);

  // "worker-src" fallbacks to "script-src", which might itself fallback to
  // "default-src".
  fallback_if_necessary(&worker_src_mapping, script_src_mapping);

  // Note: Even though "object-src" will fall back to default-src in the CSP
  // enforcement, we don't fall back to it here. This allows developers to
  // specify a default-src with a remote target without needing to separately
  // specify an object-src.

  auto is_secure_directive = [manifest_key](const DirectiveMapping& mapping,
                                            std::u16string* error) {
    if (!mapping.directive) {
      if (mapping.required) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            manifest_errors::kInvalidCSPMissingSecureSrc, manifest_key,
            mapping.status.name());
        return false;
      }

      // The directive wasn't present, but isn't required. Allow it.
      return true;
    }

    auto directive_values = mapping.directive->directive_values;
    auto it = base::ranges::find_if_not(
        directive_values, [](std::string_view source) {
          std::string source_lower = base::ToLowerASCII(source);

          return source_lower == kSelfSource || source_lower == kNoneSource ||
                 IsLocalHostSource(source_lower) ||
                 source_lower == kWasmUnsafeEvalSource;
        });

    if (it == directive_values.end())
      return true;

    *error = ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidCSPInsecureValueError, manifest_key, *it,
        mapping.status.name());
    return false;
  };

  std::set<const CSPParser::Directive*> secure_directives;
  for (const DirectiveMapping* mapping : directive_mappings) {
    // We don't need "default-src" to be a secure directive. Ignore it.
    if (mapping == &default_src_mapping)
      continue;

    if (mapping->directive && secure_directives.count(mapping->directive)) {
      // We already checked this directive and know it's secure. Skip it.
      continue;
    }

    if (!is_secure_directive(*mapping, error))
      return false;

    if (!mapping->directive) {
      DCHECK(!mapping->required);
      continue;
    }

    secure_directives.insert(mapping->directive);
  }

  return true;
}

}  // namespace csp_validator

}  // namespace extensions
