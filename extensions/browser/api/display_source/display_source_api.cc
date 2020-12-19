// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/display_source_api.h"

#include <utility>

#include "base/bind.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate_factory.h"
#include "extensions/common/api/display_source.h"

namespace extensions {

namespace {

const char kErrorNotSupported[] = "Not supported";
const char kErrorInvalidArguments[] = "Invalid arguments";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplaySourceGetAvailableSinksFunction

DisplaySourceGetAvailableSinksFunction::
    ~DisplaySourceGetAvailableSinksFunction() {}

ExtensionFunction::ResponseAction
DisplaySourceGetAvailableSinksFunction::Run() {
  DisplaySourceConnectionDelegate* delegate =
      DisplaySourceConnectionDelegateFactory::GetForBrowserContext(
          browser_context());
  if (!delegate) {
    return RespondNow(Error(kErrorNotSupported));
  }

  auto success_callback = base::BindOnce(
      &DisplaySourceGetAvailableSinksFunction::OnGetSinksCompleted, this);
  auto failure_callback = base::BindOnce(
      &DisplaySourceGetAvailableSinksFunction::OnGetSinksFailed, this);
  delegate->GetAvailableSinks(std::move(success_callback),
                              std::move(failure_callback));

  return RespondLater();
}

void DisplaySourceGetAvailableSinksFunction::OnGetSinksCompleted(
    const DisplaySourceSinkInfoList& sinks) {
  std::unique_ptr<base::ListValue> result =
      api::display_source::GetAvailableSinks::Results::Create(sinks);
  Respond(ArgumentList(std::move(result)));
}

void DisplaySourceGetAvailableSinksFunction::OnGetSinksFailed(
    const std::string& reason) {
  Respond(Error(reason));
}

////////////////////////////////////////////////////////////////////////////////
// DisplaySourceRequestAuthenticationFunction

DisplaySourceRequestAuthenticationFunction::
    ~DisplaySourceRequestAuthenticationFunction() {}

ExtensionFunction::ResponseAction
DisplaySourceRequestAuthenticationFunction::Run() {
  std::unique_ptr<api::display_source::RequestAuthentication::Params> params(
      api::display_source::RequestAuthentication::Params::Create(*args_));
  if (!params) {
    return RespondNow(Error(kErrorInvalidArguments));
  }

  DisplaySourceConnectionDelegate* delegate =
      DisplaySourceConnectionDelegateFactory::GetForBrowserContext(
          browser_context());
  if (!delegate) {
    return RespondNow(Error(kErrorNotSupported));
  }

  auto success_callback = base::BindOnce(
      &DisplaySourceRequestAuthenticationFunction::OnRequestAuthCompleted,
      this);
  auto failure_callback = base::BindOnce(
      &DisplaySourceRequestAuthenticationFunction::OnRequestAuthFailed, this);
  delegate->RequestAuthentication(params->sink_id, std::move(success_callback),
                                  std::move(failure_callback));
  return RespondLater();
}

void DisplaySourceRequestAuthenticationFunction::OnRequestAuthCompleted(
    const DisplaySourceAuthInfo& auth_info) {
  std::unique_ptr<base::ListValue> result =
      api::display_source::RequestAuthentication::Results::Create(auth_info);
  Respond(ArgumentList(std::move(result)));
}

void DisplaySourceRequestAuthenticationFunction::OnRequestAuthFailed(
    const std::string& reason) {
  Respond(Error(reason));
}

}  // namespace extensions
