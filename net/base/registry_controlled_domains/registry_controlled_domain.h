// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NB: Modelled after Mozilla's code (originally written by Pamela Greene,
// later modified by others), but almost entirely rewritten for Chrome.
//   (netwerk/dns/src/nsEffectiveTLDService.h)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla TLD Service
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pamela Greene <pamg.bugs@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
  (Documentation based on the Mozilla documentation currently at
  http://wiki.mozilla.org/Gecko:Effective_TLD_Service, written by the same
  author.)

  The RegistryControlledDomainService examines the hostname of a GURL passed to
  it and determines the longest portion that is controlled by a registrar.
  Although technically the top-level domain (TLD) for a hostname is the last
  dot-portion of the name (such as .com or .org), many domains (such as co.uk)
  function as though they were TLDs, allocating any number of more specific,
  essentially unrelated names beneath them.  For example, .uk is a TLD, but
  nobody is allowed to register a domain directly under .uk; the "effective"
  TLDs are ac.uk, co.uk, and so on.  We wouldn't want to allow any site in
  *.co.uk to set a cookie for the entire co.uk domain, so it's important to be
  able to identify which higher-level domains function as effective TLDs and
  which can be registered.

  The service obtains its information about effective TLDs from a text resource
  that must be in the following format:

  * It should use plain ASCII.
  * It should contain one domain rule per line, terminated with \n, with nothing
    else on the line.  (The last rule in the file may omit the ending \n.)
  * Rules should have been normalized using the same canonicalization that GURL
    applies.  For ASCII, that means they're not case-sensitive, among other
    things; other normalizations are applied for other characters.
  * Each rule should list the entire TLD-like domain name, with any subdomain
    portions separated by dots (.) as usual.
  * Rules should neither begin nor end with a dot.
  * If a hostname matches more than one rule, the most specific rule (that is,
    the one with more dot-levels) will be used.
  * Other than in the case of wildcards (see below), rules do not implicitly
    include their subcomponents.  For example, "bar.baz.uk" does not imply
    "baz.uk", and if "bar.baz.uk" is the only rule in the list, "foo.bar.baz.uk"
    will match, but "baz.uk" and "qux.baz.uk" won't.
  * The wildcard character '*' will match any valid sequence of characters.
  * Wildcards may only appear as the entire most specific level of a rule.  That
    is, a wildcard must come at the beginning of a line and must be followed by
    a dot.  (You may not use a wildcard as the entire rule.)
  * A wildcard rule implies a rule for the entire non-wildcard portion.  For
    example, the rule "*.foo.bar" implies the rule "foo.bar" (but not the rule
    "bar").  This is typically important in the case of exceptions (see below).
  * The exception character '!' before a rule marks an exception to a wildcard
    rule.  If your rules are "*.tokyo.jp" and "!pref.tokyo.jp", then
    "a.b.tokyo.jp" has an effective TLD of "b.tokyo.jp", but "a.pref.tokyo.jp"
    has an effective TLD of "tokyo.jp" (the exception prevents the wildcard
    match, and we thus fall through to matching on the implied "tokyo.jp" rule
    from the wildcard).
  * If you use an exception rule without a corresponding wildcard rule, the
    behavior is undefined.

  Firefox has a very similar service, and it's their data file we use to
  construct our resource.  However, the data expected by this implementation
  differs from the Mozilla file in several important ways:
   (1) We require that all single-level TLDs (com, edu, etc.) be explicitly
       listed.  As of this writing, Mozilla's file includes the single-level
       TLDs too, but that might change.
   (2) Our data is expected be in pure ASCII: all UTF-8 or otherwise encoded
       items must already have been normalized.
   (3) We do not allow comments, rule notes, blank lines, or line endings other
       than LF.
  Rules are also expected to be syntactically valid.

  The utility application tld_cleanup.exe converts a Mozilla-style file into a
  Chrome one, making sure that single-level TLDs are explicitly listed, using
  GURL to normalize rules, and validating the rules.
*/

#ifndef NET_BASE_REGISTRY_CONTROLLED_DOMAINS_REGISTRY_CONTROLLED_DOMAIN_H_
#define NET_BASE_REGISTRY_CONTROLLED_DOMAINS_REGISTRY_CONTROLLED_DOMAIN_H_

#include <stddef.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "net/base/net_export.h"

class GURL;

namespace url {
class Origin;
}

struct DomainRule;

