/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/script/script_loader.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/loader/url_matcher.h"
#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/import_map.h"
#include "third_party/blink/renderer/core/script/js_module_script.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_pending_script.h"
#include "third_party/blink/renderer/core/script/pending_import_map.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

scheduler::TaskAttributionInfo* GetRunningTask(ScriptState* script_state) {
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  if (!script_state || !script_state->World().IsMainWorld() || !tracker) {
    return nullptr;
  }
  return tracker->RunningTask();
}

}  // namespace

ScriptLoader::ScriptLoader(ScriptElementBase* element,
                           const CreateElementFlags flags)
    : element_(element) {
  // <spec href="https://html.spec.whatwg.org/C/#script-processing-model">
  // The cloning steps for a script element el being cloned to a copy copy are
  // to set copy's already started to el's already started.</spec>
  //
  // TODO(hiroshige): Cloning is implemented together with
  // {HTML,SVG}ScriptElement::cloneElementWithoutAttributesAndChildren().
  // Clean up these later.
  if (flags.WasAlreadyStarted())
    already_started_ = true;

  if (flags.IsCreatedByParser()) {
    // <spec href="https://html.spec.whatwg.org/C/#parser-inserted">... script
    // elements with non-null parser documents are known as
    // parser-inserted.</spec>
    //
    // For more information on why this is not implemented in terms of a
    // non-null parser document, see the documentation in the header file.
    parser_inserted_ = true;

    // <spec href="https://html.spec.whatwg.org/C/#parser-document">... It is
    // set by the HTML parser and the XML parser on script elements they insert,
    // ...</spec>
    parser_document_ = flags.ParserDocument();

    // <spec href="https://html.spec.whatwg.org/C/#script-force-async">... It is
    // set to false by the HTML parser and the XML parser on script elements
    // they insert, ...</spec>
    force_async_ = false;
  }
}

ScriptLoader::~ScriptLoader() {}

void ScriptLoader::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(parser_document_);
  visitor->Trace(prepared_pending_script_);
  visitor->Trace(resource_keep_alive_);
  visitor->Trace(script_web_bundle_);
  visitor->Trace(speculation_rule_set_);
  ResourceFinishObserver::Trace(visitor);
}

// <specdef
// href="https://html.spec.whatwg.org/C/#script-processing-model">
//
// <spec>When a script element el that is not parser-inserted experiences one of
// the events listed in the following list, the user agent must immediately
// prepare the script element el:</spec>
//
// The following three `PrepareScript()` are for non-parser-inserted scripts and
// thus
// - Should deny ParserBlockingInline scripts.
// - Should return nullptr, i.e. there should no PendingScript to be controlled
//   by parsers.
// - TextPosition is not given.

// <spec step="A">The script element becomes connected.</spec>
void ScriptLoader::DidNotifySubtreeInsertionsToDocument() {
  if (already_started_ &&
      GetScriptTypeAtPrepare(element_->TypeAttributeValue(),
                             element_->LanguageAttributeValue()) ==
          ScriptTypeAtPrepare::kSpeculationRules) {
    // See https://crbug.com/359355331, where this was requested.
    auto* message = MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kJavaScript, ConsoleMessage::Level::kWarning,
        "A speculation rule set was inserted into the document but will be "
        "ignored. This might happen, for example, if it was previously "
        "inserted into another document, or if it was created using the "
        "innerHTML setter.");
    element_->GetDocument().AddConsoleMessage(message,
                                              /*discard_duplicates=*/true);
  }

  if (!parser_inserted_) {
    PendingScript* pending_script = PrepareScript(
        ParserBlockingInlineOption::kDeny, TextPosition::MinimumPosition());
    DCHECK(!pending_script);
  }
}

// <spec step="B">The script element is connected and a node or document
// fragment is inserted into the script element, after any script elements
// inserted at that time.</spec>
void ScriptLoader::ChildrenChanged(
    const ContainerNode::ChildrenChange& change) {
  if (script_type_ == ScriptTypeAtPrepare::kSpeculationRules &&
      (change.type == ContainerNode::ChildrenChangeType::kTextChanged ||
       change.type == ContainerNode::ChildrenChangeType::kNonElementInserted ||
       change.type == ContainerNode::ChildrenChangeType::kNonElementRemoved) &&
      change.sibling_changed->IsCharacterDataNode()) {
    // See https://crbug.com/328100599.
    auto* message = MakeGarbageCollected<ConsoleMessage>(
        ConsoleMessage::Source::kJavaScript, ConsoleMessage::Level::kWarning,
        "Inline speculation rules cannot currently be modified after they are "
        "processed. Instead, a new <script> element must be inserted.");
    element_->GetDocument().AddConsoleMessage(message,
                                              /*discard_duplicates=*/true);
  }

  if (change.IsChildInsertion() && !parser_inserted_ &&
      element_->IsConnected()) {
    PendingScript* pending_script = PrepareScript(
        ParserBlockingInlineOption::kDeny, TextPosition::MinimumPosition());
    DCHECK(!pending_script);
  }
}

// <spec step="C">The script element is connected and has a src attribute set
// where previously the element had no such attribute.</spec>
void ScriptLoader::HandleSourceAttribute(const String& source_url) {
  if (!parser_inserted_ && element_->IsConnected() && !source_url.empty()) {
    PendingScript* pending_script = PrepareScript(
        ParserBlockingInlineOption::kDeny, TextPosition::MinimumPosition());
    DCHECK(!pending_script);
  }
}

void ScriptLoader::HandleAsyncAttribute() {
  // <spec>When an async attribute is added to a script element el, the user
  // agent must set el's force async to false.</spec>
  //
  // <spec href="https://html.spec.whatwg.org/C/#the-script-element"
  // step="1">Set this's force async to false.</spec>
  force_async_ = false;
}

void ScriptLoader::Removed() {
  // Release webbundle resources which are associated to this loader explicitly
  // without waiting for blink-GC.
  if (ScriptWebBundle* bundle = std::exchange(script_web_bundle_, nullptr))
    bundle->WillReleaseBundleLoaderAndUnregister();

  RemoveSpeculationRuleSet();
}

void ScriptLoader::DocumentBaseURLChanged() {
  if (GetScriptType() != ScriptTypeAtPrepare::kSpeculationRules) {
    return;
  }
  // We reparse the original source text and generate a new SpeculationRuleSet
  // with the new base URL. Note that any text changes since the first parse
  // will be ignored.
  if (SpeculationRuleSet* rule_set = RemoveSpeculationRuleSet()) {
    AddSpeculationRuleSet(rule_set->source());
  }
}

