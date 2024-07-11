// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"

#include "base/memory/protected_memory.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

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

}  // namespace blink
