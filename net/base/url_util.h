// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a set of utility functions related to parsing,
// manipulating, and interacting with URLs and hostnames. These functions are
// intended to be of a text-processing nature, and should not attempt to use any
// networking or blocking services.

#ifndef NET_BASE_URL_UTIL_H_
#define NET_BASE_URL_UTIL_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "url/third_party/mozilla/url_parse.h"

class GURL;

namespace url {
struct CanonHostInfo;
}

namespace net {

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
NET_EXPORT GURL AppendQueryParameter(const GURL& url,
                                     const std::string& name,
                                     const std::string& value);

// Returns a new GURL by appending or replacing the given query parameter name
// and the value. If |name| appears more than once, only the first name-value
// pair is replaced. Unsafe characters in the name and the value are escaped
// like %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com"), "name", "new").spec()
// => "http://example.com?name=value"
// AppendOrReplaceQueryParameter(
//     GURL("http://example.com?x=y&name=old"), "name", "new").spec()
// => "http://example.com?x=y&name=new"
NET_EXPORT GURL AppendOrReplaceQueryParameter(const GURL& url,
                                              const std::string& name,
                                              const std::string& value);

// Iterates over the key-value pairs in the query portion of |url|.
class NET_EXPORT QueryIterator {
 public:
  explicit QueryIterator(const GURL& url);
  ~QueryIterator();

  std::string GetKey() const;
  std::string GetValue() const;
  const std::string& GetUnescapedValue();

  bool IsAtEnd() const;
  void Advance();

 private:
  const GURL& url_;
  url::Component query_;
  bool at_end_;
  url::Component key_;
  url::Component value_;
  std::string unescaped_value_;

  DISALLOW_COPY_AND_ASSIGN(QueryIterator);
};

// Looks for |search_key| in the query portion of |url|. Returns true if the
// key is found and sets |out_value| to the unescaped value for the key.
// Returns false if the key is not found.
NET_EXPORT bool GetValueForKeyInQuery(const GURL& url,
                                      const std::string& search_key,
                                      std::string* out_value);

// Splits an input of the form <host>[":"<port>] into its consitituent parts.
// Saves the result into |*host| and |*port|. If the input did not have
// the optional port, sets |*port| to -1.
// Returns true if the parsing was successful, false otherwise.
// The returned host is NOT canonicalized, and may be invalid.
//
// IPv6 literals must be specified in a bracketed form, for instance:
//   [::1]:90 and [::1]
//
// The resultant |*host| in both cases will be "::1" (not bracketed).
NET_EXPORT bool ParseHostAndPort(base::StringPiece input,
                                 std::string* host,
                                 int* port);

// Returns a host:port string for the given URL.
NET_EXPORT std::string GetHostAndPort(const GURL& url);

// Returns a host[:port] string for the given URL, where the port is omitted
// if it is the default for the URL's scheme.
NET_EXPORT std::string GetHostAndOptionalPort(const GURL& url);

// Returns the hostname by trimming the ending dot, if one exists.
NET_EXPORT std::string TrimEndingDot(base::StringPiece host);

// Returns either the host from |url|, or, if the host is empty, the full spec.
NET_EXPORT std::string GetHostOrSpecFromURL(const GURL& url);

// Canonicalizes |host| and returns it.  Also fills |host_info| with
// IP address information.  |host_info| must not be NULL.
NET_EXPORT std::string CanonicalizeHost(base::StringPiece host,
                                        url::CanonHostInfo* host_info);

// Returns true if |host| is not an IP address and is compliant with a set of
// rules based on RFC 1738 and tweaked to be compatible with the real world.
// The rules are:
//   * One or more components separated by '.'
//   * Each component contains only alphanumeric characters and '-' or '_'
//   * The last component begins with an alphanumeric character
//   * Optional trailing dot after last component (means "treat as FQDN")
//
// NOTE: You should only pass in hosts that have been returned from
// CanonicalizeHost(), or you may not get accurate results.
NET_EXPORT bool IsCanonicalizedHostCompliant(const std::string& host);

// Returns true if |hostname| contains a non-registerable or non-assignable
// domain name (eg: a gTLD that has not been assigned by IANA) or an IP address
// that falls in an range reserved for non-publicly routable networks.
NET_EXPORT bool IsHostnameNonUnique(const std::string& hostname);

// Returns true if the host part of |url| is a local host name according to
// HostStringIsLocalhost.
NET_EXPORT bool IsLocalhost(const GURL& url);

// Returns true if |host| is one of the local hostnames
// (e.g. "localhost") or IP addresses (IPv4 127.0.0.0/8 or IPv6 ::1).
// "[::1]" is not detected as a local hostname. Do not use this method to check
// whether the host part of a URL is a local host name; use IsLocalhost instead.
//
// Note that this function does not check for IP addresses other than
// the above, although other IP addresses may point to the local
// machine.
NET_EXPORT bool HostStringIsLocalhost(base::StringPiece host);

// Strip the portions of |url| that aren't core to the network request.
//   - user name / password
//   - reference section
NET_EXPORT GURL SimplifyUrlForRequest(const GURL& url);

// Changes scheme "ws" to "http" and "wss" to "https". This is useful for origin
// checks and authentication, where WebSocket URLs are treated as if they were
// HTTP. It is an error to call this function with a url with a scheme other
// than "ws" or "wss".
NET_EXPORT GURL ChangeWebSocketSchemeToHttpScheme(const GURL& url);

// Extracts the unescaped username/password from |url|, saving the results
// into |*username| and |*password|.
NET_EXPORT_PRIVATE void GetIdentityFromURL(const GURL& url,
                                           base::string16* username,
                                           base::string16* password);

// Returns true if the url's host is a Google server. This should only be used
// for histograms and shouldn't be used to affect behavior.
NET_EXPORT_PRIVATE bool HasGoogleHost(const GURL& url);

// Returns true if |host| is the hostname of a Google server. This should only
// be used for histograms and shouldn't be used to affect behavior.
NET_EXPORT_PRIVATE bool IsGoogleHost(base::StringPiece host);

// This function tests |host| to see if its one used in the initial TLS 1.3
// deployment. TLS connections to them form the basis of our comparisons.
NET_EXPORT_PRIVATE bool IsTLS13ExperimentHost(base::StringPiece host);

// This function tests |host| to see if it is of any local hostname form.
// |host| is normalized before being tested and if |is_local6| is not NULL then
// it it will be set to true if the localhost name implies an IPv6 interface (
// for instance localhost6.localdomain6).
NET_EXPORT_PRIVATE bool IsLocalHostname(base::StringPiece host,
                                        bool* is_local6);

}  // namespace net

#endif  // NET_BASE_URL_UTIL_H_
