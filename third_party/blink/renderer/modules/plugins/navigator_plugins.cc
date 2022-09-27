// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"

#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"

namespace blink {

namespace {
bool ShouldReturnFixedPluginData(Navigator& navigator) {
  if (auto* window = navigator.DomWindow()) {
    if (auto* frame = window->GetFrame()) {
      if (frame->GetSettings()->GetAllowNonEmptyNavigatorPlugins()) {
        // See https://crbug.com/1171373 for more context. P/Nacl plugins will
        // be supported on some platforms through at least June, 2022. Since
        // some apps need to use feature detection, we need to continue
        // returning plugin data for those.
        return false;
      }
    }
  }
  // Otherwise, return fixed plugin data.
  return true;
}
}  // namespace

NavigatorPlugins::NavigatorPlugins(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      should_return_fixed_plugin_data_(ShouldReturnFixedPluginData(navigator)) {
}

// static
NavigatorPlugins& NavigatorPlugins::From(Navigator& navigator) {
  NavigatorPlugins* supplement = ToNavigatorPlugins(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorPlugins>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
NavigatorPlugins* NavigatorPlugins::ToNavigatorPlugins(Navigator& navigator) {
  return Supplement<Navigator>::From<NavigatorPlugins>(navigator);
}

// static
const char NavigatorPlugins::kSupplementName[] = "NavigatorPlugins";

// static
DOMPluginArray* NavigatorPlugins::plugins(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).plugins(navigator.DomWindow());
}

// static
DOMMimeTypeArray* NavigatorPlugins::mimeTypes(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).mimeTypes(navigator.DomWindow());
}

// static
bool NavigatorPlugins::pdfViewerEnabled(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).pdfViewerEnabled(
      navigator.DomWindow());
}

// static
bool NavigatorPlugins::javaEnabled(Navigator& navigator) {
  return false;
}

namespace {

void RecordPlugins(LocalDOMWindow* window, DOMPluginArray* plugins) {
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleWebFeature(
          WebFeature::kNavigatorPlugins) ||
      !window) {
    return;
  }
  IdentifiableTokenBuilder builder;
  for (unsigned i = 0; i < plugins->length(); i++) {
    DOMPlugin* plugin = plugins->item(i);
    builder.AddToken(IdentifiabilityBenignStringToken(plugin->name()));
    builder.AddToken(IdentifiabilityBenignStringToken(plugin->description()));
    builder.AddToken(IdentifiabilityBenignStringToken(plugin->filename()));
    for (unsigned j = 0; j < plugin->length(); j++) {
      DOMMimeType* mimeType = plugin->item(j);
      builder.AddToken(IdentifiabilityBenignStringToken(mimeType->type()));
      builder.AddToken(
          IdentifiabilityBenignStringToken(mimeType->description()));
      builder.AddToken(IdentifiabilityBenignStringToken(mimeType->suffixes()));
    }
  }
  IdentifiabilityMetricBuilder(window->UkmSourceID())
      .AddWebFeature(WebFeature::kNavigatorPlugins, builder.GetToken())
      .Record(window->UkmRecorder());
}

void RecordMimeTypes(LocalDOMWindow* window, DOMMimeTypeArray* mime_types) {
  constexpr IdentifiableSurface surface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, WebFeature::kNavigatorMimeTypes);
  if (!IdentifiabilityStudySettings::Get()->ShouldSampleSurface(surface) ||
      !window) {
    return;
  }
  IdentifiableTokenBuilder builder;
  for (unsigned i = 0; i < mime_types->length(); i++) {
    DOMMimeType* mime_type = mime_types->item(i);
    builder.AddToken(IdentifiabilityBenignStringToken(mime_type->type()));
    builder.AddToken(
        IdentifiabilityBenignStringToken(mime_type->description()));
    builder.AddToken(IdentifiabilityBenignStringToken(mime_type->suffixes()));
    DOMPlugin* plugin = mime_type->enabledPlugin();
    if (plugin) {
      builder.AddToken(IdentifiabilityBenignStringToken(plugin->name()));
      builder.AddToken(IdentifiabilityBenignStringToken(plugin->filename()));
      builder.AddToken(IdentifiabilityBenignStringToken(plugin->description()));
    }
  }
  IdentifiabilityMetricBuilder(window->UkmSourceID())
      .Add(surface, builder.GetToken())
      .Record(window->UkmRecorder());
}

}  // namespace

DOMPluginArray* NavigatorPlugins::plugins(LocalDOMWindow* window) const {
  if (!plugins_) {
    plugins_ = MakeGarbageCollected<DOMPluginArray>(
        window, should_return_fixed_plugin_data_);
  }

  DOMPluginArray* result = plugins_.Get();
  RecordPlugins(window, result);
  return result;
}

DOMMimeTypeArray* NavigatorPlugins::mimeTypes(LocalDOMWindow* window) const {
  if (!mime_types_) {
    mime_types_ = MakeGarbageCollected<DOMMimeTypeArray>(
        window, should_return_fixed_plugin_data_);
    RecordMimeTypes(window, mime_types_.Get());
  }
  return mime_types_.Get();
}

bool NavigatorPlugins::pdfViewerEnabled(LocalDOMWindow* window) const {
  return plugins(window)->IsPdfViewerAvailable();
}

void NavigatorPlugins::Trace(Visitor* visitor) const {
  visitor->Trace(plugins_);
  visitor->Trace(mime_types_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
