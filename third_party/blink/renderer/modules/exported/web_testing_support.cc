/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_testing_support.h"

#include <tuple>

#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/bindings/modules/v8/init_idl_interfaces_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/properties_per_feature_installer_for_testing.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/core/testing/v8/web_core_test_support.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

RuntimeEnabledFeatures::Backup* g_features_backup = nullptr;

InstallPropertiesPerFeatureFuncType
    g_original_install_properties_per_feature_func;

void InstallPropertiesPerFeatureForTesting(
    ScriptState* script_state,
    mojom::blink::OriginTrialFeature feature) {
  bindings::InstallPropertiesPerFeatureForTesting(script_state, feature);
  if (g_original_install_properties_per_feature_func)
    g_original_install_properties_per_feature_func(script_state, feature);
}

bool EnsureV8BindingsForTestingInternal() {
  bindings::InitIDLInterfacesForTesting();
  g_original_install_properties_per_feature_func =
      SetInstallPropertiesPerFeatureFunc(InstallPropertiesPerFeatureForTesting);
  return true;
}

void EnsureV8BindingsForTesting() {
  static bool unused = EnsureV8BindingsForTestingInternal();
  std::ignore = unused;
}

}  // namespace

WebTestingSupport::WebScopedMockScrollbars::WebScopedMockScrollbars()
    : use_mock_scrollbars_(std::make_unique<ScopedMockOverlayScrollbars>()) {}

WebTestingSupport::WebScopedMockScrollbars::~WebScopedMockScrollbars() =
    default;

void WebTestingSupport::SaveRuntimeFeatures() {
  DCHECK(!g_features_backup);
  g_features_backup = new RuntimeEnabledFeatures::Backup;
}

void WebTestingSupport::ResetRuntimeFeatures() {
  g_features_backup->Restore();
}

void WebTestingSupport::InjectInternalsObject(WebLocalFrame* frame) {
  EnsureV8BindingsForTesting();
  v8::HandleScope handle_scope(frame->GetAgentGroupScheduler()->Isolate());
  web_core_test_support::InjectInternalsObject(frame->MainWorldScriptContext());
}

void WebTestingSupport::InjectInternalsObject(v8::Local<v8::Context> context) {
  EnsureV8BindingsForTesting();
  web_core_test_support::InjectInternalsObject(context);
}

void WebTestingSupport::ResetMainFrame(WebLocalFrame* main_frame) {
  auto* main_frame_impl = To<WebLocalFrameImpl>(main_frame);
  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  web_core_test_support::ResetInternalsObject(
      main_frame_impl->MainWorldScriptContext());
  main_frame_impl->GetFrame()
      ->GetWindowProxyManager()
      ->ResetIsolatedWorldsForTesting();
}

}  // namespace blink
