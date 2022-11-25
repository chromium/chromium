// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_uma_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace blink {

namespace {

static const char kUMAIdParseResult[] = "Manifest.ParseIdResult";

}  // anonymous namespace

void ManifestUmaUtil::ParseSucceeded(
    const mojom::blink::ManifestPtr& manifest) {
  auto empty_manifest = mojom::blink::Manifest::New();
  if (manifest == empty_manifest)
    return;

  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.name", !manifest->name.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.short_name",
                        !manifest->short_name.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.description",
                        !manifest->description.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.start_url",
                        !manifest->start_url.IsEmpty());
  UMA_HISTOGRAM_BOOLEAN(
      "Manifest.HasProperty.display",
      manifest->display != blink::mojom::DisplayMode::kUndefined);
  UMA_HISTOGRAM_BOOLEAN(
      "Manifest.HasProperty.orientation",
      manifest->orientation !=
          device::mojom::blink::ScreenOrientationLockType::DEFAULT);
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.icons", !manifest->icons.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.screenshots",
                        !manifest->screenshots.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.share_target",
                        manifest->share_target.get());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.protocol_handlers",
                        !manifest->protocol_handlers.empty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.gcm_sender_id",
                        !manifest->gcm_sender_id.empty());
}

void ManifestUmaUtil::ParseIdResult(ParseIdResultType result) {
  base::UmaHistogramEnumeration(kUMAIdParseResult, result);
}

}  // namespace blink
