// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/functions.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "mojo/public/c/system/invitation.h"

namespace mojo {

namespace {

DefaultProcessErrorHandler& GetDefaultProcessErrorHandler() {
  static base::NoDestructor<DefaultProcessErrorHandler> handler;
  return *handler;
}

void HandleError(const MojoProcessErrorDetails* details) {
  const DefaultProcessErrorHandler& handler = GetDefaultProcessErrorHandler();
  handler.Run(
      std::string(details->error_message, details->error_message_length));
}

}  // namespace

void SetDefaultProcessErrorHandler(DefaultProcessErrorHandler handler) {
  // Ensure any previously set handler is wiped out.
  MojoSetDefaultProcessErrorHandler(nullptr, nullptr);
  auto& global_handler = GetDefaultProcessErrorHandler();
  global_handler = std::move(handler);
  if (global_handler)
    MojoSetDefaultProcessErrorHandler(&HandleError, nullptr);
}

}  // namespace mojo
