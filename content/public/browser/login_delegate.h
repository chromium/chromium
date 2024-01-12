// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_LOGIN_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_LOGIN_DELEGATE_H_

#include <optional>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "net/base/auth.h"

namespace content {

// Interface for getting login credentials for HTTP auth requests. If the login
// delegate obtains credentials, it should call the LoginAuthRequiredCallback
// passed to it on creation. If it is destroyed before that point, the request
// has been canceled and the callback should not be called.
class CONTENT_EXPORT LoginDelegate {
 public:
  using LoginAuthRequiredCallback =
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>;

  virtual ~LoginDelegate() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_LOGIN_DELEGATE_H_
