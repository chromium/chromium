// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_

namespace content {

constexpr char kAcceptHeaderSignedExchangeSuffix[] =
    ",application/signed-exchange;v=b3;q=0.7";

enum class SignedExchangeVersion { kUnknown, kB3 };

// Field names defined in the application/signed-exchange content type:
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#application-signed-exchange

constexpr char kCertSha256Key[] = "cert-sha256";
constexpr char kCertUrl[] = "cert-url";
constexpr char kDateKey[] = "date";
constexpr char kExpiresKey[] = "expires";
constexpr char kHeadersKey[] = "headers";
constexpr char kIntegrity[] = "integrity";
constexpr char kSig[] = "sig";
constexpr char kStatusKey[] = ":status";
constexpr char kValidityUrlKey[] = "validity-url";
constexpr char kCertChainCborMagic[] = "📜⛓";
constexpr char kCertKey[] = "cert";
constexpr char kOcspKey[] = "ocsp";
constexpr char kSctKey[] = "sct";
constexpr char kAllowedAltSxg[] = "allowed-alt-sxg";
constexpr char kHeaderIntegrity[] = "header-integrity";

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_
