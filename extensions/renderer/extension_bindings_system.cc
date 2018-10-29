// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_bindings_system.h"

#include "base/metrics/histogram_macros.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

const int kHistogramBucketCount = 50;

}  // namespace

// static
bool ExtensionBindingsSystem::IsRuntimeAvailableToContext(
    ScriptContext* context) {
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    ExternallyConnectableInfo* info = static_cast<ExternallyConnectableInfo*>(
        extension->GetManifestData(manifest_keys::kExternallyConnectable));
    if (info && info->matches.MatchesURL(context->url()))
      return true;
  }
  return false;
}

// static
const char* const ExtensionBindingsSystem::kWebAvailableFeatures[] = {
    "app", "dashboardPrivate",
};

void ExtensionBindingsSystem::LogUpdateBindingsForContextTime(
    Feature::Context context_type,
    base::TimeDelta elapsed) {
  static const int kTenSecondsInMicroseconds = 10000000;
  switch (context_type) {
    case Feature::UNSPECIFIED_CONTEXT:
      break;
    case Feature::WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime.WebPageContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "BlessedWebPageContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::SERVICE_WORKER_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "ServiceWorkerContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::BLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "BlessedExtensionContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "LockScreenExtensionContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "UnblessedExtensionContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::CONTENT_SCRIPT_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime."
          "ContentScriptContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
      break;
    case Feature::WEBUI_CONTEXT:
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Extensions.Bindings.UpdateBindingsForContextTime.WebUIContext",
          elapsed.InMicroseconds(), 1, kTenSecondsInMicroseconds,
          kHistogramBucketCount);
  }
}

}  // namespace extensions
