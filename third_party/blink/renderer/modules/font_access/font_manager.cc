// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/font_manager.h"

#include <algorithm>

#include "base/feature_list.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_query_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/font_access/font_metadata.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using mojom::blink::FontEnumerationStatus;

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

ScriptPromise FontManager::query(ScriptState* script_state,
                                 const QueryOptions* options,
                                 ExceptionState& exception_state) {
  if (!remote_manager_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "FontAccessManager backend went away");
    return ScriptPromise();
  }
  DCHECK(options->hasSelect());
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  remote_manager_->EnumerateLocalFonts(WTF::Bind(
      &FontManager::DidGetEnumerationResponse, WrapWeakPersistent(this),
      WrapPersistent(resolver), options->select()));
  return promise;
}

void FontManager::Trace(blink::Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void FontManager::DidGetEnumerationResponse(
    ScriptPromiseResolver* resolver,
    const Vector<String>& selection,
    FontEnumerationStatus status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK(resolver);
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     resolver->GetScriptState()))
    return;

  ScriptState::Scope script_state_scope(resolver->GetScriptState());
  if (RejectPromiseIfNecessary(status, resolver))
    return;

  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  FontEnumerationTable table;

  if (mapping.size() > INT_MAX) {
    // Cannot deserialize without overflow.
    resolver->Reject(V8ThrowDOMException::CreateOrDie(
        resolver->GetScriptState()->GetIsolate(), DOMExceptionCode::kDataError,
        "Font data exceeds memory limit."));
    return;
  }

  // Used to compare with data coming from the browser to avoid conversions.
  std::set<std::string> selection_utf8;
  for (const String& postscriptName : selection) {
    // While postscript names are encoded in a subset of ASCII, we convert the
    // input into UTF8. This will still allow exact matches to occur.
    selection_utf8.insert(postscriptName.Utf8());
  }

  HeapVector<Member<FontMetadata>> entries;
  table.ParseFromArray(mapping.memory(), static_cast<int>(mapping.size()));
  for (const auto& element : table.fonts()) {
    // If the selection list contains items, only allow items that match.
    if (!selection_utf8.empty() &&
        selection_utf8.find(element.postscript_name().c_str()) ==
            selection_utf8.end())
      continue;

    auto entry = FontEnumerationEntry{
        .postscript_name = String::FromUTF8(element.postscript_name().c_str()),
        .full_name = String::FromUTF8(element.full_name().c_str()),
        .family = String::FromUTF8(element.family().c_str()),
        .style = String::FromUTF8(element.style().c_str()),
    };
    entries.push_back(FontMetadata::Create(std::move(entry)));
  }

  resolver->Resolve(std::move(entries));
}

bool FontManager::RejectPromiseIfNecessary(const FontEnumerationStatus& status,
                                           ScriptPromiseResolver* resolver) {
  switch (status) {
    case FontEnumerationStatus::kOk:
      break;
    case FontEnumerationStatus::kUnimplemented:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kNotSupportedError,
          "Not yet supported on this platform."));
      return true;
    case FontEnumerationStatus::kCanceled:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kAbortError, "The user canceled the operation."));
      return true;
    case FontEnumerationStatus::kNeedsUserActivation:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kSecurityError, "User activation is required."));
      return true;
    case FontEnumerationStatus::kNotVisible:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kSecurityError, "Page needs to be visible."));
      return true;
    case FontEnumerationStatus::kPermissionDenied:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kNotAllowedError, "Permission not granted."));
      return true;
    case FontEnumerationStatus::kUnexpectedError:
    default:
      resolver->Reject(V8ThrowDOMException::CreateOrDie(
          resolver->GetScriptState()->GetIsolate(),
          DOMExceptionCode::kUnknownError, "An unexpected error occured."));
      return true;
  }
  return false;
}

void FontManager::ContextDestroyed() {
  remote_manager_.reset();
}

void FontManager::OnDisconnect() {
  remote_manager_.reset();
}

}  // namespace blink
