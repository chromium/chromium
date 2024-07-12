// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_local_frame_client.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"

namespace blink {

AssociatedInterfaceProvider*
WebLocalFrameClient::GetRemoteNavigationAssociatedInterfaces() {
  // Embedders will typically override this, but provide a base implementation
  // so it never returns null. That way we don't need to add a bunch of null
  // checks for consumers of this API.
  // TODO(dtapuska): We should make this interface a pure virtual so we don't
  // have this implementation in the base class.
  return AssociatedInterfaceProvider::GetEmptyAssociatedInterfaceProvider();
}

std::unique_ptr<URLLoader> WebLocalFrameClient::CreateURLLoaderForTesting() {
  return nullptr;
}

std::unique_ptr<WebLinkPreviewTriggerer>
WebLocalFrameClient::CreateLinkPreviewTriggerer() {
  return nullptr;
}

void WebLocalFrameClient::SetLinkPreviewTriggererForTesting(
    std::unique_ptr<WebLinkPreviewTriggerer> trigger) {}

}  // namespace blink
