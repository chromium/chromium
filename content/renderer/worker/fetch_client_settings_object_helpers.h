// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_FETCH_CLIENT_SETTINGS_OBJECT_HELPERS_H_
#define CONTENT_RENDERER_WORKER_FETCH_CLIENT_SETTINGS_OBJECT_HELPERS_H_

#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"

namespace blink {
struct WebFetchClientSettingsObject;
}  // namespace blink

namespace content {

// Helper functions for converting FetchClientSettingsObject variants.
// TODO(bashi): Remove these helpers when the Onion Soup is done.

blink::mojom::FetchClientSettingsObjectPtr
FetchClientSettingsObjectFromWebToMojom(
    const blink::WebFetchClientSettingsObject& web_settings_object);

blink::WebFetchClientSettingsObject FetchClientSettingsObjectFromMojomToWeb(
    const blink::mojom::FetchClientSettingsObjectPtr& mojom_settings_object);

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_FETCH_CLIENT_SETTINGS_OBJECT_HELPERS_H_
