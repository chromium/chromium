// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/app_view/app_view_guest_internal_api.h"

#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/guest_view/app_view/app_view_guest.h"
#include "extensions/common/api/app_view_guest_internal.h"

namespace extensions {

namespace appview = api::app_view_guest_internal;

AppViewGuestInternalAttachFrameFunction::
    AppViewGuestInternalAttachFrameFunction() {
}

ExtensionFunction::ResponseAction
AppViewGuestInternalAttachFrameFunction::Run() {
  std::unique_ptr<appview::AttachFrame::Params> params(
      appview::AttachFrame::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = extension()->GetResourceURL(params->url);
  EXTENSION_FUNCTION_VALIDATE(url.is_valid());

  ResponseValue response;
  if (AppViewGuest::CompletePendingRequest(
          browser_context(), url, params->guest_instance_id, extension_id(),
          render_frame_host()->GetProcess())) {
    response = NoArguments();
  } else {
    response = Error("could not complete");
  }
  return RespondNow(std::move(response));
}

AppViewGuestInternalDenyRequestFunction::
    AppViewGuestInternalDenyRequestFunction() {
}

ExtensionFunction::ResponseAction
AppViewGuestInternalDenyRequestFunction::Run() {
  std::unique_ptr<appview::DenyRequest::Params> params(
      appview::DenyRequest::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Since the URL passed into AppViewGuest:::CompletePendingRequest is invalid,
  // a new <appview> WebContents will not be created.
  ResponseValue response;
  if (AppViewGuest::CompletePendingRequest(
          browser_context(), GURL(), params->guest_instance_id, extension_id(),
          render_frame_host()->GetProcess())) {
    response = NoArguments();
  } else {
    response = Error("could not complete");
  }
  return RespondNow(std::move(response));
}

}  // namespace extensions
