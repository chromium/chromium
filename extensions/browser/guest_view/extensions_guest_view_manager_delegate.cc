// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

#include <memory>
#include <utility>

#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/event_router.h"
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
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-forward.h"

using guest_view::GuestViewBase;
using guest_view::GuestViewManager;

namespace extensions {

ExtensionsGuestViewManagerDelegate::ExtensionsGuestViewManagerDelegate(
    content::BrowserContext* context)
    : context_(context) {
}

ExtensionsGuestViewManagerDelegate::~ExtensionsGuestViewManagerDelegate() {
}

void ExtensionsGuestViewManagerDelegate::OnGuestAdded(
    content::WebContents* guest_web_contents) const {
  // Set the view type so extensions sees the guest view as a foreground page.
  SetViewType(guest_web_contents, VIEW_TYPE_EXTENSION_GUEST);
}

void ExtensionsGuestViewManagerDelegate::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::DictionaryValue> args,
    GuestViewBase* guest,
    int instance_id) {
  EventFilteringInfo info;
  info.instance_id = instance_id;
  std::unique_ptr<base::ListValue> event_args(new base::ListValue());
  event_args->Append(std::move(args));

  // GetEventHistogramValue maps guest view event names to their histogram
  // value. It needs to be like this because the guest view component doesn't
  // know about extensions, so GuestViewEvent can't have an
  // extensions::events::HistogramValue as an argument.
  events::HistogramValue histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  DCHECK_NE(events::UNKNOWN, histogram_value) << "Event " << event_name
                                              << " must have a histogram value";

  content::WebContents* owner = guest->owner_web_contents();
  if (!owner)
    return;  // Could happen at tab shutdown.

  EventRouter::DispatchEventToSender(
      owner->GetRenderViewHost(), guest->browser_context(), guest->owner_host(),
      histogram_value, event_name, content::ChildProcessHost::kInvalidUniqueID,
      extensions::kMainThreadId, blink::mojom::kInvalidServiceWorkerVersionId,
      std::move(event_args), info);
}

bool ExtensionsGuestViewManagerDelegate::IsGuestAvailableToContext(
    GuestViewBase* guest) {
  const Feature* feature =
      FeatureProvider::GetAPIFeature(guest->GetAPINamespace());
  if (!feature)
    return false;

  ProcessMap* process_map = ProcessMap::Get(context_);
  CHECK(process_map);

  const Extension* owner_extension = ProcessManager::Get(context_)->
      GetExtensionForWebContents(guest->owner_web_contents());

  // Ok for |owner_extension| to be nullptr, the embedder might be WebUI.
  Feature::Availability availability = feature->IsAvailableToContext(
      owner_extension,
      process_map->GetMostLikelyContextType(
          owner_extension,
          guest->owner_web_contents()->GetMainFrame()->GetProcess()->GetID()),
      guest->GetOwnerSiteURL());

  return availability.is_available();
}

bool ExtensionsGuestViewManagerDelegate::IsOwnedByExtension(
    GuestViewBase* guest) {
  return !!ProcessManager::Get(context_)->
      GetExtensionForWebContents(guest->owner_web_contents());
}

void ExtensionsGuestViewManagerDelegate::RegisterAdditionalGuestViewTypes() {
  GuestViewManager* manager = GuestViewManager::FromBrowserContext(context_);
  manager->RegisterGuestViewType<AppViewGuest>();
  manager->RegisterGuestViewType<ExtensionOptionsGuest>();
  manager->RegisterGuestViewType<MimeHandlerViewGuest>();
  manager->RegisterGuestViewType<WebViewGuest>();
}

}  // namespace extensions