namespace net::registry_controlled_domains {

// This enum is a required parameter to all public methods declared for this
// service. The Public Suffix List (http://publicsuffix.org/) this service
// uses as a data source splits all effective-TLDs into two groups. The main
// group describes registries that are acknowledged by ICANN. The second group
// contains a list of private additions for domains that enable external users
// to create subdomains, such as appspot.com.
// The RegistryFilter enum lets you choose whether you want to include the
// private additions in your lookup.
// See this for example use cases:
// https://wiki.mozilla.org/Public_Suffix_List/Use_Cases
enum PrivateRegistryFilter {
  EXCLUDE_PRIVATE_REGISTRIES = 0,
  INCLUDE_PRIVATE_REGISTRIES
};

// This enum is a required parameter to the GetRegistryLength functions
// declared for this service. Whenever there is no matching rule in the
// effective-TLD data (or in the default data, if the resource failed to
// load), the result will be dependent on which enum value was passed in.
// If EXCLUDE_UNKNOWN_REGISTRIES was passed in, the resulting registry length
// will be 0. If INCLUDE_UNKNOWN_REGISTRIES was passed in, the resulting
// registry length will be the length of the last subcomponent (eg. 3 for
// foobar.baz).
enum UnknownRegistryFilter {
  EXCLUDE_UNKNOWN_REGISTRIES = 0,
  INCLUDE_UNKNOWN_REGISTRIES
};

// Returns the registered, organization-identifying host and all its registry
// information, but no subdomains, from the given GURL.  Returns an empty
// string if the GURL is invalid, has no host (e.g. a file: URL), has multiple
// trailing dots, is an IP address, has only one subcomponent (i.e. no dots
// other than leading/trailing ones), or is itself a recognized registry
// identifier.  If no matching rule is found in the effective-TLD data (or in
// the default data, if the resource failed to load), the last subcomponent of
// the host is assumed to be the registry.
//
// Examples:
//   http://www.google.com/file.html -> "google.com"  (com)
//   http://..google.com/file.html   -> "google.com"  (com)
//   http://google.com./file.html    -> "google.com." (com)
//   http://a.b.co.uk/file.html      -> "b.co.uk"     (co.uk)
//   file:///C:/bar.html             -> ""            (no host)
//   http://foo.com../file.html      -> ""            (multiple trailing dots)
//   http://192.168.0.1/file.html    -> ""            (IP address)
//   http://bar/file.html            -> ""            (no subcomponents)
//   http://co.uk/file.html          -> ""            (host is a registry)
//   http://foo.bar/file.html        -> "foo.bar"     (no rule; assume bar)
NET_EXPORT std::string GetDomainAndRegistry(const GURL& gurl,
                                            PrivateRegistryFilter filter);

// Like the GURL version, but takes an Origin. Returns an empty string if the
// Origin is opaque.
NET_EXPORT std::string GetDomainAndRegistry(const url::Origin& origin,
                                            PrivateRegistryFilter filter);

// Like the GURL / Origin versions, but takes a host (which is canonicalized
// internally). Prefer either the GURL or Origin variants instead of this one
// to avoid needing to re-canonicalize the host.
NET_EXPORT std::string GetDomainAndRegistry(std::string_view host,
                                            PrivateRegistryFilter filter);

// Same as above, but returns a StringPiece that is backed by the supplied
// url::Origin.
NET_EXPORT std::string_view GetDomainAndRegistryAsStringPiece(
    const url::Origin& origin,
    PrivateRegistryFilter filter);

// These convenience functions return true if the two GURLs or Origins both have
// hosts and one of the following is true:
// * The hosts are identical.
// * They each have a known domain and registry, and it is the same for both
//   URLs.  Note that this means the trailing dot, if any, must match too.
// Effectively, callers can use this function to check whether the input URLs
// represent hosts "on the same site".
NET_EXPORT bool SameDomainOrHost(const GURL& gurl1, const GURL& gurl2,
                                 PrivateRegistryFilter filter);
NET_EXPORT bool SameDomainOrHost(const url::Origin& origin1,
                                 const url::Origin& origin2,
                                 PrivateRegistryFilter filter);
// Note: this returns false if |origin2| is not set.
NET_EXPORT bool SameDomainOrHost(const url::Origin& origin1,
                                 const std::optional<url::Origin>& origin2,
                                 PrivateRegistryFilter filter);
NET_EXPORT bool SameDomainOrHost(const GURL& gurl,
                                 const url::Origin& origin,
                                 PrivateRegistryFilter filter);

// Finds the length in bytes of the registrar portion of the host in the
// given GURL.  Returns std::string::npos if the GURL is invalid or has no
// host (e.g. a file: URL).  Returns 0 if the GURL has multiple trailing dots,
// is an IP address, has no subcomponents, or is itself a recognized registry
// identifier.  The result is also dependent on the UnknownRegistryFilter.
// If no matching rule is found in the effective-TLD data (or in
// the default data, if the resource failed to load), returns 0 if
// |unknown_filter| is EXCLUDE_UNKNOWN_REGISTRIES, or the length of the last
// subcomponent if |unknown_filter| is INCLUDE_UNKNOWN_REGISTRIES.
//
// Examples:
//   http://www.google.com/file.html -> 3                 (com)
//   http://..google.com/file.html   -> 3                 (com)
//   http://google.com./file.html    -> 4                 (com)
//   http://a.b.co.uk/file.html      -> 5                 (co.uk)
//   file:///C:/bar.html             -> std::string::npos (no host)
//   http://foo.com../file.html      -> 0                 (multiple trailing
//                                                         dots)
//   http://192.168.0.1/file.html    -> 0                 (IP address)
//   http://bar/file.html            -> 0                 (no subcomponents)
//   http://co.uk/file.html          -> 0                 (host is a registry)
//   http://foo.bar/file.html        -> 0 or 3, depending (no rule; assume
//                                                         bar)
NET_EXPORT size_t GetRegistryLength(const GURL& gurl,
                                    UnknownRegistryFilter unknown_filter,
                                    PrivateRegistryFilter private_filter);

// Returns true if the given host name has a registry-controlled domain. The
// host name will be internally canonicalized. Also returns true for invalid
// host names like "*.google.com" as long as it has a valid registry-controlled
// portion (see PermissiveGetHostRegistryLength for particulars).
NET_EXPORT bool HostHasRegistryControlledDomain(
    std::string_view host,
    UnknownRegistryFilter unknown_filter,
    PrivateRegistryFilter private_filter);

// Returns true if the given host name is a registry identifier. The name should
// be already canonicalized, and not an IP address. This returns true for
// registries specified by wildcard rules as well as non-wildcard rules. For
// example, if there is a wildcard rule of "foo.bar", then "a.foo.bar" is
// considered a registry identifier.
NET_EXPORT bool HostIsRegistryIdentifier(std::string_view canon_host,
                                         PrivateRegistryFilter private_filter);

// Like GetRegistryLength, but takes a previously-canonicalized host instead of
// a GURL. Prefer the GURL version or HasRegistryControlledDomain to eliminate
// the possibility of bugs with non-canonical hosts.
//
// If you have a non-canonical host name, use the "Permissive" version instead.
NET_EXPORT size_t
GetCanonicalHostRegistryLength(std::string_view canon_host,
                               UnknownRegistryFilter unknown_filter,
                               PrivateRegistryFilter private_filter);

// Like GetRegistryLength for a potentially non-canonicalized hostname.  This
// splits the input into substrings at '.' characters, then attempts to
// piecewise-canonicalize the substrings. After finding the registry length of
// the concatenated piecewise string, it then maps back to the corresponding
// length in the original input string.
//
// It will also handle hostnames that are otherwise invalid as long as they
// contain a valid registry controlled domain at the end. Invalid dot-separated
// portions of the domain will be left as-is when the string is looked up in
// the registry database (which will result in no match).
//
// This will handle all cases except for the pattern:
//   <invalid-host-chars> <non-literal-dot> <valid-registry-controlled-domain>
// For example:
//   "%00foo%2Ecom" (would canonicalize to "foo.com" if the "%00" was removed)
// A non-literal dot (like "%2E" or a fullwidth period) will normally get
// canonicalized to a dot if the host chars were valid. But since the %2E will
// be in the same substring as the %00, the substring will fail to
// canonicalize, the %2E will be left escaped, and the valid registry
// controlled domain at the end won't match.
//
// The string won't be trimmed, so things like trailing spaces will be
// considered part of the host and therefore won't match any TLD. It will
// return std::string::npos like GetRegistryLength() for empty input, but
// because invalid portions are skipped, it won't return npos in any other case.
NET_EXPORT size_t
PermissiveGetHostRegistryLength(std::string_view host,
                                UnknownRegistryFilter unknown_filter,
                                PrivateRegistryFilter private_filter);
NET_EXPORT size_t
PermissiveGetHostRegistryLength(std::u16string_view host,
                                UnknownRegistryFilter unknown_filter,
                                PrivateRegistryFilter private_filter);

typedef const struct DomainRule* (*FindDomainPtr)(const char *, unsigned int);

// Used for unit tests. Uses default domains.
NET_EXPORT_PRIVATE void ResetFindDomainGraphForTesting();

// Used for unit tests, so that a frozen list of domains is used.
NET_EXPORT_PRIVATE void SetFindDomainGraphForTesting(
    base::span<const uint8_t> domains);

}  // namespace net::registry_controlled_domains

#endif  // NET_BASE_REGISTRY_CONTROLLED_DOMAINS_REGISTRY_CONTROLLED_DOMAIN_H_
