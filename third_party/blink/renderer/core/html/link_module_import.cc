// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_module_import.h"

#include "base/check.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_script.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class LinkModuleImport::ModuleClient final : public SingleModuleClient {
 public:
  explicit ModuleClient(LinkModuleImport* module_import)
      : module_import_(module_import) {}

  void Dispose() { module_import_ = nullptr; }

  void NotifyFailure() {
    NotifyModuleLoadFinished(nullptr, v8::ModuleImportPhase::kEvaluation);
  }

  void NotifyModuleLoadFinished(ModuleScript* module_script,
                                v8::ModuleImportPhase) override {
    if (module_import_) {
      module_import_->NotifyModuleLoadFinished(this, module_script);
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(module_import_);
    SingleModuleClient::Trace(visitor);
  }

 private:
  Member<LinkModuleImport> module_import_;
};

LinkModuleImport::LinkModuleImport(HTMLLinkElement* owner)
    : LinkResource(owner) {}

LinkModuleImport::~LinkModuleImport() = default;

void LinkModuleImport::Process(LinkLoadParameters::Reason reason) {
  // If this link is no longer a module stylesheet (e.g. rel or import changed),
  // clear the association when reprocessed.
  // TODO(448174611): Have HTMLLinkElement reach this cleanup even when
  // ShouldLoadLink() is false, and switch the resource type when needed.
  if (!owner_->IsModuleStyleSheetLink()) {
    CancelPendingRequest();
    resolved_url_ = KURL();
    loaded_ = false;
    return;
  }

  if (reason == LinkLoadParameters::Reason::kMediaChange) {
    return;
  }

  CancelPendingRequest();
  loaded_ = false;

  // The `import` attribute value is resolved as a module specifier.
  // TODO(448174611): Use href without import-map resolution for an empty
  // import.
  const String specifier = owner_->FastGetAttribute(html_names::kImportAttr);

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window || !window->GetFrame()) {
    resolved_url_ = KURL();
    ScheduleFailure();
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(window->GetFrame());
  if (!script_state) {
    // A frame can outlive its usable script context, e.g. after V8 memory
    // purge.
    resolved_url_ = KURL();
    ScheduleFailure();
    return;
  }

  Modulator* modulator = Modulator::From(script_state);
  CHECK(modulator);

  auto* client = MakeGarbageCollected<ModuleClient>(this);
  pending_ = client;

  String failure_reason;
  modulator->SetAcquiringImportMapsState(
      Modulator::AcquiringImportMapsState::kAfterModuleScriptLoad);
  resolved_url_ = modulator->ResolveModuleSpecifier(
      specifier, GetDocument().BaseURL(), &failure_reason);

  // TODO(448174611): Report failure_reason in the DevTools console when module
  // specifier resolution fails, alongside the link's error event.
  if (!resolved_url_.IsValid()) {
    CancelPendingRequest();
    ScheduleFailure();
    return;
  }

  // TODO(448174611): Honor link credentials, referrer policy, and fetch
  // priority, and compute render-blocking behavior.
  ScriptFetchOptions options(
      g_empty_string, IntegrityMetadataSet(), g_empty_string,
      owner_->IsCreatedByParser() ? kParserInserted : kNotParserInserted,
      network::mojom::CredentialsMode::kSameOrigin,
      network::mojom::ReferrerPolicy::kDefault,
      mojom::blink::FetchPriorityHint::kAuto,
      RenderBlockingBehavior::kNonBlocking);

  ModuleScriptFetchRequest request(
      resolved_url_, ModuleType::kCSS, mojom::blink::RequestContextType::STYLE,
      network::mojom::RequestDestination::kStyle, options,
      Referrer::ClientReferrerString(), TextPosition::MinimumPosition(),
      v8::ModuleImportPhase::kEvaluation);
  modulator->FetchSingle(request, window->Fetcher(),
                         ModuleGraphLevel::kTopLevelModuleFetch,
                         ModuleScriptCustomFetchType::kNone, client);
}

void LinkModuleImport::OwnerRemoved() {
  CancelPendingRequest();
  resolved_url_ = KURL();
  loaded_ = false;
}

void LinkModuleImport::CancelPendingRequest() {
  if (pending_) {
    pending_->Dispose();
    pending_ = nullptr;
  }
}

void LinkModuleImport::ScheduleFailure() {
  CHECK(!pending_);
  auto* client = MakeGarbageCollected<ModuleClient>(this);
  pending_ = client;
  GetDocument()
      .GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE, BindOnce(&ModuleClient::NotifyFailure,
                                     WrapPersistent(client)));
}

void LinkModuleImport::NotifyModuleLoadFinished(ModuleClient* client,
                                                ModuleScript* module_script) {
  if (pending_ != client) {
    return;
  }
  pending_ = nullptr;
  loaded_ = module_script && !module_script->HasParseError();
  owner_->ScheduleEvent();
}

void LinkModuleImport::Trace(Visitor* visitor) const {
  visitor->Trace(pending_);
  LinkResource::Trace(visitor);
}

}  // namespace blink
