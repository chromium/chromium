// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"

#include "base/memory/protected_memory.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ContextFeatureSettings::ContextFeatureSettings(ExecutionContext& context)
    : Supplement<ExecutionContext>(context) {}

// static
const char ContextFeatureSettings::kSupplementName[] = "ContextFeatureSettings";

DEFINE_PROTECTED_DATA base::ProtectedMemory<bool>
    ContextFeatureSettings::mojo_js_allowed_;

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
void ContextFeatureSettings::InitializeMojoJSAllowedProtectedMemory() {
  static base::ProtectedMemoryInitializer mojo_js_allowed_initializer(
      mojo_js_allowed_, false);
}

// static
void ContextFeatureSettings::AllowMojoJSForProcess() {
  if (*mojo_js_allowed_) {
    // Already allowed. No need to make protected memory writable.
    return;
  }

  base::AutoWritableMemory mojo_js_allowed_writer(mojo_js_allowed_);
  mojo_js_allowed_writer.GetProtectedData() = true;
}

// static
void ContextFeatureSettings::CrashIfMojoJSNotAllowed() {
  CHECK(*mojo_js_allowed_);
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
// Permissions require UnboundedElement to be enabled AND either:
// 1) UnboundedElementOnTheOpenWeb is enabled,
// 2) The context was marked privileged (enable_unbounded_element_), or
// 3) The context origin is a privileged WebUI scheme
//    (SecurityOrigin::IsWebUI(), excluding chrome-untrusted).
//
// Note that enable_unbounded_element_ is set for WebUI contexts in
// RenderFrameImpl, but can also be set when UnboundedElementOnTheOpenWeb is
// enabled. Therefore, we check SecurityOrigin::IsWebUI() to distinguish
// kAllowedPrivileged (which does not require user activation) from
// kAllowedOpenWeb.
ContextFeatureSettings::UnboundedElementAuth
ContextFeatureSettings::GetUnboundedElementAuth(
    const ExecutionContext* context) {
  if (!RuntimeEnabledFeatures::UnboundedElementEnabled() || !context) {
    return UnboundedElementAuth::kDenied;
  }
  const SecurityOrigin* security_origin = context->GetSecurityOrigin();
  bool is_privileged = security_origin && security_origin->IsWebUI() &&
                       security_origin->Protocol() != "chrome-untrusted";
  if (is_privileged) {
    return UnboundedElementAuth::kAllowedPrivileged;
  }
  const auto* settings =
      Supplement<ExecutionContext>::From<ContextFeatureSettings>(context);
  bool is_allowed =
      (settings && settings->enable_unbounded_element_) ||
      RuntimeEnabledFeatures::UnboundedElementOnTheOpenWebEnabled();
  if (is_allowed) {
    return UnboundedElementAuth::kAllowedOpenWeb;
  }
  return UnboundedElementAuth::kDenied;
}

bool ContextFeatureSettings::isUnboundedElementEnabled() const {
  return GetUnboundedElementAuth(GetSupplementable()) !=
         UnboundedElementAuth::kDenied;
}

}  // namespace blink
