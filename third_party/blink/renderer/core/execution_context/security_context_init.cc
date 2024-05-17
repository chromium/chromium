// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/security_context_init.h"

#include <optional>

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/common/frame/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/permissions_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

// Helper function to filter out features that are not in origin trial in
// ParsedDocumentPolicy.
DocumentPolicy::ParsedDocumentPolicy FilterByOriginTrial(
    const DocumentPolicy::ParsedDocumentPolicy& parsed_policy,
    ExecutionContext* context) {
  DocumentPolicy::ParsedDocumentPolicy filtered_policy;
  for (auto i = parsed_policy.feature_state.begin(),
            last = parsed_policy.feature_state.end();
       i != last;) {
    if (!DisabledByOriginTrial(i->first, context))
      filtered_policy.feature_state.insert(*i);
    ++i;
  }
  for (auto i = parsed_policy.endpoint_map.begin(),
            last = parsed_policy.endpoint_map.end();
       i != last;) {
    if (!DisabledByOriginTrial(i->first, context))
      filtered_policy.endpoint_map.insert(*i);
    ++i;
  }
  return filtered_policy;
}

}  // namespace

// A helper class that allows the security context be initialized in the
// process of constructing the document.
SecurityContextInit::SecurityContextInit(ExecutionContext* context)
    : execution_context_(context) {}

void SecurityContextInit::ApplyDocumentPolicy(
    DocumentPolicy::ParsedDocumentPolicy& document_policy,
    const String& report_only_document_policy_header) {
  // Because Document-Policy http header is parsed in DocumentLoader,
  // when origin trial context is not initialized yet.
  // Needs to filter out features that are not in origin trial after
  // we have origin trial information available.
  document_policy = FilterByOriginTrial(document_policy, execution_context_);
  if (!document_policy.feature_state.empty()) {
    UseCounter::Count(execution_context_, WebFeature::kDocumentPolicyHeader);
    for (const auto& policy_entry : document_policy.feature_state) {
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.DocumentPolicy.Header",
                                policy_entry.first);
    }
  }
  execution_context_->GetSecurityContext().SetDocumentPolicy(
      DocumentPolicy::CreateWithHeaderPolicy(document_policy));

  // Handle Report-Only-Document-Policy HTTP header.
  // Console messages generated from logger are discarded, because currently
  // there is no way to output them to console.
  // Calling |Document::AddConsoleMessage| in
  // |SecurityContextInit::ApplyPendingDataToDocument| will have no effect,
  // because when the function is called, the document is not fully initialized
  // yet (|document_| field in current frame is not yet initialized yet).
  DocumentPolicy::ParsedDocumentPolicy report_only_document_policy;
  PolicyParserMessageBuffer logger("%s", /* discard_message */ true);
  std::optional<DocumentPolicy::ParsedDocumentPolicy>
      report_only_parsed_policy = DocumentPolicyParser::Parse(
          report_only_document_policy_header, logger);
  if (report_only_parsed_policy) {
    report_only_document_policy =
        FilterByOriginTrial(*report_only_parsed_policy, execution_context_);
    if (!report_only_document_policy.feature_state.empty()) {
      UseCounter::Count(execution_context_,
                        WebFeature::kDocumentPolicyReportOnlyHeader);
      execution_context_->GetSecurityContext().SetReportOnlyDocumentPolicy(
          DocumentPolicy::CreateWithHeaderPolicy(report_only_document_policy));
    }
  }
}

