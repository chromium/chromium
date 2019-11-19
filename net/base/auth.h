// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_AUTH_H__
#define NET_BASE_AUTH_H__

#include <string>

#include "base/strings/string16.h"
#include "net/base/net_export.h"
#include "url/origin.h"

namespace net {

// Holds info about an authentication challenge that we may want to display
// to the user.
class NET_EXPORT AuthChallengeInfo {
 public:
  AuthChallengeInfo();
  AuthChallengeInfo(const AuthChallengeInfo& other);
  ~AuthChallengeInfo();

  // Returns true if this AuthChallengeInfo is equal to |other| except for
  // |path|. Can be used to determine if the same credentials can be provided
  // for two different requests.
  bool MatchesExceptPath(const AuthChallengeInfo& other) const;

  // Whether this came from a server or a proxy.
  bool is_proxy;

  // The service issuing the challenge.
  url::Origin challenger;

  // The authentication scheme used, such as "basic" or "digest". If the
  // |source| is FTP_SERVER, this is an empty string. The encoding is ASCII.
  std::string scheme;

  // The realm of the challenge. May be empty. The encoding is UTF-8.
  std::string realm;

  // The authentication challenge.
  std::string challenge;

  // The path on which authentication was requested.
  std::string path;
};

// Authentication Credentials for an authentication credentials.
class NET_EXPORT AuthCredentials {
 public:
  AuthCredentials();
  AuthCredentials(const base::string16& username,
                  const base::string16& password);
  ~AuthCredentials();

  // Set the |username| and |password|.
  void Set(const base::string16& username, const base::string16& password);

  // Determines if |this| is equivalent to |other|.
  bool Equals(const AuthCredentials& other) const;

  // Returns true if all credentials are empty.
  bool Empty() const;

  const base::string16& username() const { return username_; }
  const base::string16& password() const { return password_; }

 private:
  // The username to provide, possibly empty. This should be ASCII only to
  // minimize compatibility problems, but arbitrary UTF-16 strings are allowed
  // and will be attempted.
  base::string16 username_;

  // The password to provide, possibly empty. This should be ASCII only to
  // minimize compatibility problems, but arbitrary UTF-16 strings are allowed
  // and will be attempted.
  base::string16 password_;

  // Intentionally allowing the implicit copy constructor and assignment
  // operators.
};

// Authentication structures
enum AuthState {
  AUTH_STATE_DONT_NEED_AUTH,
  AUTH_STATE_NEED_AUTH,
  AUTH_STATE_HAVE_AUTH,
  AUTH_STATE_CANCELED
};

}  // namespace net

#endif  // NET_BASE_AUTH_H__
