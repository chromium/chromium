// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/guest_view_internal_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/api/guest_view_internal.h"

using guest_view::GuestViewBase;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;

namespace guest_view_internal = extensions::api::guest_view_internal;

namespace extensions {

GuestViewInternalCreateGuestFunction::GuestViewInternalCreateGuestFunction() =
    default;

GuestViewInternalCreateGuestFunction::~GuestViewInternalCreateGuestFunction() =
    default;

ExtensionFunction::ResponseAction GuestViewInternalCreateGuestFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  const std::string& view_type = args()[0].GetString();
  base::Value& create_params = mutable_args()[1];

  // Since we are creating a new guest, we will create a GuestViewManager
  // if we don't already have one.
  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  if (!guest_view_manager) {
    guest_view_manager = GuestViewManager::CreateWithDelegate(
        browser_context(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
            browser_context()));
  }

  content::WebContents* sender_web_contents = GetSenderWebContents();
  if (!sender_web_contents)
    return RespondNow(Error("Guest views can only be embedded in web content"));

  auto callback = base::BindOnce(
      &GuestViewInternalCreateGuestFunction::CreateGuestCallback, this);

  // Add flag to |create_params| to indicate that the element size is specified
  // in logical units.
  create_params.SetBoolKey(guest_view::kElementSizeIsLogical, true);

  guest_view_manager->CreateGuest(view_type, sender_web_contents,
                                  create_params.GetDict(), std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void GuestViewInternalCreateGuestFunction::CreateGuestCallback(
    guest_view::GuestViewBase* guest) {
  int guest_instance_id = guest ? guest->guest_instance_id() : 0;

  base::Value::Dict return_params;
  return_params.Set(guest_view::kID, guest_instance_id);

  Respond(WithArguments(std::move(return_params)));
}

GuestViewInternalSetSizeFunction::GuestViewInternalSetSizeFunction() = default;

GuestViewInternalSetSizeFunction::~GuestViewInternalSetSizeFunction() = default;

ExtensionFunction::ResponseAction GuestViewInternalSetSizeFunction::Run() {
  std::unique_ptr<guest_view_internal::SetSize::Params> params(
      guest_view_internal::SetSize::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  GuestViewBase* guest =
      GuestViewBase::FromInstanceID(source_process_id(), params->instance_id);
  if (!guest)
    return RespondNow(Error(kUnknownErrorDoNotUse));

  guest_view::SetSizeParams set_size_params;
  if (params->params.enable_auto_size) {
    set_size_params.enable_auto_size = params->params.enable_auto_size;
  }
  if (params->params.min) {
    set_size_params.min_size.emplace(params->params.min->width,
                                     params->params.min->height);
  }
  if (params->params.max) {
    set_size_params.max_size.emplace(params->params.max->width,
                                     params->params.max->height);
  }
  if (params->params.normal) {
    set_size_params.normal_size.emplace(params->params.normal->width,
                                        params->params.normal->height);
  }

  guest->SetSize(set_size_params);
  return RespondNow(NoArguments());
}

}  // namespace extensions