void SecurityContextInit::ApplyPermissionsPolicy(
    LocalFrame& frame,
    const ResourceResponse& response,
    const FramePolicy& frame_policy,
    const std::optional<ParsedPermissionsPolicy>& isolated_app_policy,
    const base::optional_ref<const FencedFrame::RedactedFencedFrameProperties>
        fenced_frame_properties) {
  const url::Origin origin =
      execution_context_->GetSecurityOrigin()->ToUrlOrigin();
  // If we are a HTMLViewSourceDocument we use container, header or
  // inherited policies. https://crbug.com/898688.
  if (frame.InViewSourceMode()) {
    execution_context_->GetSecurityContext().SetPermissionsPolicy(
        PermissionsPolicy::CreateFromParentPolicy(nullptr, /*header_policy=*/{},
                                                  {}, origin));
    return;
  }

  const String& permissions_policy_header =
      response.HttpHeaderField(http_names::kPermissionsPolicy);
  const String& report_only_permissions_policy_header =
      response.HttpHeaderField(http_names::kPermissionsPolicyReportOnly);
  if (!permissions_policy_header.empty())
    UseCounter::Count(execution_context_, WebFeature::kPermissionsPolicyHeader);

  PolicyParserMessageBuffer feature_policy_logger(
      "Error with Feature-Policy header: ");
  PolicyParserMessageBuffer report_only_feature_policy_logger(
      "Error with Feature-Policy-Report-Only header: ");

  PolicyParserMessageBuffer permissions_policy_logger(
      "Error with Permissions-Policy header: ");
  PolicyParserMessageBuffer report_only_permissions_policy_logger(
      "Error with Permissions-Policy-Report-Only header: ");

  WTF::StringBuilder policy_builder;
  policy_builder.Append(response.HttpHeaderField(http_names::kFeaturePolicy));
  String feature_policy_header = policy_builder.ToString();
  if (!feature_policy_header.empty())
    UseCounter::Count(execution_context_, WebFeature::kFeaturePolicyHeader);

  permissions_policy_header_ = PermissionsPolicyParser::ParseHeader(
      feature_policy_header, permissions_policy_header,
      execution_context_->GetSecurityOrigin(), feature_policy_logger,
      permissions_policy_logger, execution_context_);

  ParsedPermissionsPolicy parsed_report_only_permissions_policy_header =
      PermissionsPolicyParser::ParseHeader(
          response.HttpHeaderField(http_names::kFeaturePolicyReportOnly),
          report_only_permissions_policy_header,
          execution_context_->GetSecurityOrigin(),
          report_only_feature_policy_logger,
          report_only_permissions_policy_logger, execution_context_);

  if (!response.HttpHeaderField(http_names::kFeaturePolicyReportOnly).empty()) {
    UseCounter::Count(execution_context_,
                      WebFeature::kFeaturePolicyReportOnlyHeader);
  }

  auto messages = Vector<PolicyParserMessageBuffer::Message>();
  messages.AppendVector(feature_policy_logger.GetMessages());
  messages.AppendVector(report_only_feature_policy_logger.GetMessages());
  messages.AppendVector(permissions_policy_logger.GetMessages());
  messages.AppendVector(report_only_permissions_policy_logger.GetMessages());

  for (const auto& message : messages) {
    execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity, message.level,
        message.content));
  }

  ParsedPermissionsPolicy container_policy;
  if (frame.Owner() || frame.IsFencedFrameRoot()) {
    container_policy = frame_policy.container_policy;
  }

  // DocumentLoader applied the sandbox flags before calling this function, so
  // they are accessible here.
  auto sandbox_flags = execution_context_->GetSandboxFlags();

  if (RuntimeEnabledFeatures::BlockingFocusWithoutUserActivationEnabled() &&
      frame.Tree().Parent() &&
      (sandbox_flags & network::mojom::blink::WebSandboxFlags::kNavigation) !=
          network::mojom::blink::WebSandboxFlags::kNone) {
    // Enforcing the policy for sandbox frames (for context see
    // https://crbug.com/954349).
    DisallowFeatureIfNotPresent(
        mojom::blink::PermissionsPolicyFeature::kFocusWithoutUserActivation,
        container_policy);
  }

  if (isolated_app_policy) {
    DCHECK(frame.IsOutermostMainFrame());
    std::unique_ptr<PermissionsPolicy> permissions_policy =
        PermissionsPolicy::CreateFromParsedPolicy(permissions_policy_header_,
                                                  isolated_app_policy, origin);
    execution_context_->GetSecurityContext().SetPermissionsPolicy(
        std::move(permissions_policy));
  } else {
    std::unique_ptr<PermissionsPolicy> permissions_policy;
    if (frame.IsFencedFrameRoot()) {
      if (!fenced_frame_properties.has_value()) {
        // Without fenced frame properties, there won't be a list of effective
        // enabled permissions or information about the embedder's permissions
        // policies, so we create a permissions policy with every permission
        // disabled.
        permissions_policy = PermissionsPolicy::CreateFixedForFencedFrame(
            origin, /*header_policy=*/permissions_policy_header_, {});
      } else if (fenced_frame_properties->parent_permissions_info()
                     .has_value()) {
        // Fenced frames with flexible permissions are allowed to inherit
        // certain permissions from their parent.
        auto parent_permissions_policy =
            PermissionsPolicy::CreateFromParsedPolicy(
                fenced_frame_properties->parent_permissions_info()
                    ->parsed_permissions_policy,
                /*base_policy=*/std::nullopt,
                fenced_frame_properties->parent_permissions_info()->origin);

        permissions_policy = PermissionsPolicy::CreateFlexibleForFencedFrame(
            parent_permissions_policy.get(),
            /*header_policy=*/permissions_policy_header_, container_policy,
            origin);

        // Warn if a disallowed permissions policy is attempted to be enabled.
        for (const auto& policy : container_policy) {
          if (!base::Contains(blink::kFencedFrameAllowedFeatures,
                              policy.feature)) {
            bool is_isolated_context =
                execution_context_ && execution_context_->IsIsolatedContext();
            execution_context_->AddConsoleMessage(
                MakeGarbageCollected<ConsoleMessage>(
                    mojom::blink::ConsoleMessageSource::kSecurity,
                    mojom::blink::ConsoleMessageLevel::kWarning,
                    "The permissions policy '" +
                        GetNameForFeature(policy.feature, is_isolated_context) +
                        "' is disallowed in fenced frames and will not be "
                        "enabled."));
          }
        }
      } else {
        // Fenced frames with fixed permissions have a list of required
        // permission policies to load and can't be granted extra policies, so
        // use the required policies instead of inheriting from its parent. Note
        // that the parent policies must allow the required policies, which is
        // checked separately in
        // NavigationRequest::CheckPermissionsPoliciesForFencedFrames.
        permissions_policy = PermissionsPolicy::CreateFixedForFencedFrame(
            origin, /*header_policy=*/permissions_policy_header_,
            fenced_frame_properties->effective_enabled_permissions());
      }
    } else {
      auto* parent_permissions_policy = frame.Tree().Parent()
                                            ? frame.Tree()
                                                  .Parent()
                                                  ->GetSecurityContext()
                                                  ->GetPermissionsPolicy()
                                            : nullptr;
      permissions_policy = PermissionsPolicy::CreateFromParentPolicy(
          parent_permissions_policy,
          /*header_policy=*/permissions_policy_header_, container_policy,
          origin);
    }
    execution_context_->GetSecurityContext().SetPermissionsPolicy(
        std::move(permissions_policy));
  }

  // Report-only permissions policy only takes effect when it is stricter than
  // enforced permissions policy, i.e. when enforced permissions policy allows a
  // feature while report-only permissions policy do not. In such scenario, a
  // report-only policy violation report will be generated, but the feature is
  // still allowed to be used. Since child frames cannot loosen enforced
  // permissions policy, there is no need to inherit parent policy and container
  // policy for report-only permissions policy. For inherited policies, the
  // behavior is dominated by enforced permissions policy.
  if (!parsed_report_only_permissions_policy_header.empty()) {
    std::unique_ptr<PermissionsPolicy> report_only_policy =
        PermissionsPolicy::CreateFromParentPolicy(
            nullptr /* parent_policy */,
            /*header_policy=*/parsed_report_only_permissions_policy_header,
            {} /* container_policy */,
            execution_context_->GetSecurityOrigin()->ToUrlOrigin());
    execution_context_->GetSecurityContext().SetReportOnlyPermissionsPolicy(
        std::move(report_only_policy));
  }
}

void SecurityContextInit::InitPermissionsPolicyFrom(
    const SecurityContext& other) {
  auto& security_context = execution_context_->GetSecurityContext();
  security_context.SetPermissionsPolicy(
      PermissionsPolicy::CopyStateFrom(other.GetPermissionsPolicy()));
  security_context.SetReportOnlyPermissionsPolicy(
      PermissionsPolicy::CopyStateFrom(other.GetReportOnlyPermissionsPolicy()));
}

void SecurityContextInit::InitDocumentPolicyFrom(const SecurityContext& other) {
  auto& security_context = execution_context_->GetSecurityContext();
  security_context.SetDocumentPolicy(
      DocumentPolicy::CopyStateFrom(other.GetDocumentPolicy()));
  security_context.SetReportOnlyDocumentPolicy(
      DocumentPolicy::CopyStateFrom(other.GetReportOnlyDocumentPolicy()));
}
}  // namespace blink
