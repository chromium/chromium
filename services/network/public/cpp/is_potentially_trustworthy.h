// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace network {

// Returns whether an origin is potentially trustworthy according to
// https://www.w3.org/TR/powerful-features/#is-origin-trustworthy.
//
// This function is safe to be called from any thread.
//
// See also blink::SecurityOrigin::isPotentiallyTrustworthy.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsOriginPotentiallyTrustworthy(const url::Origin& origin);

// Returns whether a URL is potentially trustworthy according to
// https://www.w3.org/TR/powerful-features/#is-url-trustworthy.
//
// This function is safe to be called from any thread.
COMPONENT_EXPORT(NETWORK_CPP)
bool IsUrlPotentiallyTrustworthy(const GURL& url);

// Helper class for maintaining an allowlist of origins and hostname patterns
// that should be considered trustworthy.
//
// The allowlist is a sum of 1) the --unsafely-treat-insecure-origin-as-secure
// command-line parameter, 2) the argument passed to SetAuxiliaryAllowlist (e.g.
// the value taken from the enterprise policy).
//
// The argument passed to --unsafely-treat-insecure-origin-as-secure should be a
// comma-separated list of origins and wildcard hostname patterns up to eTLD+1.
// For example, the list may contain "http://foo.com", "http://foo.com:8000",
// "*.foo.com", "*.foo.*.bar.com", and "http://*.foo.bar.com", but not
// "*.co.uk", "*.com", or "test.*.com". Hostname patterns must contain a
// wildcard somewhere (so "test.com" is not a valid pattern) and wildcards can
// only replace full components ("test*.foo.com" is not valid).
//
// Plain origins ("http://foo.com") are canonicalized when they are inserted
// into this list by converting to url::Origin and serializing. For hostname
// patterns, each component is individually canonicalized.
//
// See also https://www.w3.org/TR/powerful-features/#is-origin-trustworthy -
// this class handles marking origins as "configured as a trustworthy origin".
//
// Note: all methods of this class are thread-safe.
class COMPONENT_EXPORT(NETWORK_CPP) SecureOriginAllowlist {
 public:
  static SecureOriginAllowlist& GetInstance();

  // Returns true if |origin| has a match in the secure origin allowlist.
  bool IsOriginAllowlisted(const url::Origin& origin);

  // Returns the current allowlist, combining 1) the
  // --unsafely-treat-insecure-origin-as-secure command-line parameter, 2) the
  // argument passed to SetAuxiliaryAllowlist (e.g. the value taken from the
  // enterprise policy).
  //
  // This method is safe to be called from any thread.
  std::vector<std::string> GetCurrentAllowlist();

  // Allows setting allowlist from an additional source (e.g. from an enterprise
  // policy).
  //
  // As with --unsafely-treat-insecure-origin-as-secure, the
  // |auxiliary_allowlist| will be canonicalized (for more details see the
  // class-level comment above).  Input patterns that fail validation and
  // canonicalization will not be added, and will be appended to the optional
  // |rejected_patterns| output parameter.
  //
  // This method is safe to be called from any thread.
  void SetAuxiliaryAllowlist(const std::string& auxiliary_allowlist,
                             std::vector<std::string>* rejected_patterns);

  // Empties the secure origin allowlist.
  //
  // This function is safe to be called from any thread.
  void ResetForTesting();

  // For unit tests.
  static std::vector<std::string> CanonicalizeAllowlistForTesting(
      const std::vector<std::string>& allowlist,
      std::vector<std::string>* rejected_patterns);

 private:
  friend class base::NoDestructor<SecureOriginAllowlist>;
  SecureOriginAllowlist();
  ~SecureOriginAllowlist() = delete;

  void ParseCmdlineIfNeeded() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::Lock lock_;

  std::vector<std::string> cmdline_allowlist_ GUARDED_BY(lock_);
  bool has_cmdline_been_parsed_ GUARDED_BY(lock_) = false;

  std::vector<std::string> auxiliary_allowlist_ GUARDED_BY(lock_);

  DISALLOW_COPY_AND_ASSIGN(SecureOriginAllowlist);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_IS_POTENTIALLY_TRUSTWORTHY_H_
