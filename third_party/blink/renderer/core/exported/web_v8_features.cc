// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_v8_features.h"

#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

// static
void WebV8Features::EnableMojoJS(v8::Local<v8::Context> context, bool enable) {
  ScriptState* script_state = ScriptState::From(context);
  DCHECK(script_state->World().IsMainWorld());
  ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnableMojoJS(enable);
}

// static
void WebV8Features::EnableMojoJSAndUseBroker(
    v8::Local<v8::Context> context,
    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote) {
  EnableMojoJS(context, /*enable*/ true);
  blink::ExecutionContext::From(context)->SetMojoJSInterfaceBroker(
      std::move(broker_remote));
}

// static
void WebV8Features::EnableMojoJSFileSystemAccessHelper(
    v8::Local<v8::Context> context,
    bool enable) {
  ScriptState* script_state = ScriptState::From(context);
  DCHECK(script_state->World().IsMainWorld());

  auto* context_feature_settings = ContextFeatureSettings::From(
      ExecutionContext::From(script_state),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists);

  if (!context_feature_settings->isMojoJSEnabled())
    return;

  context_feature_settings->EnableMojoJSFileSystemAccessHelper(enable);
}

}  // namespace blink
