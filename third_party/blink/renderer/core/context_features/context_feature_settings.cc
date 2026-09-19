// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_feature_checks.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ContextFeatureSettings::ContextFeatureSettings(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

// static
const char ContextFeatureSettings::kSupplementName[] = "ContextFeatureSettings";

// static
ContextFeatureSettings* ContextFeatureSettings::From(
    ExecutionContext* context,
    CreationMode creation_mode) {
  ContextFeatureSettings* settings =
      Supplement<ExecutionContext>::From<ContextFeatureSettings>(context);
  if (!settings && creation_mode == CreationMode::kCreateIfNotExists) {
    settings = MakeGarbageCollected<ContextFeatureSettings>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, settings);
  }
  return settings;
}

// static
void ContextFeatureSettings::CrashIfMojoJSNotAllowed() {
  CHECK(IsMojoJSAllowedPerContextForProcess());
}

void ContextFeatureSettings::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
}

bool ContextFeatureSettings::isMojoJSEnabled() const {
  if (enable_mojo_js_) {
    // If enable_mojo_js_ is true and mojo_js_allowed_ isn't also true, then it
    // means enable_mojo_js_ was set to true without going through the proper
    // code paths, suggesting an attack. In this case, we should crash.
    // (crbug.com/976506)
    CrashIfMojoJSNotAllowed();
  }
  return enable_mojo_js_;
}

// static
// Preconditions/permissions for unbounded elements are checked in:
// - RenderFrameHostImpl::GetUnboundedElementAuth (browser side)
// - ContextFeatureSettings::GetUnboundedElementAuth (renderer side)
//
// In the renderer, RenderFrameImpl::DidCreateScriptContext determines whether
// the context is allowed to use UnboundedElement (either via open-web flag or
// privileged WebUI/extension origin) and configures ContextFeatureSettings via
// WebV8Features::EnableUnboundedElement(context, is_privileged).
ContextFeatureSettings::UnboundedElementAuth
ContextFeatureSettings::GetUnboundedElementAuth(
    const ExecutionContext* context) {
  if (!RuntimeEnabledFeatures::UnboundedElementEnabled() || !context) {
    return UnboundedElementAuth::kDenied;
  }
  const auto* settings =
      Supplement<ExecutionContext>::From<ContextFeatureSettings>(context);
  if (!settings || !settings->enable_unbounded_element_) {
    return UnboundedElementAuth::kDenied;
  }
  return settings->enable_unbounded_element_privileged_
             ? UnboundedElementAuth::kAllowedPrivileged
             : UnboundedElementAuth::kAllowedOpenWeb;
}

bool ContextFeatureSettings::isUnboundedElementEnabled() const {
  return GetUnboundedElementAuth(GetSupplementable()) !=
         UnboundedElementAuth::kDenied;
}

}  // namespace blink
