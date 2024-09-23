// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/media/media_internals_ui.h"

#include "base/command_line.h"
#include "content/browser/media/media_internals_handler.h"
#include "content/browser/resources/media/grit/media_internals_resources.h"
#include "content/browser/resources/media/grit/media_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {
namespace {

void CreateAndAddMediaInternalsHTMLSource(BrowserContext* browser_context) {
  WebUIDataSource* source = WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIMediaInternalsHost);

  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kMediaInternalsResources, kMediaInternalsResourcesSize));
  source->SetDefaultResource(IDR_MEDIA_INTERNALS_MEDIA_INTERNALS_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// MediaInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

MediaInternalsUI::MediaInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<MediaInternalsMessageHandler>());

  CreateAndAddMediaInternalsHTMLSource(
      web_ui->GetWebContents()->GetBrowserContext());
}

}  // namespace content
