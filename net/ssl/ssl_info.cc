// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_info.h"

#include "net/cert/x509_certificate.h"

namespace net {

SSLInfo::SSLInfo() = default;

SSLInfo::SSLInfo(const SSLInfo& info) = default;

SSLInfo::~SSLInfo() = default;

SSLInfo& SSLInfo::operator=(const SSLInfo& info) = default;

void SSLInfo::Reset() {
  *this = SSLInfo();
}

}  // namespace net
