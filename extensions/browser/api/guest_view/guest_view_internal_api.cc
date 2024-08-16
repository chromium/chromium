// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/guest_view_internal_api.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/token.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/render_process_host.h"
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
  std::optional<guest_view_internal::CreateGuest::Params> params =
      guest_view_internal::CreateGuest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Since we are creating a new guest, we will create a GuestViewManager
  // if we don't already have one.
  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  if (!guest_view_manager) {
    guest_view_manager = GuestViewManager::CreateWithDelegate(
        browser_context(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  if (!render_frame_host()) {
    return RespondNow(Error("Guest views can only be embedded in web content"));
  }

  // The renderer identifies the window which owns the guest view element. We
  // consider this the most likely owner for the guest view. It is possible,
  // however, for the guest to be embedded in another same-process frame upon
  // attachment.
  const int sender_process_id = render_frame_host()->GetProcess()->GetID();

  content::RenderFrameHost* owner_rfh = nullptr;
  auto token = base::Token::FromString(params->owner_frame_token);
  if (token) {
    auto unguessable_token =
        base::UnguessableToken::Deserialize(token->high(), token->low());
    if (unguessable_token) {
      owner_rfh = content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              sender_process_id,
              blink::LocalFrameToken(unguessable_token.value())));
    }
  }

  if (!owner_rfh) {
    // If the renderer can't determine the owner at creation, fall back to
    // assuming the main frame.
    owner_rfh = render_frame_host()->GetMainFrame();
  }

  auto callback = base::BindOnce(
      &GuestViewInternalCreateGuestFunction::CreateGuestCallback, this);

  // Add flag to |create_params| to indicate that the element size is specified
  // in logical units.
  base::Value::Dict& create_params =
      params->create_params.additional_properties;
  create_params.Set(guest_view::kElementSizeIsLogical, true);

  guest_view_manager->CreateGuest(params->view_type, owner_rfh, create_params,
                                  std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void GuestViewInternalCreateGuestFunction::CreateGuestCallback(
    guest_view::GuestViewBase* guest) {
  const int guest_instance_id =
      guest ? guest->guest_instance_id() : guest_view::kInstanceIDNone;

  Respond(WithArguments(guest_instance_id));
}

GuestViewInternalDestroyUnattachedGuestFunction::
    GuestViewInternalDestroyUnattachedGuestFunction() = default;
GuestViewInternalDestroyUnattachedGuestFunction::
    ~GuestViewInternalDestroyUnattachedGuestFunction() = default;

ExtensionFunction::ResponseAction
GuestViewInternalDestroyUnattachedGuestFunction::Run() {
  std::optional<guest_view_internal::DestroyUnattachedGuest::Params> params =
      guest_view_internal::DestroyUnattachedGuest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GuestViewBase* guest =
      GuestViewBase::FromInstanceID(source_process_id(), params->instance_id);
  if (guest) {
    std::unique_ptr<GuestViewBase> owned_guest =
        guest->GetGuestViewManager()->TransferOwnership(guest);
    owned_guest.reset();
  }

  return RespondNow(NoArguments());
}

GuestViewInternalSetSizeFunction::GuestViewInternalSetSizeFunction() = default;

GuestViewInternalSetSizeFunction::~GuestViewInternalSetSizeFunction() = default;

ExtensionFunction::ResponseAction GuestViewInternalSetSizeFunction::Run() {
  std::optional<guest_view_internal::SetSize::Params> params =
      guest_view_internal::SetSize::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GuestViewBase* guest =
      GuestViewBase::FromInstanceID(source_process_id(), params->instance_id);
  if (!guest) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

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
