// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/guest_view_internal_api.h"

#include <utility>

#include "base/bind.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/api/guest_view_internal.h"
#include "extensions/common/permissions/permissions_data.h"

using guest_view::GuestViewBase;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;

namespace guest_view_internal = extensions::api::guest_view_internal;

namespace extensions {

GuestViewInternalCreateGuestFunction::
    GuestViewInternalCreateGuestFunction() {
}

ExtensionFunction::ResponseAction GuestViewInternalCreateGuestFunction::Run() {
  std::string view_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &view_type));

  base::DictionaryValue* create_params;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &create_params));

  // Since we are creating a new guest, we will create a GuestViewManager
  // if we don't already have one.
  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  if (!guest_view_manager) {
    guest_view_manager = GuestViewManager::CreateWithDelegate(
        browser_context(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(context_));
  }

  content::WebContents* sender_web_contents = GetSenderWebContents();
  if (!sender_web_contents)
    return RespondNow(Error("Guest views can only be embedded in web content"));

  GuestViewManager::WebContentsCreatedCallback callback = base::BindOnce(
      &GuestViewInternalCreateGuestFunction::CreateGuestCallback, this);

  // Add flag to |create_params| to indicate that the element size is specified
  // in logical units.
  create_params->SetBoolean(guest_view::kElementSizeIsLogical, true);

  guest_view_manager->CreateGuest(view_type, sender_web_contents,
                                  *create_params, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void GuestViewInternalCreateGuestFunction::CreateGuestCallback(
    content::WebContents* guest_web_contents) {
  int guest_instance_id = 0;
  int content_window_id = MSG_ROUTING_NONE;
  if (guest_web_contents) {
    GuestViewBase* guest = GuestViewBase::FromWebContents(guest_web_contents);
    guest_instance_id = guest->guest_instance_id();
    content_window_id = guest->proxy_routing_id();
  }
  auto return_params = std::make_unique<base::DictionaryValue>();
  return_params->SetInteger(guest_view::kID, guest_instance_id);
  return_params->SetInteger(guest_view::kContentWindowID, content_window_id);

  Respond(OneArgument(std::move(return_params)));
}

GuestViewInternalDestroyGuestFunction::
    GuestViewInternalDestroyGuestFunction() {
}

GuestViewInternalDestroyGuestFunction::
    ~GuestViewInternalDestroyGuestFunction() {
}

ExtensionFunction::ResponseAction GuestViewInternalDestroyGuestFunction::Run() {
  std::unique_ptr<guest_view_internal::DestroyGuest::Params> params(
      guest_view_internal::DestroyGuest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  GuestViewBase* guest =
      GuestViewBase::From(source_process_id(), params->instance_id);
  if (!guest)
    return RespondNow(Error(kUnknownErrorDoNotUse));
  guest->Destroy(true);
  return RespondNow(NoArguments());
}

GuestViewInternalSetSizeFunction::GuestViewInternalSetSizeFunction() {
}

GuestViewInternalSetSizeFunction::~GuestViewInternalSetSizeFunction() {
}

ExtensionFunction::ResponseAction GuestViewInternalSetSizeFunction::Run() {
  std::unique_ptr<guest_view_internal::SetSize::Params> params(
      guest_view_internal::SetSize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  GuestViewBase* guest =
      GuestViewBase::From(source_process_id(), params->instance_id);
  if (!guest)
    return RespondNow(Error(kUnknownErrorDoNotUse));

  guest_view::SetSizeParams set_size_params;
  if (params->params.enable_auto_size) {
    set_size_params.enable_auto_size.reset(
        params->params.enable_auto_size.release());
  }
  if (params->params.min) {
    set_size_params.min_size.reset(
        new gfx::Size(params->params.min->width, params->params.min->height));
  }
  if (params->params.max) {
    set_size_params.max_size.reset(
        new gfx::Size(params->params.max->width, params->params.max->height));
  }
  if (params->params.normal) {
    set_size_params.normal_size.reset(new gfx::Size(
        params->params.normal->width, params->params.normal->height));
  }

  guest->SetSize(set_size_params);
  return RespondNow(NoArguments());
}

}  // namespace extensions
