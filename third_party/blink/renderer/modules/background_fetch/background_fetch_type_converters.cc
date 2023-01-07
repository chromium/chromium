// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_options.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/manifest/image_resource_type_converters.h"

namespace mojo {

blink::mojom::blink::BackgroundFetchOptionsPtr
TypeConverter<blink::mojom::blink::BackgroundFetchOptionsPtr,
              const blink::BackgroundFetchOptions*>::
    Convert(const blink::BackgroundFetchOptions* options) {
  blink::mojom::blink::BackgroundFetchOptionsPtr mojo_options =
      blink::mojom::blink::BackgroundFetchOptions::New();

  WTF::Vector<blink::mojom::blink::ManifestImageResourcePtr> mojo_icons;
  mojo_icons.ReserveInitialCapacity(options->icons().size());

  for (auto& icon : options->icons()) {
    mojo_icons.push_back(
        blink::mojom::blink::ManifestImageResource::From(icon.Get()));
  }

  mojo_options->icons = std::move(mojo_icons);
  mojo_options->download_total = options->downloadTotal();
  mojo_options->title = options->hasTitle() ? options->title() : "";

  return mojo_options;
}

}  // namespace mojo
