// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_OAUTH_TOKEN_GETTER_PROXY_H_
#define REMOTING_BASE_OAUTH_TOKEN_GETTER_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace remoting {

// Takes an instance of |OAuthTokenGetter| and runs (and deletes) it on the
// |task_runner| sequence. The proxy will silently drop requests once
// |token_getter| is deleted. This class is useful when a class needs to take a
// unique_ptr to OAuthTokenGetter but you still want to share the underlying
// token getter instance. Methods can be called from any sequence.
class OAuthTokenGetterProxy : public OAuthTokenGetter {
 public:
  OAuthTokenGetterProxy(base::WeakPtr<OAuthTokenGetter> token_getter,
                        scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Creates an OAuthTokenGetterProxy that always runs |token_getter| on the
  // sequence where this constructor is called.
  explicit OAuthTokenGetterProxy(base::WeakPtr<OAuthTokenGetter> token_getter);

  ~OAuthTokenGetterProxy() override;

  // OAuthTokenGetter overrides.
  void CallWithToken(TokenCallback on_access_token) override;
  void InvalidateCache() override;

 private:
  base::WeakPtr<OAuthTokenGetter> token_getter_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenGetterProxy);
};

}  // namespace remoting

#endif  // REMOTING_BASE_OAUTH_TOKEN_GETTER_PROXY_H_
