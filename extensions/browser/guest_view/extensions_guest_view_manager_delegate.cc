// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

#include <memory>
#include <utility>

#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/guest_view/app_view/app_view_guest.h"
#include "extensions/browser/guest_view/extension_options/extension_options_guest.h"
#include "extensions/browser/guest_view/guest_view_events.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/utils/extension_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"

using guest_view::GuestViewBase;
using guest_view::GuestViewManager;

namespace extensions {

// Returns a HostID instance based on the given GuestViewBase.
mojom::HostID GenerateHostIdFromGuestView(
    const guest_view::GuestViewBase& guest) {
  // Note: We return a type of kExtensions for all cases where
  // |guest.IsOwnedByExtension()| are true, as well as some additional cases
  // where that call is false but also |guest.IsOwnedByWebUI()| and
  // |guest.IsOwnedByControlledFrameEmbedder()| are false. Those appear to be
  // when the provided extension identifier is blank. Future work in this area
  // could improve the checks here so all the cases are declared relative to
  // what the GuestView instance asserts itself to be.
  mojom::HostID::HostType host_type = mojom::HostID::HostType::kExtensions;

  if (guest.IsOwnedByWebUI()) {
    host_type = mojom::HostID::HostType::kWebUi;
  } else if (guest.IsOwnedByControlledFrameEmbedder()) {
    host_type = mojom::HostID::HostType::kControlledFrameEmbedder;
  }

  return mojom::HostID(host_type, guest.owner_host());
}

// static
bool ExtensionsGuestViewManagerDelegate::IsGuestAvailableToContextWithFeature(
    const GuestViewBase* guest,
    const std::string& feature_name) {
  const Feature* feature = FeatureProvider::GetAPIFeature(feature_name);
  if (!feature) {
    return false;
  }

  content::BrowserContext* context = guest->browser_context();
  ProcessMap* process_map = ProcessMap::Get(context);
  CHECK(process_map);

  const Extension* owner_extension =
      ProcessManager::Get(context)->GetExtensionForRenderFrameHost(
          guest->owner_rfh());

  const GURL& owner_site_url = guest->GetOwnerSiteURL();
  // Ok for |owner_extension| to be nullptr, the embedder might be WebUI.
  Feature::Availability availability = feature->IsAvailableToContext(
      owner_extension,
      process_map->GetMostLikelyContextType(
          owner_extension, guest->owner_rfh()->GetProcess()->GetID(),
          &owner_site_url),
      owner_site_url, util::GetBrowserContextId(context),
      BrowserFrameContextData(guest->owner_rfh()));

  return availability.is_available();
}

ExtensionsGuestViewManagerDelegate::ExtensionsGuestViewManagerDelegate() =
    default;

ExtensionsGuestViewManagerDelegate::~ExtensionsGuestViewManagerDelegate() =
    default;

void ExtensionsGuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {
  // Set the view type so extensions sees the guest view as a foreground page.
  SetViewType(guest_web_contents, mojom::ViewType::kExtensionGuest);
}

void ExtensionsGuestViewManagerDelegate::DispatchEvent(
    const std::string& event_name,
    base::Value::Dict args,
    GuestViewBase* guest,
    int instance_id) {
  CHECK(guest);
  mojom::EventFilteringInfoPtr info = mojom::EventFilteringInfo::New();
  info->has_instance_id = true;
  info->instance_id = instance_id;
  base::Value::List event_args;
  event_args.Append(std::move(args));

  // GetEventHistogramValue maps guest view event names to their histogram
  // value. It needs to be like this because the guest view component doesn't
  // know about extensions, so GuestViewEvent can't have an
  // extensions::events::HistogramValue as an argument.
  events::HistogramValue histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  DCHECK_NE(events::UNKNOWN, histogram_value) << "Event " << event_name
                                              << " must have a histogram value";

  content::RenderFrameHost* owner = guest->owner_rfh();
  if (!owner || !ExtensionsBrowserClient::Get()->IsValidContext(
                    guest->browser_context())) {
    return;  // Could happen at tab shutdown.
  }

  EventRouter::Get(guest->browser_context())
      ->DispatchEventToSender(owner->GetProcess(), guest->browser_context(),
                              GenerateHostIdFromGuestView(*guest),
                              histogram_value, event_name,
                              extensions::kMainThreadId,
                              blink::mojom::kInvalidServiceWorkerVersionId,
                              std::move(event_args), std::move(info));
}

bool ExtensionsGuestViewManagerDelegate::IsGuestAvailableToContext(
    const GuestViewBase* guest) const {
  return IsGuestAvailableToContextWithFeature(guest, guest->GetAPINamespace());
}

bool ExtensionsGuestViewManagerDelegate::IsOwnedByExtension(
    const GuestViewBase* guest) {
  content::BrowserContext* context = guest->browser_context();
  return !!ProcessManager::Get(context)->GetExtensionForRenderFrameHost(
      guest->owner_rfh());
}

bool ExtensionsGuestViewManagerDelegate::IsOwnedByControlledFrameEmbedder(
    const GuestViewBase* guest) {
  return false;
}

void ExtensionsGuestViewManagerDelegate::RegisterAdditionalGuestViewTypes(
    GuestViewManager* manager) {
  manager->RegisterGuestViewType(AppViewGuest::Type,
                                 base::BindRepeating(&AppViewGuest::Create),
                                 base::NullCallback());
  manager->RegisterGuestViewType(
      ExtensionOptionsGuest::Type,
      base::BindRepeating(&ExtensionOptionsGuest::Create),
      base::NullCallback());
  manager->RegisterGuestViewType(
      MimeHandlerViewGuest::Type,
      base::BindRepeating(&MimeHandlerViewGuest::Create), base::NullCallback());
  manager->RegisterGuestViewType(WebViewGuest::Type,
                                 base::BindRepeating(&WebViewGuest::Create),
                                 base::BindRepeating(&WebViewGuest::CleanUp));
}

}  // namespace extensions