namespace {

// <specdef href="https://html.spec.whatwg.org/C/#prepare-the-script-element">
bool IsValidClassicScriptTypeAndLanguage(const String& type,
                                         const String& language) {
  if (type.IsNull()) {
    // <spec step="8.B">el has no type attribute but it has a language attribute
    // and that attribute's value is the empty string; or</spec>
    //
    // <spec step="8.C">el has neither a type attribute nor a language
    // attribute</spec>
    if (language.empty())
      return true;

    // <spec step="8">... Otherwise, el has a non-empty language attribute; let
    // the script block's type string be the concatenation of "text/" and the
    // value of el's language attribute.</spec>
    if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType("text/" + language))
      return true;
  } else if (type.empty()) {
    // <spec step="8.A">el has a type attribute whose value is the empty
    // string;</spec>
    return true;
  } else {
    // <spec step="8">... Otherwise, if el has a type attribute, then let the
    // script block's type string be the value of that attribute with leading
    // and trailing ASCII whitespace stripped. ...</spec>
    //
    // <spec step="9">If the script block's type string is a JavaScript MIME
    // type essence match, then set el's type to "classic".</spec>
    if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
            type.StripWhiteSpace())) {
      return true;
    }
  }

  return false;
}

bool IsSameSite(const KURL& url, const Document& element_document) {
  scoped_refptr<const SecurityOrigin> url_origin = SecurityOrigin::Create(url);
  return url_origin->IsSameSiteWith(
      element_document.GetExecutionContext()->GetSecurityOrigin());
}

bool IsDocumentReloadedOrFormSubmitted(const Document& element_document) {
  Document& top_document = element_document.TopDocument();
  return top_document.Loader() &&
         top_document.Loader()->IsReloadedOrFormSubmitted();
}

// Common eligibility conditions for the interventions below.
bool IsEligibleCommon(const Document& element_document) {
  // As some interventions need parser support (e.g. defer), interventions are
  // enabled only for HTMLDocuments, because XMLDocumentParser lacks support for
  // e.g. defer scripts. Thus the parser document (==element document) is
  // checked here.
  if (!IsA<HTMLDocument>(element_document))
    return false;

  // Do not enable interventions on reload.
  // No specific reason to use element document here instead of context
  // document though.
  if (IsDocumentReloadedOrFormSubmitted(element_document))
    return false;

  return true;
}

// [Intervention, ForceInOrderScript, crbug.com/1344772]
bool IsEligibleForForceInOrder(const Document& element_document) {
  return base::FeatureList::IsEnabled(features::kForceInOrderScript) &&
         IsEligibleCommon(element_document);
}

// [Intervention, DelayAsyncScriptExecution, crbug.com/1340837]
bool IsEligibleForDelay(const Resource& resource,
                        const Document& element_document,
                        const ScriptElementBase& element) {
  if (!base::FeatureList::IsEnabled(features::kDelayAsyncScriptExecution))
    return false;

  if (!IsEligibleCommon(element_document))
    return false;

  if (element.IsPotentiallyRenderBlocking())
    return false;

  // We don't delay async scripts that have matched a resource in the preload
  // cache, because we're using <link rel=preload> as a signal that the script
  // is higher-than-usual priority, and therefore should be executed earlier
  // rather than later.
  if (resource.IsLinkPreload())
    return false;

  // Most LCP elements are provided by the main frame, and delaying subframe's
  // resources seems not to improve LCP.
  const bool main_frame_only =
      features::kDelayAsyncScriptExecutionMainFrameOnlyParam.Get();
  if (main_frame_only && !element_document.IsInOutermostMainFrame())
    return false;

  const base::TimeDelta feature_limit =
      features::kDelayAsyncScriptExecutionFeatureLimitParam.Get();
  if (!feature_limit.is_zero() &&
      element_document.GetStartTime().Elapsed() > feature_limit) {
    return false;
  }

  bool is_ad_resource = resource.GetResourceRequest().IsAdResource();
  const features::AsyncScriptExperimentalSchedulingTarget target =
      features::kDelayAsyncScriptExecutionTargetParam.Get();
  switch (target) {
    case features::AsyncScriptExperimentalSchedulingTarget::kAds:
      if (!is_ad_resource) {
        return false;
      }
      break;
    case features::AsyncScriptExperimentalSchedulingTarget::kNonAds:
      if (is_ad_resource) {
        return false;
      }
      break;
    case features::AsyncScriptExperimentalSchedulingTarget::kBoth:
      break;
  }

  const bool opt_out_low =
      features::kDelayAsyncScriptExecutionOptOutLowFetchPriorityHintParam.Get();
  const bool opt_out_auto =
      features::kDelayAsyncScriptExecutionOptOutAutoFetchPriorityHintParam
          .Get();
  const bool opt_out_high =
      features::kDelayAsyncScriptExecutionOptOutHighFetchPriorityHintParam
          .Get();

  switch (resource.GetResourceRequest().GetFetchPriorityHint()) {
    case mojom::blink::FetchPriorityHint::kLow:
      if (opt_out_low) {
        return false;
      }
      break;
    case mojom::blink::FetchPriorityHint::kAuto:
      if (opt_out_auto) {
        return false;
      }
      break;
    case mojom::blink::FetchPriorityHint::kHigh:
      if (opt_out_high) {
        return false;
      }
      break;
  }

  const features::DelayAsyncScriptTarget delay_async_script_target =
      features::kDelayAsyncScriptTargetParam.Get();
  switch (delay_async_script_target) {
    case features::DelayAsyncScriptTarget::kAll:
      return true;
    case features::DelayAsyncScriptTarget::kCrossSiteOnly:
      return !IsSameSite(resource.Url(), element_document);
    case features::DelayAsyncScriptTarget::kCrossSiteWithAllowList:
    case features::DelayAsyncScriptTarget::kCrossSiteWithAllowListReportOnly:
      if (IsSameSite(resource.Url(), element_document))
        return false;
      DEFINE_STATIC_LOCAL(
          UrlMatcher, url_matcher,
          (UrlMatcher(features::kDelayAsyncScriptAllowList.Get())));
      return url_matcher.Match(resource.Url());
  }
}

// [Intervention, LowPriorityScriptLoading, crbug.com/1365763]
bool IsEligibleForLowPriorityScriptLoading(const Document& element_document,
                                           const ScriptElementBase& element,
                                           const KURL& url) {
  static const bool enabled =
      base::FeatureList::IsEnabled(features::kLowPriorityScriptLoading);
  if (!enabled)
    return false;

  if (!IsEligibleCommon(element_document))
    return false;

  if (element.IsPotentiallyRenderBlocking())
    return false;

  // Most LCP elements are provided by the main frame, and delaying subframe's
  // resources seems not to improve LCP.
  const bool main_frame_only =
      features::kLowPriorityScriptLoadingMainFrameOnlyParam.Get();
  if (main_frame_only && !element_document.IsInOutermostMainFrame())
    return false;

  const base::TimeDelta feature_limit =
      features::kLowPriorityScriptLoadingFeatureLimitParam.Get();
  if (!feature_limit.is_zero() &&
      element_document.GetStartTime().Elapsed() > feature_limit) {
    return false;
  }

  const bool cross_site_only =
      features::kLowPriorityScriptLoadingCrossSiteOnlyParam.Get();
  if (cross_site_only && IsSameSite(url, element_document))
    return false;

  DEFINE_STATIC_LOCAL(
      UrlMatcher, deny_list,
      (UrlMatcher(features::kLowPriorityScriptLoadingDenyListParam.Get())));
  if (deny_list.Match(url))
    return false;

  return true;
}

