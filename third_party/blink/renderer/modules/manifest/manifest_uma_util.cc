// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_uma_util.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

namespace {

static const char kUMANameParseSuccess[] = "Manifest.ParseSuccess";
static const char kUMANameFetchResult[] = "Manifest.FetchResult";

// Enum for UMA purposes, make sure you update histograms.xml if you add new
// result types. Never delete or reorder an entry; only add new entries
// immediately before MANIFEST_FETCH_RESULT_TYPE_COUNT.
enum ManifestFetchResultType {
  MANIFEST_FETCH_SUCCESS = 0,
  MANIFEST_FETCH_ERROR_EMPTY_URL = 1,
  MANIFEST_FETCH_ERROR_UNSPECIFIED = 2,
  MANIFEST_FETCH_ERROR_FROM_OPAQUE_ORIGIN = 3,

  // Must stay at the end.
  MANIFEST_FETCH_RESULT_TYPE_COUNT
};

}  // anonymous namespace

void ManifestUmaUtil::ParseSucceeded(
    const mojom::blink::ManifestPtr& manifest) {
  UMA_HISTOGRAM_BOOLEAN(kUMANameParseSuccess, true);

  auto empty_manifest = mojom::blink::Manifest::New();
  if (manifest == empty_manifest)
    return;

  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.name", !manifest->name.IsEmpty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.short_name",
                        !manifest->short_name.IsEmpty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.start_url",
                        !manifest->start_url.IsEmpty());
  UMA_HISTOGRAM_BOOLEAN(
      "Manifest.HasProperty.display",
      manifest->display != blink::mojom::DisplayMode::kUndefined);
  UMA_HISTOGRAM_BOOLEAN(
      "Manifest.HasProperty.orientation",
      manifest->orientation != kWebScreenOrientationLockDefault);
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.icons",
                        !manifest->icons.IsEmpty());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.share_target",
                        manifest->share_target.get());
  UMA_HISTOGRAM_BOOLEAN("Manifest.HasProperty.gcm_sender_id",
                        !manifest->gcm_sender_id.IsEmpty());
}

void ManifestUmaUtil::ParseFailed() {
  UMA_HISTOGRAM_BOOLEAN(kUMANameParseSuccess, false);
}

void ManifestUmaUtil::FetchSucceeded() {
  UMA_HISTOGRAM_ENUMERATION(kUMANameFetchResult, MANIFEST_FETCH_SUCCESS,
                            MANIFEST_FETCH_RESULT_TYPE_COUNT);
}

void ManifestUmaUtil::FetchFailed(FetchFailureReason reason) {
  ManifestFetchResultType fetch_result_type = MANIFEST_FETCH_RESULT_TYPE_COUNT;
  switch (reason) {
    case FETCH_EMPTY_URL:
      fetch_result_type = MANIFEST_FETCH_ERROR_EMPTY_URL;
      break;
    case FETCH_FROM_OPAQUE_ORIGIN:
      fetch_result_type = MANIFEST_FETCH_ERROR_FROM_OPAQUE_ORIGIN;
      break;
    case FETCH_UNSPECIFIED_REASON:
      fetch_result_type = MANIFEST_FETCH_ERROR_UNSPECIFIED;
      break;
  }
  DCHECK_NE(fetch_result_type, MANIFEST_FETCH_RESULT_TYPE_COUNT);

  UMA_HISTOGRAM_ENUMERATION(kUMANameFetchResult, fetch_result_type,
                            MANIFEST_FETCH_RESULT_TYPE_COUNT);
}

}  // namespace blink
