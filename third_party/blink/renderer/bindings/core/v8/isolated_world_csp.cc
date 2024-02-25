// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/isolated_world_csp.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

enum class CSPType { kEmpty, kNonEmpty };

class IsolatedWorldCSPDelegate final
    : public GarbageCollected<IsolatedWorldCSPDelegate>,
      public ContentSecurityPolicyDelegate {

 public:
  IsolatedWorldCSPDelegate(LocalDOMWindow& window,
                           scoped_refptr<SecurityOrigin> security_origin,
                           int32_t world_id,
                           CSPType type)
      : window_(&window),
        security_origin_(std::move(security_origin)),
        world_id_(world_id),
        csp_type_(type) {
    DCHECK(security_origin_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(window_);
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
  void SetSandboxFlags(network::mojom::blink::WebSandboxFlags) override {}
  void SetRequireTrustedTypes() override {}
  void AddInsecureRequestPolicy(mojom::blink::InsecureRequestPolicy) override {}

  // TODO(crbug.com/916885): Figure out if we want to support violation
  // reporting for isolated world CSPs.
  std::unique_ptr<SourceLocation> GetSourceLocation() override {
    return nullptr;
  }
  std::optional<uint16_t> GetStatusCode() override { return std::nullopt; }
  String GetDocumentReferrer() override { return g_empty_string; }
  void DispatchViolationEvent(const SecurityPolicyViolationEventInit&,
                              Element*) override {
    // Sanity check that an empty CSP doesn't lead to a violation.
    DCHECK(csp_type_ == CSPType::kNonEmpty);
  }
  void PostViolationReport(const SecurityPolicyViolationEventInit&,
                           const String& stringified_report,
                           bool is_frame_ancestors_violation,
                           const Vector<String>& report_endpoints,
                           bool use_reporting_api) override {
    // Sanity check that an empty CSP doesn't lead to a violation.
    DCHECK(csp_type_ == CSPType::kNonEmpty);
  }

  void Count(WebFeature feature) override {
    // Log the features used by isolated world CSPs on the underlying window.
    UseCounter::Count(window_, feature);
  }

  void AddConsoleMessage(ConsoleMessage* console_message) override {
    // Add console messages on the underlying window.
    window_->AddConsoleMessage(console_message);
  }

  void AddInspectorIssue(AuditsIssue issue) override {
    window_->AddInspectorIssue(std::move(issue));
  }

  void DisableEval(const String& error_message) override {
    window_->GetScriptController().DisableEvalForIsolatedWorld(world_id_,
                                                               error_message);
  }

  void SetWasmEvalErrorMessage(const String& error_message) override {
    window_->GetScriptController().SetWasmEvalErrorMessageForIsolatedWorld(
        world_id_, error_message);
  }

  void ReportBlockedScriptExecutionToInspector(
      const String& directive_text) override {
    // This allows users to set breakpoints in the Devtools for the case when
    // script execution is blocked by CSP.
    probe::ScriptExecutionBlockedByCSP(window_.Get(), directive_text);
  }

  void DidAddContentSecurityPolicies(
      WTF::Vector<network::mojom::blink::ContentSecurityPolicyPtr>) override {}

 private:
  const Member<LocalDOMWindow> window_;
  const scoped_refptr<SecurityOrigin> security_origin_;
  const int32_t world_id_;
  const CSPType csp_type_;
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
    LocalDOMWindow& window,
    int32_t world_id) {
  DCHECK(IsMainThread());
  DCHECK(DOMWrapperWorld::IsIsolatedWorldId(world_id));

  auto it = csp_map_.find(world_id);
  if (it == csp_map_.end())
    return nullptr;

  const String& policy = it->value.policy;
  scoped_refptr<SecurityOrigin> self_origin = it->value.self_origin;

  auto* csp = MakeGarbageCollected<ContentSecurityPolicy>();

  IsolatedWorldCSPDelegate* delegate =
      MakeGarbageCollected<IsolatedWorldCSPDelegate>(
          window, self_origin, world_id,
          policy.empty() ? CSPType::kEmpty : CSPType::kNonEmpty);
  csp->BindToDelegate(*delegate);
  csp->AddPolicies(ParseContentSecurityPolicies(
      policy, network::mojom::blink::ContentSecurityPolicyType::kEnforce,
      network::mojom::blink::ContentSecurityPolicySource::kHTTP,
      *(self_origin)));

  return csp;
}

IsolatedWorldCSP::IsolatedWorldCSP() = default;

}  // namespace blink
