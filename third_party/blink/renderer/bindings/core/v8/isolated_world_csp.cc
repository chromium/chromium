// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

class IsolatedWorldCSPDelegate final
    : public GarbageCollected<IsolatedWorldCSPDelegate>,
      public ContentSecurityPolicyDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(IsolatedWorldCSPDelegate);

 public:
  IsolatedWorldCSPDelegate(Document& document,
                           scoped_refptr<SecurityOrigin> security_origin,
                           int32_t world_id,
                           bool apply_policy)
      : document_(&document),
        security_origin_(std::move(security_origin)),
        world_id_(world_id),
        apply_policy_(apply_policy) {
    DCHECK(security_origin_);
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(document_);
    ContentSecurityPolicyDelegate::Trace(visitor);
  }

  const SecurityOrigin* GetSecurityOrigin() override {
    return security_origin_.get();
  }

  const KURL& Url() const override {
    // This is used to populate violation data's violation url. See
    // https://w3c.github.io/webappsec-csp/#violation-url.
    // TODO(crbug.com/916885): Figure out if we want to support violation
    // reporting for isolated world CSPs.
    DEFINE_STATIC_LOCAL(const KURL, g_empty_url, ());
    return g_empty_url;
  }

  // Isolated world CSPs don't support these directives: "sandbox",
  // "trusted-types" and "upgrade-insecure-requests".
  //
  // These directives depend on ExecutionContext for their implementation and
  // since isolated worlds don't have their own ExecutionContext, these are not
  // supported.
  void SetSandboxFlags(SandboxFlags) override {}
  void SetRequireTrustedTypes() override {}
  void AddInsecureRequestPolicy(WebInsecureRequestPolicy) override {}

  // TODO(crbug.com/916885): Figure out if we want to support violation
  // reporting for isolated world CSPs.
  std::unique_ptr<SourceLocation> GetSourceLocation() override {
    return nullptr;
  }
  base::Optional<uint16_t> GetStatusCode() override { return base::nullopt; }
  String GetDocumentReferrer() override { return g_empty_string; }
  void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                              Element*) override {
    DCHECK(apply_policy_);
  }
  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           const String& stringified_report,
                           bool is_frame_ancestors_violation,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api) override {
    DCHECK(apply_policy_);
  }

  void Count(WebFeature feature) override {
    // Log the features used by isolated world CSPs on the underlying Document.
    UseCounter::Count(document_, feature);
  }

  void AddConsoleMessage(ConsoleMessage* console_message) override {
    // Add console messages on the underlying Document.
    document_->AddConsoleMessage(console_message);
  }

  void DisableEval(const String& error_message) override {
    if (!document_->GetFrame())
      return;
    document_->GetFrame()->GetScriptController().DisableEvalForIsolatedWorld(
        world_id_, error_message);
  }

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) override {
    // This allows users to set breakpoints in the Devtools for the case when
    // script execution is blocked by CSP.
    probe::ScriptExecutionBlockedByCSP(document_, directive_text);
  }

  void DidAddContentSecurityPolicies(
      const blink::WebVector<WebContentSecurityPolicy>&) override {}

 private:
  const Member<Document> document_;
  const scoped_refptr<SecurityOrigin> security_origin_;
  const int32_t world_id_;

  // Whether the 'IsolatedWorldCSP' feature is enabled, and we are applying the
  // CSP provided by the isolated world.
  const bool apply_policy_;
};

}  // namespace

// static
IsolatedWorldCSP& IsolatedWorldCSP::Get() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(IsolatedWorldCSP, g_isolated_world_csp, ());
  return g_isolated_world_csp;
}

void IsolatedWorldCSP::SetContentSecurityPolicy(
    int32_t world_id,
    const String& policy,
    scoped_refptr<SecurityOrigin> self_origin) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  if (!policy) {
    csp_map_.erase(world_id);
    return;
  }

  DCHECK(self_origin);
  PolicyInfo policy_info;
  policy_info.policy = policy;
  policy_info.self_origin = std::move(self_origin);
  csp_map_.Set(world_id, policy_info);
}

bool IsolatedWorldCSP::HasContentSecurityPolicy(int32_t world_id) const {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  return it != csp_map_.end();
}

ContentSecurityPolicy* IsolatedWorldCSP::CreateIsolatedWorldCSP(
    Document& document,
    int32_t world_id) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  if (it == csp_map_.end())
    return nullptr;

  const String& policy = it->value.policy;
  scoped_refptr<SecurityOrigin> self_origin = it->value.self_origin;

  const bool apply_policy = RuntimeEnabledFeatures::IsolatedWorldCSPEnabled();

  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();

  IsolatedWorldCSPDelegate* delegate =
      MakeGarbageCollected<IsolatedWorldCSPDelegate>(
          document, std::move(self_origin), world_id, apply_policy);
  csp->BindToDelegate(*delegate);

  if (apply_policy) {
    csp->AddPolicyFromHeaderValue(policy,
                                  kContentSecurityPolicyHeaderTypeEnforce,
                                  kContentSecurityPolicyHeaderSourceHTTP);
  }

  return csp;
}

IsolatedWorldCSP::IsolatedWorldCSP() = default;

}  // namespace blink
