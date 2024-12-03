// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/view_type_utils.h"

#include "base/lazy_instance.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_client.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "components/guest_view/browser/guest_view_base.h"
#endif

using content::WebContents;

namespace extensions {

namespace {

const char kViewTypeUserDataKey[] = "ViewTypeUserData";

class ViewTypeUserData : public base::SupportsUserData::Data {
 public:
  explicit ViewTypeUserData(mojom::ViewType type) : type_(type) {}
  ~ViewTypeUserData() override {}
  mojom::ViewType type() { return type_; }

 private:
  mojom::ViewType type_;
};

}  // namespace

mojom::ViewType GetViewType(WebContents* tab) {
  if (!tab) {
    return mojom::ViewType::kInvalid;
  }

  ViewTypeUserData* user_data = static_cast<ViewTypeUserData*>(
      tab->GetUserData(&kViewTypeUserDataKey));

  return user_data ? user_data->type() : mojom::ViewType::kInvalid;
}

mojom::ViewType GetViewType(content::RenderFrameHost* frame_host) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (guest_view::GuestViewBase::IsGuest(frame_host)) {
    return mojom::ViewType::kExtensionGuest;
  }
#endif
  return GetViewType(content::WebContents::FromRenderFrameHost(frame_host));
}

void SetViewType(WebContents* tab, mojom::ViewType type) {
  tab->SetUserData(&kViewTypeUserDataKey,
                   std::make_unique<ViewTypeUserData>(type));

  ExtensionsBrowserClient::Get()->AttachExtensionTaskManagerTag(tab, type);

  if (auto* ewco = ExtensionWebContentsObserver::GetForWebContents(tab)) {
    tab->ForEachRenderFrameHost([ewco,
                                 type](content::RenderFrameHost* frame_host) {
      if (mojom::LocalFrame* local_frame = ewco->GetLocalFrame(frame_host)) {
        local_frame->NotifyRenderViewType(type);
      }
    });
  }
}

}  // namespace extensions
