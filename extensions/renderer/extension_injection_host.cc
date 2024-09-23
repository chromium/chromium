// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_injection_host.h"

#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/renderer/extension_web_view_helper.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/common/pdf_util.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_PDF)

namespace extensions {

ExtensionInjectionHost::ExtensionInjectionHost(const Extension* extension)
    : InjectionHost(
          mojom::HostID(mojom::HostID::HostType::kExtensions, extension->id())),
      extension_(extension) {}

ExtensionInjectionHost::~ExtensionInjectionHost() {
}

// static
std::unique_ptr<const InjectionHost> ExtensionInjectionHost::Create(
    const ExtensionId& extension_id) {
  const Extension* extension =
      RendererExtensionRegistry::Get()->GetByID(extension_id);
  if (!extension)
    return nullptr;
  return std::unique_ptr<const ExtensionInjectionHost>(
      new ExtensionInjectionHost(extension));
}

const std::string* ExtensionInjectionHost::GetContentSecurityPolicy() const {
  return CSPInfo::GetIsolatedWorldCSP(*extension_);
}

const GURL& ExtensionInjectionHost::url() const {
  return extension_->url();
}

const std::string& ExtensionInjectionHost::name() const {
  return extension_->name();
}

PermissionsData::PageAccess ExtensionInjectionHost::CanExecuteOnFrame(
    const GURL& document_url,
    content::RenderFrame* render_frame,
    int tab_id,
    bool is_declarative) const {
  blink::WebLocalFrame* web_local_frame = render_frame->GetWebFrame();

#if BUILDFLAG(ENABLE_PDF)
  // Block executing scripts in the PDF content frame. The parent frame should
  // be the PDF extension frame.
  blink::WebFrame* parent_web_frame = web_local_frame->Parent();
  if (parent_web_frame && IsPdfExtensionOrigin(url::Origin(
                              parent_web_frame->GetSecurityOrigin()))) {
    return PermissionsData::PageAccess::kDenied;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  // If the WebView is embedded in another WebView the outermost extension
  // origin will be set, otherwise we should use it directly from the
  // WebFrame's top origin.
  auto outermost_origin =
      ExtensionWebViewHelper::Get(render_frame->GetWebView())
          ->GetOutermostOrigin();
  if (!outermost_origin) {
    outermost_origin = web_local_frame->Top()->GetSecurityOrigin();
  }

  // Only allowlisted extensions may run scripts on another extension's page.
  if (outermost_origin->scheme() == kExtensionScheme &&
      outermost_origin->host() != extension_->id() &&
      !PermissionsData::CanExecuteScriptEverywhere(extension_->id(),
                                                   extension_->location())) {
    return PermissionsData::PageAccess::kDenied;
  }

  // Declarative user scripts use "page access" (from "permissions" section in
  // manifest) whereas non-declarative user scripts use custom
  // "content script access" logic.
  PermissionsData::PageAccess access = PermissionsData::PageAccess::kAllowed;
  if (is_declarative) {
    access = extension_->permissions_data()->GetPageAccess(
        document_url,
        tab_id,
        nullptr /* ignore error */);
  } else {
    access = extension_->permissions_data()->GetContentScriptAccess(
        document_url,
        tab_id,
        nullptr /* ignore error */);
  }
  if (access == PermissionsData::PageAccess::kWithheld &&
      (tab_id == -1 || !render_frame->GetWebFrame()->IsOutermostMainFrame())) {
    // Note: we don't consider ACCESS_WITHHELD for child frames or for frames
    // outside of tabs because there is nowhere to surface a request.
    // TODO(devlin): We should ask for permission somehow. crbug.com/491402.
    access = PermissionsData::PageAccess::kDenied;
  }
  return access;
}

}  // namespace extensions