// [Intervention, SelectiveInOrderScript, crbug.com/1356396]
bool IsEligibleForSelectiveInOrder(const Resource& resource,
                                   const Document& element_document) {
  // The feature flag is checked separately.

  if (!IsEligibleCommon(element_document))
    return false;

  // Cross-site scripts only: 1st party scripts are out of scope of the
  // intervention.
  if (IsSameSite(resource.Url(), element_document))
    return false;

  // Only script request URLs in the allowlist.
  DEFINE_STATIC_LOCAL(
      UrlMatcher, url_matcher,
      (UrlMatcher(features::kSelectiveInOrderScriptAllowList.Get())));
  return url_matcher.Match(resource.Url());
}

ScriptRunner::DelayReasons DetermineDelayReasonsToWait(
    ScriptRunner* script_runner,
    bool is_eligible_for_delay) {
  using DelayReason = ScriptRunner::DelayReason;
  using DelayReasons = ScriptRunner::DelayReasons;

  DelayReasons reasons = static_cast<DelayReasons>(DelayReason::kLoad);

  if (is_eligible_for_delay &&
      script_runner->IsActive(DelayReason::kMilestone)) {
    reasons |= static_cast<DelayReasons>(DelayReason::kMilestone);
  }

  return reasons;
}

}  // namespace

ScriptLoader::ScriptTypeAtPrepare ScriptLoader::GetScriptTypeAtPrepare(
    const String& type,
    const String& language) {
  if (IsValidClassicScriptTypeAndLanguage(type, language)) {
    // <spec step="9">If the script block's type string is a JavaScript MIME
    // type essence match, then set el's type to "classic".</spec>
    return ScriptTypeAtPrepare::kClassic;
  }

  if (EqualIgnoringASCIICase(type, script_type_names::kModule)) {
    // <spec step="10">Otherwise, if the script block's type string is an ASCII
    // case-insensitive match for the string "module", then set el's type to
    // "module".</spec>
    return ScriptTypeAtPrepare::kModule;
  }

  if (EqualIgnoringASCIICase(type, script_type_names::kImportmap)) {
    return ScriptTypeAtPrepare::kImportMap;
  }

  if (EqualIgnoringASCIICase(type, script_type_names::kSpeculationrules)) {
    return ScriptTypeAtPrepare::kSpeculationRules;
  }
  if (EqualIgnoringASCIICase(type, script_type_names::kWebbundle)) {
    return ScriptTypeAtPrepare::kWebBundle;
  }

  // <spec step="11">Otherwise, return. (No script is executed, and el's type is
  // left as null.)</spec>
  return ScriptTypeAtPrepare::kInvalid;
}

bool ScriptLoader::BlockForNoModule(ScriptTypeAtPrepare script_type,
                                    bool nomodule) {
  return nomodule && script_type == ScriptTypeAtPrepare::kClassic;
}

