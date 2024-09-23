// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_frame_context_data.h"

#include "extensions/renderer/renderer_context_data.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

std::unique_ptr<FrameContextData>
RendererFrameContextData::CloneFrameContextData() const {
  // Note: Extension tests mock objects like ScriptContext and don't fill in
  // their frame. This results in calls to this constructor with a nullptr
  // frame, so we can't CHECK(frame_) here.
  return std::make_unique<RendererFrameContextData>(frame_);
}

bool RendererFrameContextData::HasControlledFrameCapability() const {
  CHECK(frame_);
  return frame_->IsFeatureEnabled(
             blink::mojom::PermissionsPolicyFeature::kControlledFrame) &&
         RendererContextData::IsIsolatedWebAppContextAndEnabled();
}

std::unique_ptr<FrameContextData>
RendererFrameContextData::GetLocalParentOrOpener() const {
  CHECK(frame_);
  blink::WebFrame* parent_or_opener = nullptr;
  if (frame_->Parent()) {
    parent_or_opener = frame_->Parent();
  } else {
    parent_or_opener = frame_->Opener();
  }
  if (!parent_or_opener || !parent_or_opener->IsWebLocalFrame()) {
    return nullptr;
  }

  blink::WebLocalFrame* local_parent_or_opener =
      parent_or_opener->ToWebLocalFrame();
  if (local_parent_or_opener->GetDocument().IsNull()) {
    return nullptr;
  }

  return std::make_unique<RendererFrameContextData>(local_parent_or_opener);
}

GURL RendererFrameContextData::GetUrl() const {
  CHECK(frame_);
  if (frame_->GetDocument().Url().IsEmpty()) {
    // It's possible for URL to be empty when `frame_` is on the initial empty
    // document. TODO(crbug.com/40176869): Consider making  `frame_`'s
    // document's URL about:blank instead of empty in that case.
    return GURL(url::kAboutBlankURL);
  }
  return frame_->GetDocument().Url();
}

url::Origin RendererFrameContextData::GetOrigin() const {
  CHECK(frame_);
  return frame_->GetSecurityOrigin();
}

bool RendererFrameContextData::CanAccess(const url::Origin& target) const {
  CHECK(frame_);
  return frame_->GetSecurityOrigin().CanAccess(target);
}

bool RendererFrameContextData::CanAccess(const FrameContextData& target) const {
  CHECK(frame_);
  // It is important that below `web_security_origin` wraps the security
  // origin of the `target_frame` (rather than a new origin created via
  // url::Origin round-trip - such an origin wouldn't be 100% equivalent -
  // e.g. `disallowdocumentaccess` information might be lost).  FWIW, this
  // scenario is execised by ScriptContextTest.GetEffectiveDocumentURL.
  const blink::WebLocalFrame* target_frame =
      static_cast<const RendererFrameContextData&>(target).frame_;
  CHECK(target_frame);
  blink::WebSecurityOrigin web_security_origin =
      target_frame->GetDocument().GetSecurityOrigin();

  return frame_->GetSecurityOrigin().CanAccess(web_security_origin);
}

uintptr_t RendererFrameContextData::GetId() const {
  CHECK(frame_);
  return reinterpret_cast<uintptr_t>(frame_.get());
}

}  // namespace extensions
