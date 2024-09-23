// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browser_frame_context_data.h"

#include <memory>

#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"

namespace extensions {

std::unique_ptr<FrameContextData>
BrowserFrameContextData::CloneFrameContextData() const {
  return std::make_unique<BrowserFrameContextData>(frame_);
}

bool BrowserFrameContextData::HasControlledFrameCapability() const {
  return frame_ &&
         frame_->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kControlledFrame) &&
         content::HasIsolatedContextCapability(frame_);
}

std::unique_ptr<FrameContextData>
BrowserFrameContextData::GetLocalParentOrOpener() const {
  CHECK(frame_);
  content::RenderFrameHost* parent_or_opener = frame_->GetParent();
  // Non primary pages (e.g. fenced frame, prerendered page, bfcache) can't look
  // at the opener, and WebContents::GetOpener returns the opener on the primary
  // frame tree. Thus, GetOpener should be called when `frame_` is a primary
  // main frame.
  if (!parent_or_opener && frame_->IsInPrimaryMainFrame()) {
    parent_or_opener =
        content::WebContents::FromRenderFrameHost(frame_)->GetOpener();
  }
  if (!parent_or_opener) {
    return nullptr;
  }

  // Renderer-side WebLocalFrameAdapter only considers local frames.
  // Comparing processes is robust way to replicate such renderer-side checks,
  // because out caller (DoesContentScriptMatch) accepts false positives.
  // This comparison might be less accurate (e.g. give more false positives)
  // than SiteInstance comparison, but comparing processes should be robust
  // and stable as SiteInstanceGroup refactoring proceeds.
  if (parent_or_opener->GetProcess() != frame_->GetProcess()) {
    return nullptr;
  }

  return std::make_unique<BrowserFrameContextData>(parent_or_opener);
}

GURL BrowserFrameContextData::GetUrl() const {
  CHECK(frame_);
  if (frame_->GetLastCommittedURL().is_empty()) {
    // It's possible for URL to be empty when `frame_` is on the initial empty
    // document. TODO(crbug.com/40176869): Consider making  `frame_`'s
    // document's URL about:blank instead of empty in that case.
    return GURL(url::kAboutBlankURL);
  }
  return frame_->GetLastCommittedURL();
}

url::Origin BrowserFrameContextData::GetOrigin() const {
  CHECK(frame_);
  return frame_->GetLastCommittedOrigin();
}

// BrowserFrameContextData::CanAccess is unable to replicate all of the
// WebSecurityOrigin::CanAccess checks, so these methods should not be called.
bool BrowserFrameContextData::CanAccess(const url::Origin& target) const {
  NOTREACHED_IN_MIGRATION();
  return true;
}

bool BrowserFrameContextData::CanAccess(const FrameContextData& target) const {
  NOTREACHED_IN_MIGRATION();
  return true;
}

uintptr_t BrowserFrameContextData::GetId() const {
  CHECK(frame_);
  return frame_->GetRoutingID();
}

}  // namespace extensions
