// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_challenge_tokenizer.h"

#include <string_view>

#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace net {

HttpAuthChallengeTokenizer::HttpAuthChallengeTokenizer(
    std::string_view challenge)
    : challenge_(challenge) {
  Init(challenge);
}

HttpAuthChallengeTokenizer::~HttpAuthChallengeTokenizer() = default;

HttpUtil::NameValuePairsIterator HttpAuthChallengeTokenizer::param_pairs()
    const {
  return HttpUtil::NameValuePairsIterator(params_, /*delimiter=*/',');
}

std::string_view HttpAuthChallengeTokenizer::base64_param() const {
  // Strip off any padding.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
  //
  // Our base64 decoder requires that the length be a multiple of 4.
  auto encoded_length = params_.length();
  while (encoded_length > 0 && encoded_length % 4 != 0 &&
         params_[encoded_length - 1] == '=') {
    --encoded_length;
  }
  return params_.substr(0, encoded_length);
}

void HttpAuthChallengeTokenizer::Init(std::string_view challenge) {
  // The first space-separated token is the auth-scheme.
  // NOTE: we are more permissive than RFC 2617 which says auth-scheme
  // is separated by 1*SP.
  base::StringViewTokenizer tok(challenge, HTTP_LWS);
  if (!tok.GetNext()) {
    // Default param and scheme iterators provide empty strings
    return;
  }

  // Save the scheme's position.
  lower_case_scheme_ = base::ToLowerASCII(
      base::MakeStringPiece(tok.token_begin(), tok.token_end()));

  params_ =
      HttpUtil::TrimLWS(std::string_view(tok.token_end(), challenge.end()));
}

}  // namespace net
