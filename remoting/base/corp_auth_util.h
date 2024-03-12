// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CORP_AUTH_UTIL_H_
#define REMOTING_BASE_CORP_AUTH_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/base/oauth_token_getter_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

// Creates an OAuthTokenGetterImpl that is suitable for making requests to corp
// APIs. Corp APIs' authentication config is different from that of the regular
// APIs, so you can't make authenticated corp API requests with the regular
// OAuthTokenGetter.
std::unique_ptr<OAuthTokenGetterImpl> CreateCorpTokenGetter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& service_account_email,
    const std::string& refresh_token);

}  // namespace remoting

#endif  // REMOTING_BASE_CORP_AUTH_UTIL_H_
