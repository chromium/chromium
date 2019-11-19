// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_challenge_tokenizer.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_tokenizer.h"

namespace net {

HttpAuthChallengeTokenizer::HttpAuthChallengeTokenizer(
    std::string::const_iterator begin,
    std::string::const_iterator end)
    : begin_(begin),
      end_(end),
      params_begin_(end),
      params_end_(end) {
  Init(begin, end);
}

HttpAuthChallengeTokenizer::~HttpAuthChallengeTokenizer() = default;

HttpUtil::NameValuePairsIterator HttpAuthChallengeTokenizer::param_pairs()
    const {
  return HttpUtil::NameValuePairsIterator(params_begin_, params_end_, ',');
}

std::string HttpAuthChallengeTokenizer::base64_param() const {
  // Strip off any padding.
  // (See https://bugzilla.mozilla.org/show_bug.cgi?id=230351.)
  //
  // Our base64 decoder requires that the length be a multiple of 4.
  auto encoded_length = params_end_ - params_begin_;
  while (encoded_length > 0 && encoded_length % 4 != 0 &&
         params_begin_[encoded_length - 1] == '=') {
    --encoded_length;
  }
  return std::string(params_begin_, params_begin_ + encoded_length);
}

void HttpAuthChallengeTokenizer::Init(std::string::const_iterator begin,
                                      std::string::const_iterator end) {
  // The first space-separated token is the auth-scheme.
  // NOTE: we are more permissive than RFC 2617 which says auth-scheme
  // is separated by 1*SP.
  base::StringTokenizer tok(begin, end, HTTP_LWS);
  if (!tok.GetNext()) {
    // Default param and scheme iterators provide empty strings
    return;
  }

  // Save the scheme's position.
  lower_case_scheme_ =
      base::ToLowerASCII(base::StringPiece(tok.token_begin(), tok.token_end()));

  params_begin_ = tok.token_end();
  params_end_ = end;
  HttpUtil::TrimLWS(&params_begin_, &params_end_);
}

}  // namespace net
