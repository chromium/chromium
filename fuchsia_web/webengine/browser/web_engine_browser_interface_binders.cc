// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_browser_interface_binders.h"

#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/web_engine_media_resource_provider_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

void PopulateFuchsiaFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<mojom::WebEngineMediaResourceProvider>(
      base::BindRepeating(&WebEngineMediaResourceProviderImpl::Bind));
}
