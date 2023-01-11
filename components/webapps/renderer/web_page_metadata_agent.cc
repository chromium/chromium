// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/renderer/web_page_metadata_agent.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "components/webapps/common/constants.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/renderer/web_page_metadata_extraction.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace webapps {

WebPageMetadataAgent::WebPageMetadataAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::WebPageMetadataAgent>(base::BindRepeating(
          &WebPageMetadataAgent::OnRenderFrameObserverRequest,
          base::Unretained(this)));
}

WebPageMetadataAgent::~WebPageMetadataAgent() = default;

void WebPageMetadataAgent::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void WebPageMetadataAgent::OnDestruct() {
  delete this;
}

void WebPageMetadataAgent::GetWebPageMetadata(
    GetWebPageMetadataCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  mojom::WebPageMetadataPtr web_page_metadata = ExtractWebPageMetadata(frame);

  // The warning below is specific to mobile but it doesn't hurt to show it even
  // if the Chromium build is running on a desktop. It will get more exposition.
  if (web_page_metadata->mobile_capable ==
      mojom::WebPageMobileCapable::ENABLED_APPLE) {
    blink::WebConsoleMessage message(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> is "
        "deprecated. Please include <meta name=\"mobile-web-app-capable\" "
        "content=\"yes\">");
    frame->AddMessageToConsole(message);
  }

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (auto it = web_page_metadata->icons.begin();
       it != web_page_metadata->icons.end();) {
    if ((*it)->url.SchemeIs(url::kDataScheme))
      it = web_page_metadata->icons.erase(it);
    else
      ++it;
  }

  // Truncate the strings we send to the browser process.
  web_page_metadata->application_name =
      web_page_metadata->application_name.substr(0, kMaxMetaTagAttributeLength);
  web_page_metadata->description =
      web_page_metadata->description.substr(0, kMaxMetaTagAttributeLength);

  std::move(callback).Run(std::move(web_page_metadata));
}

void WebPageMetadataAgent::OnRenderFrameObserverRequest(
    mojo::PendingAssociatedReceiver<mojom::WebPageMetadataAgent> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace webapps