// Corresponds to
// https://html.spec.whatwg.org/C/#module-script-credentials-mode
// which is a translation of the CORS settings attribute in the context of
// module scripts. This is used in:
//   - Step 17 of
//     https://html.spec.whatwg.org/C/#prepare-the-script-element
//   - Step 6 of obtaining a preloaded module script
//     https://html.spec.whatwg.org/C/#link-type-modulepreload.
network::mojom::CredentialsMode ScriptLoader::ModuleScriptCredentialsMode(
    CrossOriginAttributeValue cross_origin) {
  switch (cross_origin) {
    case kCrossOriginAttributeNotSet:
    case kCrossOriginAttributeAnonymous:
      return network::mojom::CredentialsMode::kSameOrigin;
    case kCrossOriginAttributeUseCredentials:
      return network::mojom::CredentialsMode::kInclude;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::CredentialsMode::kOmit;
}

// <specdef href="https://html.spec.whatwg.org/C/#prepare-the-script-element">
PendingScript* ScriptLoader::PrepareScript(
    ParserBlockingInlineOption parser_blocking_inline_option,
    const TextPosition& script_start_position) {
  // <spec step="1">If el's already started is true, then return.</spec>
  if (already_started_)
    return nullptr;

  // <spec step="2">Let parser document be el's parser document.</spec>
  //
  // Here and below we manipulate `parser_inserted_` flag instead of
  // `parser_document_`. See the comment at the `parser_document_` declaration.
  bool was_parser_inserted = parser_inserted_;

  // <spec step="3">Set el's parser document to null.</spec>
  parser_inserted_ = false;

  // <spec step="4">If parser document is non-null and el does not have an async
  // attribute, then set el's force async to true.</spec>
  if (was_parser_inserted && !element_->AsyncAttributeValue())
    force_async_ = true;

  // <spec step="5">Let source text be el's child text content.</spec>
  //
  // Trusted Types additionally requires:
  // https://w3c.github.io/trusted-types/dist/spec/#slot-value-verification
  // - Step 4: Execute the Prepare the script URL and text algorithm upon the
  //     script element. If that algorithm threw an error, then return. The
  //     script is not executed.
  // - Step 5: Let source text be the element’s [[ScriptText]] internal slot
  //     value.
  const String source_text = GetScriptText();

  // <spec step="6">If el has no src attribute, and source text is the empty
  // string, then return.</spec>
  if (!element_->HasSourceAttribute() && source_text.empty())
    return nullptr;

  // <spec step="7">If el is not connected, then return.</spec>
  if (!element_->IsConnected())
    return nullptr;

  Document& element_document = element_->GetDocument();
  LocalDOMWindow* context_window = element_document.domWindow();

  // Steps 8-11.
  script_type_ = GetScriptTypeAtPrepare(element_->TypeAttributeValue(),
                                        element_->LanguageAttributeValue());

  switch (GetScriptType()) {
    case ScriptTypeAtPrepare::kInvalid:
      return nullptr;

    case ScriptTypeAtPrepare::kSpeculationRules:
    case ScriptTypeAtPrepare::kWebBundle:
    case ScriptTypeAtPrepare::kClassic:
    case ScriptTypeAtPrepare::kModule:
    case ScriptTypeAtPrepare::kImportMap:
      break;
  }

  // <spec step="12">If parser document is non-null, then set el's parser
  // document back to parser document and set el's force async to false.</spec>
  if (was_parser_inserted) {
    parser_inserted_ = true;
    force_async_ = false;
  }

  // <spec step="13">Set el's already started to true.</spec>
  already_started_ = true;

  // <spec step="15">If parser document is non-null, and parser document is not
  // equal to el's preparation-time document, then return.</spec>
  if (parser_inserted_ && parser_document_ != &element_->GetDocument()) {
    return nullptr;
  }

  // <spec step="16">If scripting is disabled for el, then return.</spec>
  //
  // <spec href="https://html.spec.whatwg.org/C/#concept-n-noscript">Scripting
  // is disabled for a node when scripting is not enabled, i.e., when its node
  // document's browsing context is null or when scripting is disabled for its
  // relevant settings object.</spec>
  if (!context_window)
    return nullptr;
  if (!context_window->CanExecuteScripts(kAboutToExecuteScript))
    return nullptr;

  // <spec step="17">If el has a nomodule content attribute and its type is
  // "classic", then return.</spec>
  if (BlockForNoModule(GetScriptType(), element_->NomoduleAttributeValue()))
    return nullptr;

  // TODO(csharrison): This logic only works if the tokenizer/parser was not
  // blocked waiting for scripts when the element was inserted. This usually
  // fails for instance, on second document.write if a script writes twice
  // in a row. To fix this, the parser might have to keep track of raw
  // string position.
  //
  // Also PendingScript's contructor has the same code.
  const bool is_in_document_write = element_document.IsInDocumentWrite();

  // Reset line numbering for nested writes.
  TextPosition position = is_in_document_write ? TextPosition::MinimumPosition()
                                               : script_start_position;

  // <spec step="18">If el does not have a src content attribute, and the Should
  // element's inline behavior be blocked by Content Security Policy? algorithm
  // returns "Blocked" when given el, "script", and source text, then return.
  // [CSP]</spec>
  if (!element_->HasSourceAttribute() &&
      !element_->AllowInlineScriptForCSP(element_->GetNonceForElement(),
                                         position.line_, source_text)) {
    return nullptr;
  }

  // Step 19.
  if (!IsScriptForEventSupported())
    return nullptr;

  // 14. is handled below.

  // <spec step="21">Let classic script CORS setting be the current state of
  // el's crossorigin content attribute.</spec>
  CrossOriginAttributeValue cross_origin =
      GetCrossOriginAttributeValue(element_->CrossOriginAttributeValue());

  // <spec step="22">Let module script credentials mode be the CORS settings
  // attribute credentials mode for el's crossorigin content attribute.</spec>
  network::mojom::CredentialsMode credentials_mode =
      ModuleScriptCredentialsMode(cross_origin);

  // <spec step="23">Let cryptographic nonce be el's [[CryptographicNonce]]
  // internal slot's value.</spec>
  String nonce = element_->GetNonceForElement();

  // <spec step="24">If el has an integrity attribute, then let integrity
  // metadata be that attribute's value. Otherwise, let integrity metadata be
  // the empty string.</spec>
  String integrity_attr = element_->IntegrityAttributeValue();
  IntegrityMetadataSet integrity_metadata;
  if (!integrity_attr.empty()) {
    SubresourceIntegrity::IntegrityFeatures integrity_features =
        SubresourceIntegrityHelper::GetFeatures(
            element_->GetExecutionContext());
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attr, integrity_features, integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(*element_->GetExecutionContext(),
                                         report_info);
  }

  // <spec step="25">Let referrer policy be the current state of el's
  // referrerpolicy content attribute.</spec>
  String referrerpolicy_attr = element_->ReferrerPolicyAttributeValue();
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  if (!referrerpolicy_attr.empty()) {
    SecurityPolicy::ReferrerPolicyFromString(
        referrerpolicy_attr, kDoNotSupportReferrerPolicyLegacyKeywords,
        &referrer_policy);
  }

  // <spec href="https://wicg.github.io/priority-hints/#script" step="8">... Let
  // fetchpriority be the current state of the element’s fetchpriority
  // attribute.</spec>
  String fetch_priority_attr = element_->FetchPriorityAttributeValue();
  mojom::blink::FetchPriorityHint fetch_priority_hint =
      GetFetchPriorityAttributeValue(fetch_priority_attr);

  // <spec step="28">Let parser metadata be "parser-inserted" if el is
  // ...</spec>
  ParserDisposition parser_state =
      IsParserInserted() ? kParserInserted : kNotParserInserted;

  if (GetScriptType() == ScriptLoader::ScriptTypeAtPrepare::kModule)
    UseCounter::Count(*context_window, WebFeature::kPrepareModuleScript);
  else if (GetScriptType() == ScriptTypeAtPrepare::kSpeculationRules)
    UseCounter::Count(*context_window, WebFeature::kSpeculationRules);

  DCHECK(!prepared_pending_script_);

  bool potentially_render_blocking = element_->IsPotentiallyRenderBlocking();
  RenderBlockingBehavior render_blocking_behavior =
      potentially_render_blocking ? RenderBlockingBehavior::kBlocking
                                  : RenderBlockingBehavior::kNonBlocking;

  // <spec step="29">Let options be a script fetch options whose cryptographic
  // nonce is cryptographic nonce, integrity metadata is integrity metadata,
  // parser metadata is parser metadata, credentials mode is module script
  // credentials mode, and referrer policy is referrer policy.</spec>
  ScriptFetchOptions options(nonce, integrity_metadata, integrity_attr,
                             parser_state, credentials_mode, referrer_policy,
                             fetch_priority_hint, render_blocking_behavior,
                             RejectCoepUnsafeNone(false));

  // <spec step="30">Let settings object be el's node document's relevant
  // settings object.</spec>
  //
  // In some cases (mainly for classic scripts) |element_document| is used as
  // the "settings object", while in other cases (mainly for module scripts)
  // |content_document| is used.
  // TODO(hiroshige): Use a consistent Document everywhere.
  auto* fetch_client_settings_object_fetcher = context_window->Fetcher();
  ScriptState* script_state =
      ToScriptStateForMainWorld(context_window->GetFrame());

  bool is_eligible_for_delay = false;
  bool is_eligible_for_selective_in_order = false;

  // <spec step="31">If el has a src content attribute, then:</spec>
  if (element_->HasSourceAttribute()) {
    // <spec step="31.1">If el's type is "importmap", then queue an element task
    // on the DOM manipulation task source given el to fire an event named error
    // at el, and return.
    if (GetScriptType() == ScriptTypeAtPrepare::kImportMap) {
      element_document.GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                   WrapPersistent(element_.Get())));
      return nullptr;
    }
    // <spec step="31.2">Let src be the value of el's src attribute.</spec>
    String src =
        StripLeadingAndTrailingHTMLSpaces(element_->SourceAttributeValue());

    // <spec step="31.3">If src is the empty string, then queue a task to fire
    // an event named error at el, and return.</spec>
    if (src.empty()) {
      element_document.GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                   WrapPersistent(element_.Get())));
      return nullptr;
    }

    // <spec step="31.4">Set el's from an external file to true.</spec>
    is_external_script_ = true;

    // <spec step="31.5">Let url be the result of encoding-parsing a URL given
    // src, relative to el's node document.</spec>
    KURL url = element_document.CompleteURL(src);

    // <spec step="31.6">If url is failure, then queue an element task on the
    // DOM manipulation task source given el to fire an event named error at el,
    // and return.</spec>
    if (!url.IsValid()) {
      element_document.GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                   WrapPersistent(element_.Get())));
      return nullptr;
    }

    // TODO(apaseltiner): Propagate the element instead of passing nullptr.
    if (element_->HasAttributionsrcAttribute() &&
        context_window->GetFrame()->GetAttributionSrcLoader()->CanRegister(
            url,
            /*element=*/nullptr,
            /*request_id=*/std::nullopt)) {
      options.SetAttributionReportingEligibility(
          ScriptFetchOptions::AttributionReportingEligibility::kEligible);
    }

    // <spec step="31.7">If el is potentially render-blocking, then block
    // rendering on el.</spec>
    if (potentially_render_blocking &&
        element_document.GetRenderBlockingResourceManager()) {
      element_document.GetRenderBlockingResourceManager()->AddPendingScript(
          *element_);
    }

    // <spec step="31.8">Set el's delaying the load event to true.</spec>
    //
    // <spec step="32.2.B.1">Set el's delaying the load event to true.</spec>
    //
    // When controlled by ScriptRunner, implemented by
    // ScriptRunner::QueueScriptForExecution(). Otherwise (controlled by a
    // parser), then the parser evaluates the script (e.g. parser-blocking,
    // defer, etc.) before DOMContentLoaded, and thus explicit logic for this is
    // not needed.

    // <spec step="31.11">Switch on el's type:</spec>
    switch (GetScriptType()) {
      case ScriptTypeAtPrepare::kInvalid:
      case ScriptTypeAtPrepare::kImportMap:
        NOTREACHED_IN_MIGRATION();
        return nullptr;

      case ScriptTypeAtPrepare::kSpeculationRules:
        // TODO(crbug.com/1182803): Implement external speculation rules.
        element_document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            "External speculation rules are not yet supported."));
        return nullptr;

      case ScriptTypeAtPrepare::kWebBundle:
        element_document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError,
            "External webbundle is not yet supported."));
        element_document.GetTaskRunner(TaskType::kDOMManipulation)
            ->PostTask(FROM_HERE,
                       WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                     WrapPersistent(element_.Get())));
        return nullptr;

      case ScriptTypeAtPrepare::kClassic: {
        // - "classic":

        // <spec step="20">If el has a charset attribute, then let encoding be
        // the result of getting an encoding from the value of the charset
        // attribute. If el does not have a charset attribute, or if getting an
        // encoding failed, then let encoding be el's node document's the
        // encoding.</spec>
        //
        // TODO(hiroshige): Should we handle failure in getting an encoding?
        WTF::TextEncoding encoding;
        if (!element_->CharsetAttributeValue().empty())
          encoding = WTF::TextEncoding(element_->CharsetAttributeValue());
        else
          encoding = element_document.Encoding();

        // <spec step="31.11.A">"classic"
        //
        // Fetch a classic script given url, settings object, options, classic
        // script CORS setting, and encoding.</spec>
        FetchParameters::DeferOption defer = FetchParameters::kNoDefer;
        if (!parser_inserted_ || element_->AsyncAttributeValue() ||
            element_->DeferAttributeValue()) {
          if (!IsEligibleForLowPriorityScriptLoading(element_document,
                                                     *element_, url)) {
            defer = FetchParameters::kLazyLoad;
          } else {
            defer = FetchParameters::kIdleLoad;
          }
        }
        ClassicPendingScript* pending_script = ClassicPendingScript::Fetch(
            url, element_document, options, cross_origin, encoding, element_,
            defer, GetRunningTask(script_state));
        prepared_pending_script_ = pending_script;
        Resource* resource = pending_script->GetResource();
        resource_keep_alive_ = resource;
        is_eligible_for_delay =
            IsEligibleForDelay(*resource, element_document, *element_);
        is_eligible_for_selective_in_order =
            IsEligibleForSelectiveInOrder(*resource, element_document);
        break;
      }
      case ScriptTypeAtPrepare::kModule: {
        // - "module":

        // Step 15 is skipped because they are not used in module
        // scripts.

        // <spec step="31.11.B">"module"
        //
        // Fetch an external module script graph given url, settings object, and
        // options.</spec>
        Modulator* modulator = Modulator::From(script_state);
        if (integrity_attr.IsNull()) {
          // <spec step="31.11.B">If el does not have an integrity attribute,
          // then set options's integrity metadata to the result of resolving a
          // module integrity metadata with url and settings object </spec>
          options.SetIntegrityMetadata(modulator->GetIntegrityMetadata(url));
          options.SetIntegrityAttributeValue(
              modulator->GetIntegrityMetadataString(url));
        }
        FetchModuleScriptTree(url, fetch_client_settings_object_fetcher,
                              modulator, options);
      } break;
    }
  }

  // <spec step="32">If el does not have a src content attribute:</spec>
  if (!element_->HasSourceAttribute()) {
    // <spec step="32.1">Let base URL be el's node document's document base
    // URL.</spec>
    KURL base_url = element_document.BaseURL();

    // Don't report source_url to DevTools for dynamically created module or
    // classic scripts.
    // If we report a source_url here, the dynamic script would look like
    // an inline script of the current document to DevTools, which leads to
    // a confusing debugging experience. The dynamic scripts' source is not
    // present in the document, so stepping in the document as we would for
    // an inline script doesn't make any sense.
    KURL source_url = (!is_in_document_write && parser_inserted_)
                          ? element_document.Url()
                          : KURL();

    // <spec step="32.2">Switch on el's type:</spec>

    switch (GetScriptType()) {
      case ScriptTypeAtPrepare::kInvalid:
        NOTREACHED_IN_MIGRATION();
        return nullptr;

      // <spec step="32.2.C">"importmap"</spec>
      case ScriptTypeAtPrepare::kImportMap: {
        // <spec step="32.2.C.1">If el's relevant global object's import maps
        // allowed is false, then queue an element task on the DOM manipulation
        // task source given el to fire an event named error at el, and
        // return.</spec>
        Modulator* modulator = Modulator::From(script_state);
        auto aquiring_state = modulator->GetAcquiringImportMapsState();
        switch (aquiring_state) {
          case Modulator::AcquiringImportMapsState::kAfterModuleScriptLoad:
          case Modulator::AcquiringImportMapsState::kMultipleImportMaps:
            element_document.AddConsoleMessage(
                MakeGarbageCollected<ConsoleMessage>(
                    mojom::blink::ConsoleMessageSource::kJavaScript,
                    mojom::blink::ConsoleMessageLevel::kError,
                    aquiring_state == Modulator::AcquiringImportMapsState::
                                          kAfterModuleScriptLoad
                        ? "An import map is added after module script load was "
                          "triggered."
                        : "Multiple import maps are not yet supported. "
                          "https://crbug.com/927119"));
            element_document.GetTaskRunner(TaskType::kDOMManipulation)
                ->PostTask(FROM_HERE,
                           WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                         WrapPersistent(element_.Get())));
            return nullptr;

          case Modulator::AcquiringImportMapsState::kAcquiring:
            // <spec step="32.2.C.2">Set el's relevant global object's import
            // maps allowed to false.</spec>
            modulator->SetAcquiringImportMapsState(
                Modulator::AcquiringImportMapsState::kMultipleImportMaps);
            break;
        }
        UseCounter::Count(*context_window, WebFeature::kImportMap);

        // <spec step="32.2.C.3">Let result be the result of creating an import
        // map parse result given source text and base URL.</spec>
        PendingImportMap* pending_import_map =
            PendingImportMap::CreateInline(*element_, source_text, base_url);

        // Because we currently support inline import maps only, the pending
        // import map is ready immediately and thus we call `register an import
        // map` synchronously here.
        //
        // https://html.spec.whatwg.org/C#execute-the-script-element step 6.C
        pending_import_map->RegisterImportMap();

        return nullptr;
      }
      case ScriptTypeAtPrepare::kWebBundle: {
        DCHECK(!script_web_bundle_);

        absl::variant<ScriptWebBundle*, ScriptWebBundleError>
            script_web_bundle_or_error =
                ScriptWebBundle::CreateOrReuseInline(*element_, source_text);
        if (absl::holds_alternative<ScriptWebBundle*>(
                script_web_bundle_or_error)) {
          script_web_bundle_ =
              absl::get<ScriptWebBundle*>(script_web_bundle_or_error);
          DCHECK(script_web_bundle_);
        }
        if (absl::holds_alternative<ScriptWebBundleError>(
                script_web_bundle_or_error)) {
          ScriptWebBundleError error =
              absl::get<ScriptWebBundleError>(script_web_bundle_or_error);
          // Errors with type kSystemError should fire an error event silently
          // for the user, while the other error types should report an
          // exception.
          if (error.GetType() == ScriptWebBundleError::Type::kSystemError) {
            element_->DispatchErrorEvent();
          } else {
            if (script_state->ContextIsValid()) {
              ScriptState::Scope scope(script_state);
              V8ScriptRunner::ReportException(script_state->GetIsolate(),
                                              error.ToV8(script_state));
            }
          }
        }
        return nullptr;
      }

      case ScriptTypeAtPrepare::kSpeculationRules: {
        auto* source = SpeculationRuleSet::Source::FromInlineScript(
            source_text, element_document, element_->GetDOMNodeId());
        AddSpeculationRuleSet(source);
        return nullptr;
      }

        // <spec step="30.2.A">"classic"</spec>
      case ScriptTypeAtPrepare::kClassic: {
        // <spec step="30.2.A.1">Let script be the result of creating a classic
        // script using source text, settings object, base URL, and
        // options.</spec>

        ScriptSourceLocationType script_location_type =
            ScriptSourceLocationType::kInline;
        if (!parser_inserted_) {
          script_location_type =
              ScriptSourceLocationType::kInlineInsideGeneratedElement;
        } else if (is_in_document_write) {
          script_location_type =
              ScriptSourceLocationType::kInlineInsideDocumentWrite;
        }

        prepared_pending_script_ = ClassicPendingScript::CreateInline(
            element_, position, source_url, base_url, source_text,
            script_location_type, options, GetRunningTask(script_state));

        // <spec step="30.2.A.2">Mark as ready el given script.</spec>
        //
        // Implemented by ClassicPendingScript.
        break;
      }

        // <spec step="30.2.B">"module"</spec>
      case ScriptTypeAtPrepare::kModule: {
        // <spec step="30.2.B.2">Fetch an inline module script graph, given
        // source text, base URL, settings object, and options. When this
        // asynchronously completes with result, mark as ready el given
        // result.</spec>
        //
        // <specdef label="fetch-an-inline-module-script-graph"
        // href="https://html.spec.whatwg.org/C/#fetch-an-inline-module-script-graph">

        // Strip any fragment identifiers from the source URL reported to
        // DevTools, so that breakpoints hit reliably for inline module
        // scripts, see crbug.com/1338257 for more details.
        if (source_url.HasFragmentIdentifier())
          source_url.RemoveFragmentIdentifier();
        Modulator* modulator = Modulator::From(script_state);

        // <spec label="fetch-an-inline-module-script-graph" step="1">Let script
        // be the result of creating a JavaScript module script using source
        // text, settings object, base URL, and options.</spec>

        ModuleScriptCreationParams params(
            source_url, base_url, ScriptSourceLocationType::kInline,
            ModuleType::kJavaScript, ParkableString(source_text.Impl()),
            nullptr, network::mojom::ReferrerPolicy::kDefault);
        ModuleScript* module_script =
            JSModuleScript::Create(params, modulator, options, position);

        // <spec label="fetch-an-inline-module-script-graph" step="2">If script
        // is null, asynchronously complete this algorithm with null, and
        // return.</spec>
        if (!module_script)
          return nullptr;

        if (RuntimeEnabledFeatures::RenderBlockingInlineModuleScriptEnabled() &&
            potentially_render_blocking &&
            element_document.GetRenderBlockingResourceManager()) {
          // After https://github.com/whatwg/html/pull/10035:
          // <spec label="fetch-an-inline-module-script-graph" step="3">If el is
          // potentially render-blocking, then block rendering on el and set
          // options's  render-blocking  to true.</spec>
          element_document.GetRenderBlockingResourceManager()->AddPendingScript(
              *element_);
        }

        // <spec label="fetch-an-inline-module-script-graph" step="4">Fetch the
        // descendants of and link script, given settings object, the
        // destination "script", and visited set. When this asynchronously
        // completes with final result, asynchronously complete this algorithm
        // with final result.</spec>
        auto* module_tree_client =
            MakeGarbageCollected<ModulePendingScriptTreeClient>();
        modulator->FetchDescendantsForInlineScript(
            module_script, fetch_client_settings_object_fetcher,
            mojom::blink::RequestContextType::SCRIPT,
            network::mojom::RequestDestination::kScript, module_tree_client);
        prepared_pending_script_ = MakeGarbageCollected<ModulePendingScript>(
            element_, module_tree_client, is_external_script_,
            GetRunningTask(script_state));
        break;
      }
    }
  }

  prepared_pending_script_->SetParserInserted(parser_inserted_);
  prepared_pending_script_->SetIsInDocumentWrite(is_in_document_write);

  ScriptSchedulingType script_scheduling_type = GetScriptSchedulingTypePerSpec(
      element_document, parser_blocking_inline_option);

  // [Intervention, SelectiveInOrderScript, crbug.com/1356396]
  // Check for external script that
  // should be in-order. This simply marks the parser blocking scripts as
  // kInOrder if it's eligible. We use ScriptSchedulingType::kInOrder
  // rather than kForceInOrder here since we don't preserve evaluation order
  // between intervened scripts and ordinary parser-blocking/inline scripts.
  if (is_eligible_for_selective_in_order) {
    switch (script_scheduling_type) {
      case ScriptSchedulingType::kParserBlocking:
        UseCounter::Count(context_window->document()->TopDocument(),
                          WebFeature::kSelectiveInOrderScript);
        if (base::FeatureList::IsEnabled(features::kSelectiveInOrderScript))
          script_scheduling_type = ScriptSchedulingType::kInOrder;
        break;
      default:
        break;
    }
  }

  // [Intervention, ForceInOrderScript, crbug.com/1344772]
  // Check for external script that
  // should be force in-order. Not only the pending scripts that would be marked
  // (without the intervention) as ScriptSchedulingType::kParserBlocking or
  // kInOrder, but also the scripts that would be marked as kAsync are put into
  // the force in-order queue in ScriptRunner because we have to guarantee the
  // execution order of the scripts.
  if (IsEligibleForForceInOrder(element_document)) {
    switch (script_scheduling_type) {
      case ScriptSchedulingType::kAsync:
      case ScriptSchedulingType::kInOrder:
      case ScriptSchedulingType::kParserBlocking:
        script_scheduling_type = ScriptSchedulingType::kForceInOrder;
        break;
      default:
        break;
    }
  }

  // [Intervention, ForceInOrderScript, crbug.com/1344772]
  // If ScriptRunner still has
  // ForceInOrder scripts not executed yet, attempt to mark the inline script as
  // parser blocking so that the inline script is evaluated after the
  // ForceInOrder scripts are evaluated.
  if (script_scheduling_type == ScriptSchedulingType::kImmediate &&
      parser_inserted_ &&
      parser_blocking_inline_option == ParserBlockingInlineOption::kAllow &&
      context_window->document()->GetScriptRunner()->HasForceInOrderScripts()) {
    DCHECK(base::FeatureList::IsEnabled(features::kForceInOrderScript));
    script_scheduling_type = ScriptSchedulingType::kParserBlockingInline;
  }

  // <spec step="31">If el's type is "classic" and el has a src attribute, or
  // el's type is "module":</spec>
  switch (script_scheduling_type) {
    case ScriptSchedulingType::kAsync:
      // <spec step="31.2.1">Let scripts be el's preparation-time document's set
      // of scripts that will execute as soon as possible.</spec>
      //
      // <spec step="31.2.2">Append el to scripts.</spec>
    case ScriptSchedulingType::kInOrder:
      // <spec step="31.3.1">Let scripts be el's preparation-time document's
      // list of scripts that will execute in order as soon as possible.</spec>
      //
      // <spec step="31.3.2">Append el to scripts.</spec>
    case ScriptSchedulingType::kForceInOrder:
      // [intervention, https://crbug.com/1344772] Append el to el's
      // preparation-time document's list of force-in-order scripts.

      {
        // [Intervention, DelayAsyncScriptExecution, crbug.com/1340837]
        // If the target is kCrossSiteWithAllowList or
        // kCrossSiteWithAllowListReportOnly, record the metrics and override
        // is_eligible_for_delay to be always false when
        // kCrossSiteWithAllowListReportOnly.
        if (is_eligible_for_delay &&
            script_scheduling_type == ScriptSchedulingType::kAsync) {
          const features::DelayAsyncScriptTarget delay_async_script_target =
              features::kDelayAsyncScriptTargetParam.Get();
          if (delay_async_script_target ==
              features::DelayAsyncScriptTarget::
                  kCrossSiteWithAllowListReportOnly) {
            is_eligible_for_delay = false;
          }
        }
        // TODO(hiroshige): Here the context document is used as "node document"
        // while Step 14 uses |elementDocument| as "node document". Fix this.
        ScriptRunner* script_runner =
            context_window->document()->GetScriptRunner();
        script_runner->QueueScriptForExecution(
            TakePendingScript(script_scheduling_type),
            DetermineDelayReasonsToWait(script_runner, is_eligible_for_delay));
        // The #mark-as-ready part is implemented in ScriptRunner.
      }

      // [no-spec] Do not keep alive ScriptResource controlled by ScriptRunner
      // after loaded.
      if (resource_keep_alive_) {
        resource_keep_alive_->AddFinishObserver(
            this, element_document.GetTaskRunner(TaskType::kNetworking).get());
      }

      return nullptr;

    case ScriptSchedulingType::kDefer:
    case ScriptSchedulingType::kParserBlocking:
    case ScriptSchedulingType::kParserBlockingInline:
      // The remaining part is implemented by the caller-side of
      // PrepareScript().
      DCHECK(parser_inserted_);
      if (script_scheduling_type ==
          ScriptSchedulingType::kParserBlockingInline) {
        DCHECK_EQ(parser_blocking_inline_option,
                  ParserBlockingInlineOption::kAllow);
      }

      return TakePendingScript(script_scheduling_type);

    case ScriptSchedulingType::kImmediate: {
      // <spec step="32.3">Otherwise, immediately execute the script element el,
      // even if other scripts are already executing.</spec>
      TakePendingScript(ScriptSchedulingType::kImmediate)->ExecuteScriptBlock();
      return nullptr;
    }

    case ScriptSchedulingType::kNotSet:
    case ScriptSchedulingType::kDeprecatedForceDefer:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

ScriptSchedulingType ScriptLoader::GetScriptSchedulingTypePerSpec(
    Document& element_document,
    ParserBlockingInlineOption parser_blocking_inline_option) const {
  DCHECK_NE(GetScriptType(), ScriptLoader::ScriptTypeAtPrepare::kImportMap);
  DCHECK(prepared_pending_script_);

  // <spec step="31">If el's type is "classic" and el has a src attribute, or
  // el's type is "module":</spec>
  if ((GetScriptType() == ScriptTypeAtPrepare::kClassic &&
       element_->HasSourceAttribute()) ||
      GetScriptType() == ScriptTypeAtPrepare::kModule) {
    // <spec step="31.2">If el has an async attribute or el's force async is
    // true:</spec>
    if (element_->AsyncAttributeValue() || force_async_)
      return ScriptSchedulingType::kAsync;

    // <spec step="31.3">Otherwise, if el is not parser-inserted:</spec>
    if (!parser_inserted_)
      return ScriptSchedulingType::kInOrder;

    // <spec step="31.4">Otherwise, if el has a defer attribute or el's type is
    // "module":</spec>
    if (element_->DeferAttributeValue() ||
        GetScriptType() == ScriptTypeAtPrepare::kModule) {
      return ScriptSchedulingType::kDefer;
    }

    // <spec step="31.5">Otherwise:</spec>
    return ScriptSchedulingType::kParserBlocking;
  } else {
    // <spec step="32">Otherwise:</spec>
    DCHECK_EQ(GetScriptType(), ScriptTypeAtPrepare::kClassic);
    DCHECK(!element_->HasSourceAttribute());
    DCHECK(!is_external_script_);

    // <spec step="32.2">If el is parser-inserted, and either the parser that
    // created el is an XML parser or it's an HTML parser whose script nesting
    // level is not greater than one, and el's parser document has a style sheet
    // that is blocking scripts:</spec>
    if (parser_inserted_ &&
        parser_blocking_inline_option == ParserBlockingInlineOption::kAllow &&
        !element_document.IsScriptExecutionReady()) {
      return ScriptSchedulingType::kParserBlockingInline;
    }

    // <spec step="32.3">Otherwise, immediately execute the script element el,
    // even if other scripts are already executing.</spec>
    return ScriptSchedulingType::kImmediate;
  }
}

void ScriptLoader::FetchModuleScriptTree(
    const KURL& url,
    ResourceFetcher* fetch_client_settings_object_fetcher,
    Modulator* modulator,
    const ScriptFetchOptions& options) {
  auto* module_tree_client =
      MakeGarbageCollected<ModulePendingScriptTreeClient>();
  modulator->FetchTree(url, ModuleType::kJavaScript,
                       fetch_client_settings_object_fetcher,
                       mojom::blink::RequestContextType::SCRIPT,
                       network::mojom::RequestDestination::kScript, options,
                       ModuleScriptCustomFetchType::kNone, module_tree_client);
  prepared_pending_script_ = MakeGarbageCollected<ModulePendingScript>(
      element_, module_tree_client, is_external_script_,
      GetRunningTask(modulator->GetScriptState()));
}

PendingScript* ScriptLoader::TakePendingScript(
    ScriptSchedulingType scheduling_type) {
  CHECK(prepared_pending_script_);

  // Record usage histograms per script tag.
  if (element_->GetDocument().Url().ProtocolIsInHTTPFamily()) {
    base::UmaHistogramEnumeration("Blink.Script.SchedulingType",
                                  scheduling_type);
  }

  // Record usage histograms per page.
  switch (scheduling_type) {
    case ScriptSchedulingType::kDefer:
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kScriptSchedulingType_Defer);
      break;
    case ScriptSchedulingType::kParserBlocking:
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kScriptSchedulingType_ParserBlocking);
      break;
    case ScriptSchedulingType::kParserBlockingInline:
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kScriptSchedulingType_ParserBlockingInline);
      break;
    case ScriptSchedulingType::kInOrder:
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kScriptSchedulingType_InOrder);
      break;
    case ScriptSchedulingType::kAsync:
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kScriptSchedulingType_Async);
      break;
    default:
      break;
  }

  PendingScript* pending_script = prepared_pending_script_;
  prepared_pending_script_ = nullptr;
  pending_script->SetSchedulingType(scheduling_type);
  return pending_script;
}

