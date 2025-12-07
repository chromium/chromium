// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/patch.h"

#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/fetch_api.mojom-data-view.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/mutation_observer.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/core/patching/patch_event.h"
#include "third_party/blink/renderer/core/patching/patch_supplement.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8-exception.h"
#include "v8-isolate.h"

namespace blink {
// static
Patch* Patch::Create(ContainerNode& target,
                     HTMLTemplateElement* source,
                     const KURL& source_url,
                     Node* previous_child,
                     Node* next_child) {
  return MakeGarbageCollected<Patch>(source, target, source_url, previous_child,
                                     next_child);
}

Patch::Patch(HTMLTemplateElement* source,
             ContainerNode& target,
             const KURL& source_url,
             Node* previous_child,
             Node* next_child)
    : source_(source),
      target_(target),
      previous_child_(previous_child),
      next_child_(next_child),
      finished_(
          MakeGarbageCollected<ScriptPromiseProperty<IDLUndefined, IDLAny>>(
              target.GetDocument().GetExecutionContext())),
      source_url_(source_url) {}

ScriptPromise<IDLUndefined> Patch::finished(ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void Patch::Start() {
  if (state_ != State::kPending) {
    return;
  }
  // A patch replaces the existing children of the target.
  MutationObserver::EnqueuePatch(*this);
  PatchSupplement::From(GetDocument())->DidStart(*target_, this);
  Commit();
}

void Patch::Commit() {
  state_ = State::kActive;
  loader_.Clear();
  if (!next_child_ && !previous_child_) {
    target_->RemoveChildren();
  } else {
    while (true) {
      Node* next = previous_child_ ? previous_child_->nextSibling()
                                   : target_->firstChild();
      if (!next || next == next_child_) {
        break;
      }
      target_->RemoveChild(next);
    }
    if (next_child_) {
      buffer_fragment_ = DocumentFragment::Create(GetDocument());
    }
  }

  parser_ = MakeGarbageCollected<HTMLDocumentParser>(
      buffer_fragment_ ? buffer_fragment_ : target_,
      target_->IsElementNode() ? &To<Element>(*target_)
                               : target_->parentElement(),
      ParserContentPolicy::kAllowScriptingContentAndDoNotMarkAlreadyStarted,
      ParserPrefetchPolicy::kDisallowPrefetching, /*registry*/ nullptr);
}

void Patch::DispatchPatchEvent() {
  Event* event =
      MakeGarbageCollected<PatchEvent>(event_type_names::kPatch, this);
  event->SetTarget(target_);
  target_->DispatchEvent(*event);
}

void Patch::Finish() {
  if (state_ == State::kTerminated) {
    return;
  }

  parser_->Finish();

  // TODO(nrosenthal): see if we can also stream between.
  if (buffer_fragment_) {
    CHECK(next_child_);

    // We need to check that this InsertBefore is still valid.
    // Since patching is an async operation, the child we use as the
    // insertBefore reference might no longer be connected or no longer the
    // child of this parent. In that case, we reject the patch's promise with
    // the error thrown by the DOM operation.
    v8::Isolate* isolate = GetDocument().GetExecutionContext()->GetIsolate();
    ExceptionState exception_state(isolate);
    TryRethrowScope rethrow(isolate, exception_state);
    target_->InsertBefore(buffer_fragment_.Release(), next_child_,
                          exception_state);
    if (rethrow.HasCaught()) {
      Terminate(ScriptValue(isolate, rethrow.GetException()));
      return;
    }
  }

  if (!source_url_.IsEmpty()) {
    state_ = State::kPending;
    // TODO(nrosenthal): start fetching earlier and buffer the response if
    // races with inline content
    Fetch();
    return;
  }

  state_ = State::kFinished;
  finished_->ResolveWithUndefined();
  PatchSupplement::From(GetDocument())->DidComplete(*target_);
}

void Patch::Terminate(ScriptValue reason) {
  if (state_ == State::kFinished || state_ == State::kTerminated) {
    return;
  }

  state_ = State::kTerminated;

  parser_->Finish();
  if (loader_) {
    loader_->Cancel();
    loader_.Release();
  }
  PatchSupplement::From(GetDocument())->DidComplete(*target_);
  finished_->Reject(reason);
}

void Patch::Append(const String& text) {
  if (state_ != State::kTerminated) {
    parser_->Append(text);
  }
}

void Patch::AppendBytes(base::span<uint8_t> bytes) {
  if (state_ == State::kTerminated) {
    return;
  }
  if (parser_->NeedsDecoder()) {
    parser_->SetDecoder(
        std::make_unique<TextResourceDecoder>(TextResourceDecoderOptions(
            TextResourceDecoderOptions::ContentType::kHTMLContent,
            GetDocument().Encoding())));
  }
  parser_->AppendBytes(bytes);
}

Document& Patch::GetDocument() {
  return target_->GetDocument();
}

void Patch::Fetch() {
  ResourceRequest request(source_url_);
  source_url_ = KURL();
  // TODO(nrosenthal): add actual patch request destination & context
  request.SetRequestDestination(network::mojom::RequestDestination::kScript);
  request.SetRequestContext(mojom::blink::RequestContextType::SUBRESOURCE);

  // TODO(nrosenthal): add a crossorigin property
  request.SetCredentialsMode(network::mojom::CredentialsMode::kSameOrigin);
  request.SetMode(network::mojom::RequestMode::kCors);
  // TODO(nrosenthal): change accept header based on target element
  request.SetHttpHeaderField(http_names::kAccept, AtomicString("text/html"));
  ResourceLoaderOptions resource_loader_options(
      GetDocument().GetExecutionContext()->GetCurrentWorld());
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  loader_ = MakeGarbageCollected<ThreadableLoader>(
      *GetDocument().GetExecutionContext(), this, resource_loader_options);
  loader_->Start(std::move(request));
}

void Patch::DidReceiveResponse(uint64_t, const ResourceResponse& response) {
  if (!network::IsSuccessfulStatus(response.HttpStatusCode())) {
    // TODO(nrosenthal): use different DOMExceptions for different statuses?
    // Or maybe using "network error" for all of them?
    if (response.HttpStatusCode() == net::HTTP_NOT_FOUND) {
      OnFetchError(DOMExceptionCode::kNotFoundError, response.HttpStatusText());
    } else {
      OnFetchError(DOMExceptionCode::kNetworkError, response.HttpStatusText());
    }
  } else {
    Commit();
  }
}

void Patch::DidReceiveData(base::span<const char> bytes) {
  AppendBytes(reinterpret_cast<const base::span<uint8_t>&>(bytes));
}

void Patch::DidFinishLoading(uint64_t /*identifier*/) {
  Finish();
}

void Patch::DidFail(uint64_t /*identifier*/, const ResourceError& error) {
  OnFetchError(DOMExceptionCode::kNetworkError,
               AtomicString("Failed to fetch resource"));
}

void Patch::OnFetchError(DOMExceptionCode code, const AtomicString& message) {
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetExecutionContext());
  ScriptState::Scope scope(script_state);
  v8::Local<v8::Value> exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), code, message);
  Terminate(ScriptValue(script_state->GetIsolate(), exception));
}

void Patch::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(target_);
  visitor->Trace(previous_child_);
  visitor->Trace(next_child_);
  visitor->Trace(finished_);
  visitor->Trace(parser_);
  visitor->Trace(buffer_fragment_);
  visitor->Trace(loader_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
