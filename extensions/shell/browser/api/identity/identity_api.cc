// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/identity/identity_api.h"

#include "extensions/shell/common/api/identity.h"

namespace extensions {
namespace shell {

IdentityRemoveCachedAuthTokenFunction::IdentityRemoveCachedAuthTokenFunction() =
    default;

IdentityRemoveCachedAuthTokenFunction::
    ~IdentityRemoveCachedAuthTokenFunction() = default;

ExtensionFunction::ResponseAction IdentityRemoveCachedAuthTokenFunction::Run() {
  std::unique_ptr<api::identity::RemoveCachedAuthToken::Params> params(
      api::identity::RemoveCachedAuthToken::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // This stub identity API does not maintain a token cache, so there is nothing
  // to remove.
  return RespondNow(NoArguments());
}

}  // namespace shell
}  // namespace extensions
