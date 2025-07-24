// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/patching/dom_patch_status.h"

#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/fetch_api.mojom-data-view.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abstract_range.h"
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
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
// static
DOMPatchStatus* DOMPatchStatus::Create(ContainerNode& target,
                                       HTMLTemplateElement* source,
                                       const KURL& source_url) {
  return MakeGarbageCollected<DOMPatchStatus>(source, target, source_url);
}

DOMPatchStatus::DOMPatchStatus(HTMLTemplateElement* source,
                               ContainerNode& target,
                               const KURL& source_url)
    : source_(source),
      target_(target),
      finished_(
          MakeGarbageCollected<ScriptPromiseProperty<IDLUndefined, IDLAny>>(
              target.GetDocument().GetExecutionContext())),
      source_url_(source_url) {}

ScriptPromise<IDLUndefined> DOMPatchStatus::finished(
    ScriptState* script_state) {
  return finished_->Promise(script_state->World());
}

void DOMPatchStatus::Start() {
  if (state_ != State::kPending) {
    return;
  }
  // A patch replaces the existing children of the target.
  MutationObserver::EnqueuePatch(*this);
  PatchSupplement::From(GetDocument())->DidStart(*target_, this);
  Commit();
}

void DOMPatchStatus::Commit() {
  state_ = State::kActive;
  loader_.Clear();
  target_->RemoveChildren();
  parser_ = MakeGarbageCollected<HTMLDocumentParser>(
      target_,
      target_->IsElementNode() ? &To<Element>(*target_)
                               : target_->parentElement(),
      ParserContentPolicy::kDisallowScriptingAndPluginContent);
}

void DOMPatchStatus::DispatchPatchEvent() {
  Event* event =
      MakeGarbageCollected<PatchEvent>(event_type_names::kPatch, this);
  event->SetTarget(target_);
  target_->DispatchEvent(*event);
}

class PatchLoaderClient : public GarbageCollected<PatchLoaderClient>,
                          public ThreadableLoaderClient {
 public:
  explicit PatchLoaderClient(DOMPatchStatus* patch) : patch_(patch) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(patch_);
    ThreadableLoaderClient::Trace(visitor);
  }

 private:
  void DidReceiveResponse(uint64_t, const ResourceResponse& response) override {
    if (!network::IsSuccessfulStatus(response.HttpStatusCode())) {
      // TODO(nrosenthal): use different DOMExceptions for different statuses?
      // Or maybe using "network error" for all of them?
      if (response.HttpStatusCode() == net::HTTP_NOT_FOUND) {
        OnError(DOMExceptionCode::kNotFoundError, response.HttpStatusText());
      } else {
        OnError(DOMExceptionCode::kNetworkError, response.HttpStatusText());
      }
    } else {
      patch_->Commit();
    }
  }
  void DidReceiveData(base::span<const char> bytes) override {
    patch_->AppendBytes(reinterpret_cast<const base::span<uint8_t>&>(bytes));
  }
  void DidFinishLoading(uint64_t /*identifier*/) override { patch_->Finish(); }
  void DidFail(uint64_t /*identifier*/, const ResourceError& error) override {
    OnError(DOMExceptionCode::kNetworkError,
            AtomicString("Failed to fetch resource"));
  }

  void OnError(DOMExceptionCode code, const AtomicString& message) {
    ScriptState* script_state =
        ToScriptStateForMainWorld(patch_->GetDocument().GetExecutionContext());
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Value> exception = V8ThrowDOMException::CreateOrEmpty(
        script_state->GetIsolate(), code, message);
    patch_->Terminate(ScriptValue(script_state->GetIsolate(), exception));
  }

  Member<DOMPatchStatus> patch_;
};

void DOMPatchStatus::Fetch() {
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
      *GetDocument().GetExecutionContext(),
      MakeGarbageCollected<PatchLoaderClient>(this), resource_loader_options);
  loader_->Start(std::move(request));
}

void DOMPatchStatus::Finish() {
  if (state_ == State::kTerminated) {
    return;
  }

  parser_->Finish();

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

void DOMPatchStatus::Append(const String& text) {
  if (state_ != State::kTerminated) {
    parser_->Append(text);
  }
}

void DOMPatchStatus::Terminate(ScriptValue reason) {
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

void DOMPatchStatus::AppendBytes(base::span<uint8_t> bytes) {
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

Document& DOMPatchStatus::GetDocument() {
  return target_->GetDocument();
}

void DOMPatchStatus::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(target_);
  visitor->Trace(finished_);
  visitor->Trace(parser_);
  visitor->Trace(loader_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