void ScriptLoader::NotifyFinished() {
  // Historically we clear |resource_keep_alive_| when the scheduling type is
  // kAsync or kInOrder (crbug.com/778799). But if the script resource was
  // served via signed exchange, the script may not be in the HTTPCache, and
  // therefore will need to be refetched over network if it's evicted from the
  // memory cache. So we keep |resource_keep_alive_| to keep the resource in the
  // memory cache.
  if (resource_keep_alive_ &&
      !resource_keep_alive_->GetResponse().IsSignedExchangeInnerResponse()) {
    resource_keep_alive_ = nullptr;
  }
}

// <specdef href="https://html.spec.whatwg.org/C/#prepare-the-script-element">
bool ScriptLoader::IsScriptForEventSupported() const {
  // <spec step="19.1">Let for be the value of el's' for attribute.</spec>
  String event_attribute = element_->EventAttributeValue();
  // <spec step="19.2">Let event be the value of el's event attribute.</spec>
  String for_attribute = element_->ForAttributeValue();

  // <spec step="19">If el has an event attribute and a for attribute, and el's
  // type is "classic", then:</spec>
  if (GetScriptType() != ScriptTypeAtPrepare::kClassic ||
      event_attribute.IsNull() || for_attribute.IsNull())
    return true;

  // <spec step="19.3">Strip leading and trailing ASCII whitespace from event
  // and for.</spec>
  for_attribute = for_attribute.StripWhiteSpace();
  // <spec step="19.4">If for is not an ASCII case-insensitive match for the
  // string "window", then return.</spec>
  if (!EqualIgnoringASCIICase(for_attribute, "window"))
    return false;
  event_attribute = event_attribute.StripWhiteSpace();
  // <spec step="19.5">If event is not an ASCII case-insensitive match for
  // either the string "onload" or the string "onload()", then return.</spec>
  return EqualIgnoringASCIICase(event_attribute, "onload") ||
         EqualIgnoringASCIICase(event_attribute, "onload()");
}

