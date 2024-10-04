// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_CHALLENGE_TOKENIZER_H_
#define NET_HTTP_HTTP_AUTH_CHALLENGE_TOKENIZER_H_

#include <string>
#include <string_view>

#include "net/base/net_export.h"
#include "net/http/http_util.h"

namespace net {

// Breaks up a challenge string into the the auth scheme and parameter list,
// according to RFC 2617 Sec 1.2:
//     challenge = auth-scheme 1*SP 1#auth-param
//
// Depending on the challenge scheme, it may be appropriate to interpret the
// parameters as either a base-64 encoded string or a comma-delimited list
// of name-value pairs. param_pairs() and base64_param() methods are provided
// to support either usage.
class NET_EXPORT_PRIVATE HttpAuthChallengeTokenizer {
 public:
  explicit HttpAuthChallengeTokenizer(std::string_view challenge);
  ~HttpAuthChallengeTokenizer();

  // Get the original text.
  std::string_view challenge_text() const { return challenge_; }

  // Get the authenthication scheme of the challenge. The returned scheme is
  // always lowercase.
  const std::string& auth_scheme() const { return lower_case_scheme_; }

  std::string_view params() const { return params_; }
  HttpUtil::NameValuePairsIterator param_pairs() const;
  std::string_view base64_param() const;

 private:
  void Init(std::string_view challenge);

  std::string_view challenge_;
  std::string_view params_;

  std::string lower_case_scheme_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_CHALLENGE_TOKENIZER_H_
