// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "net/http/url_security_manager.h"

#include "net/http/http_auth_filter.h"

namespace net {

// static
std::unique_ptr<URLSecurityManager> URLSecurityManager::Create() {
  return std::make_unique<URLSecurityManagerAllowlist>();
}

}  //  namespace net
