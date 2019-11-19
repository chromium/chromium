// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CSP_VALIDATOR_H_
#define EXTENSIONS_COMMON_CSP_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace csp_validator {

// Checks whether the given |policy| is legal for use in the extension system.
// This check just ensures that the policy doesn't contain any characters that
// will cause problems when we transmit the policy in an HTTP header.
bool ContentSecurityPolicyIsLegal(const std::string& policy);

// This specifies options for configuring which CSP directives are permitted in
// extensions.
enum Options {
  OPTIONS_NONE = 0,
  // Allows 'unsafe-eval' to be specified as a source in a directive.
  OPTIONS_ALLOW_UNSAFE_EVAL = 1 << 0,
  // Allow an object-src to be specified with any sources (i.e. it may contain
  // wildcards or http sources). Specifying this requires the CSP to contain
  // a plugin-types directive which restricts the plugins that can be loaded
  // to those which are fully sandboxed.
  OPTIONS_ALLOW_INSECURE_OBJECT_SRC = 1 << 1,
};

// Helper to parse a serialized content security policy string.
// Exposed for testing.
class CSPParser {
 public:
  // Represents a CSP directive.
  // E.g. for the directive "script-src 'self' www.google.com"
  // |directive_string| is "script-src 'self' www.google.com".
  // |directive_name| is "script_src".
  // |directive_values| is ["'self'", "www.google.com"].
  struct Directive {
    Directive(base::StringPiece directive_string,
              std::string directive_name,
              std::vector<base::StringPiece> directive_values);
    ~Directive();
    Directive(Directive&&);
    Directive& operator=(Directive&&);

    base::StringPiece directive_string;

    // Must be lower case.
    std::string directive_name;

    std::vector<base::StringPiece> directive_values;

    DISALLOW_COPY_AND_ASSIGN(Directive);
  };

  using DirectiveList = std::vector<Directive>;

  CSPParser(std::string policy);
  ~CSPParser();

  // It's not safe to move CSPParser since |directives_| refers to memory owned
  // by |policy_|. Once move constructed, |directives_| will end up being in an
  // invalid state, as it will point to memory owned by a "moved" string
  // instance.
  CSPParser(CSPParser&&) = delete;
  CSPParser& operator=(CSPParser&&) = delete;

  // This can contain duplicate directives (directives having the same directive
  // name).
  const DirectiveList& directives() const { return directives_; }

 private:
  void Parse();

  const std::string policy_;

  // This refers to memory owned by |policy_|.
  DirectiveList directives_;

  DISALLOW_COPY_AND_ASSIGN(CSPParser);
};

// Checks whether the given |policy| meets the minimum security requirements
// for use in the extension system.
//
// Ideally, we would like to say that an XSS vulnerability in the extension
// should not be able to execute script, even in the precense of an active
// network attacker.
//
// However, we found that it broke too many deployed extensions to limit
// 'unsafe-eval' in the script-src directive, so that is allowed as a special
// case for extensions. Platform apps disallow it.
//
// |options| is a bitmask of Options.
//
// If |warnings| is not NULL, any validation errors are appended to |warnings|.
// Returns the sanitized policy.
std::string SanitizeContentSecurityPolicy(
    const std::string& policy,
    std::string manifest_key,
    int options,
    std::vector<InstallWarning>* warnings);

// Given the Content Security Policy of an app sandbox page, returns the
// effective CSP for that sandbox page.
//
// The effective policy restricts the page from loading external web content
// (frames and scripts) within the page. This is done through adding 'self'
// directive source to relevant CSP directive names.
//
// If |warnings| is not nullptr, any validation errors are appended to
// |warnings|.
std::string GetEffectiveSandoxedPageCSP(const std::string& policy,
                                        std::string manifest_key,
                                        std::vector<InstallWarning>* warnings);

// Checks whether the given |policy| enforces a unique origin sandbox as
// defined by http://www.whatwg.org/specs/web-apps/current-work/multipage/
// the-iframe-element.html#attr-iframe-sandbox. The policy must have the
// "sandbox" directive, and the sandbox tokens must not include
// "allow-same-origin". Additional restrictions may be imposed depending on
// |type|.
bool ContentSecurityPolicyIsSandboxed(
    const std::string& policy, Manifest::Type type);

// Returns whether the given |content_security_policy| prevents remote scripts.
// If not, populates |error|.
bool DoesCSPDisallowRemoteCode(const std::string& content_security_policy,
                               base::StringPiece manifest_key,
                               base::string16* error);

}  // namespace csp_validator

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CSP_VALIDATOR_H_
