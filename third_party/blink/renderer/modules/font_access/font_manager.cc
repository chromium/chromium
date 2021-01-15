// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_manager.h"

#include <algorithm>

#include "base/feature_list.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/font_access/font_iterator.h"
#include "third_party/blink/renderer/modules/font_access/font_metadata.h"
#include "third_party/blink/renderer/modules/font_access/query_options.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

void ReturnDataFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, info.Data());
}

}  // namespace

FontManager::FontManager(ExecutionContext* context)
    : ExecutionContextLifecycleObserver(context) {
  // Only connect if the feature is enabled. Otherwise, there will
  // be no service to connect to on the end.
  if (base::FeatureList::IsEnabled(blink::features::kFontAccess)) {
    context->GetBrowserInterfaceBroker().GetInterface(
        remote_manager_.BindNewPipeAndPassReceiver());
    remote_manager_.set_disconnect_handler(
        WTF::Bind(&FontManager::OnDisconnect, WrapWeakPersistent(this)));
  }
}

ScriptValue FontManager::query(ScriptState* script_state,
                               const QueryOptions* options,
                               ExceptionState& exception_state) {
  if (exception_state.HadException())
    return ScriptValue();

  Vector<String> selection =
      options->hasSelect() ? options->select() : Vector<String>();

  auto* iterator = MakeGarbageCollected<FontIterator>(
      ExecutionContext::From(script_state), std::move(selection));
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!result
           ->Set(context, v8::Symbol::GetAsyncIterator(isolate),
                 v8::Function::New(context, &ReturnDataFunction,
                                   ToV8(iterator, script_state))
                     .ToLocalChecked())
           .ToChecked()) {
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), result);
}

ScriptPromise FontManager::showFontChooser(ScriptState* script_state,
                                           const QueryOptions* options) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  remote_manager_->ChooseLocalFonts(WTF::Bind(&FontManager::DidShowFontChooser,
                                              WrapWeakPersistent(this),
                                              WrapPersistent(resolver)));

  return promise;
}

void FontManager::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void FontManager::DidShowFontChooser(
    ScriptPromiseResolver* resolver,
    mojom::blink::FontEnumerationStatus status,
    Vector<mojom::blink::FontMetadataPtr> fonts) {
  switch (status) {
    case mojom::blink::FontEnumerationStatus::kOk:
      break;
    case mojom::blink::FontEnumerationStatus::kUnimplemented:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "Not yet supported on this platform."));
      return;
    case mojom::blink::FontEnumerationStatus::kCanceled:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "The user canceled the operation."));
      return;
    case mojom::blink::FontEnumerationStatus::kNeedsUserActivation:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError, "User activation is required."));
      return;
    case mojom::blink::FontEnumerationStatus::kUnexpectedError:
    default:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "An unexpected error occured."));
      return;
  }

  auto entries = HeapVector<Member<FontMetadata>>();
  for (const auto& font : fonts) {
    auto entry = FontEnumerationEntry{font->postscript_name, font->full_name,
                                      font->family};
    entries.push_back(FontMetadata::Create(std::move(entry)));
  }
  resolver->Resolve(std::move(entries));
}

void FontManager::ContextDestroyed() {
  remote_manager_.reset();
}

void FontManager::OnDisconnect() {
  remote_manager_.reset();
}

}  // namespace blink