String ScriptLoader::GetScriptText() const {
  // Step 3 of
  // https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-prepare-the-script-url-and-text
  // called from § 4.1.3.3, step 4 of
  // https://w3c.github.io/trusted-types/dist/spec/#slot-value-verification
  // This will return the [[ScriptText]] internal slot value after that step,
  // or a null string if the the Trusted Type algorithm threw an error.
  String child_text_content = element_->ChildTextContent();
  DCHECK(!child_text_content.IsNull());
  String script_text_internal_slot = element_->ScriptTextInternalSlot();
  if (child_text_content == script_text_internal_slot)
    return child_text_content;
  return GetStringForScriptExecution(child_text_content,
                                     element_->GetScriptElementType(),
                                     element_->GetExecutionContext());
}

void ScriptLoader::AddSpeculationRuleSet(SpeculationRuleSet::Source* source) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html
  // Let result be the result of parsing speculation rules given source
  // text and base URL.
  // Set the script’s result to result.
  // If the script’s result is not null, append it to the element’s node
  // document's list of speculation rule sets.
  Document& element_document = element_->GetDocument();
  LocalDOMWindow* context_window = element_document.domWindow();
  if (!context_window) {
    return;
  }

  speculation_rule_set_ = SpeculationRuleSet::Parse(source, context_window);
  CHECK(speculation_rule_set_);
  DocumentSpeculationRules::From(element_document)
      .AddRuleSet(speculation_rule_set_);
  speculation_rule_set_->AddConsoleMessageForValidation(*element_);
}

SpeculationRuleSet* ScriptLoader::RemoveSpeculationRuleSet() {
  if (SpeculationRuleSet* rule_set =
          std::exchange(speculation_rule_set_, nullptr)) {
    // Speculation rules in this script no longer apply.
    // Candidate speculations must be re-evaluated.
    DCHECK_EQ(GetScriptType(), ScriptTypeAtPrepare::kSpeculationRules);
    DocumentSpeculationRules::From(element_->GetDocument())
        .RemoveRuleSet(rule_set);
    return rule_set;
  }
  return nullptr;
}

}  // namespace blink
