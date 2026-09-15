/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_runtime_features.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"
#include "third_party/blink/renderer/platform/runtime_enabled_feature_checks.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void WebRuntimeFeatures::InitializeMojoJSPermissions() {
  blink::InitializeMojoJSPermissions();
}

void WebRuntimeFeatures::EnableExperimentalFeatures(bool enable) {
  RuntimeEnabledFeatures::SetExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableFeatureFromString(std::string_view name,
                                                 bool enable) {
  if (enable) {
    if (name == "MojoJS") {
      blink::AllowMojoJSForProcess();
    } else if (name == "MojoJSTest") {
      blink::AllowMojoJSTestForProcess();
    }
  }
  RuntimeEnabledFeatures::SetFeatureEnabledFromString(name, enable);
}

void WebRuntimeFeatures::UpdateStatusFromBaseFeatures() {
  // MojoJSTest is not controlled by a corresponding base::Feature.
  if (base::FeatureList::IsEnabled(features::kMojoJS)) {
    blink::AllowMojoJSForProcess();
  }
  RuntimeEnabledFeatures::UpdateStatusFromBaseFeatures();
}

void WebRuntimeFeatures::EnableTestOnlyFeatures(bool enable) {
  // Many test fixtures call this early on and don't expect to have to
  // explicitly initialize the ProtectedMemory storage for MojoJS, et cetera, so
  // handle it here as a convenience for tests.
  blink::InitializeMojoJSPermissions();

  if (enable) {
    blink::AllowMojoJSForProcess();
    blink::AllowMojoJSPerContextForProcess();
    blink::AllowMojoJSTestForProcess();
  }
  RuntimeEnabledFeatures::SetTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableOriginTrialControlledFeatures(bool enable) {
  RuntimeEnabledFeatures::SetOriginTrialControlledFeaturesEnabled(enable);
}

void WebRuntimeFeatures::EnableOverlayScrollbars(bool enable) {
  ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::EnableFluentScrollbars(bool enable) {
  ScrollbarThemeSettings::SetFluentScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::EnableDesktopAndroidScrollbars(bool enable) {
  ScrollbarThemeSettings::SetDesktopAndroidScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::EnableLocalNetworkAccessWebRTC(bool enable) {
  RuntimeEnabledFeatures::SetLocalNetworkAccessWebRTCEnabled(enable);
}

// static
void WebRuntimeFeatures::SetMojoJSFeaturesEnabledForTesting(bool mojo_js,
                                                            bool mojo_js_test) {
  RuntimeEnabledFeatures::SetMojoJSEnabled(mojo_js);
  RuntimeEnabledFeatures::SetMojoJSTestEnabled(mojo_js_test);
}

ScopedDisallowMojoJsForTesting::ScopedDisallowMojoJsForTesting()
    : mojo_js_enabled_(RuntimeEnabledFeatures::MojoJSEnabled()),
      mojo_js_test_enabled_(RuntimeEnabledFeatures::MojoJSTestEnabled()),
      mojo_js_per_context_allowed_(
          blink::IsMojoJSAllowedPerContextForProcess()),
      mojo_js_runtime_feature_allowed_(blink::IsMojoJSAllowedForProcess()),
      mojo_js_test_runtime_feature_allowed_(
          blink::IsMojoJSTestAllowedForProcess()) {
  WebRuntimeFeatures::SetMojoJSFeaturesEnabledForTesting(false, false);
  blink::SetMojoJSPermissionsForTesting(false, false, false);
}

ScopedDisallowMojoJsForTesting::~ScopedDisallowMojoJsForTesting() {
  blink::SetMojoJSPermissionsForTesting(mojo_js_per_context_allowed_,
                                        mojo_js_runtime_feature_allowed_,
                                        mojo_js_test_runtime_feature_allowed_);
  WebRuntimeFeatures::SetMojoJSFeaturesEnabledForTesting(mojo_js_enabled_,
                                                         mojo_js_test_enabled_);
}

}  // namespace blink
