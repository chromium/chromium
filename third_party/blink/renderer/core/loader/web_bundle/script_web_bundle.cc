// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle.h"

#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"
#include "components/web_package/web_bundle_utils.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"
#include "third_party/blink/renderer/core/loader/web_bundle/web_bundle_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle_list.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// MicroTask which is used to release a webbundle resource.
class ScriptWebBundle::ReleaseResourceTask {
 public:
  explicit ReleaseResourceTask(ScriptWebBundle& script_web_bundle)
      : script_web_bundle_(&script_web_bundle) {}

  void Run() {
    if (script_web_bundle_->WillBeReleased()) {
      script_web_bundle_->ReleaseBundleLoaderAndUnregister();
    }
  }

 private:
  Persistent<ScriptWebBundle> script_web_bundle_;
};

absl::variant<ScriptWebBundle*, ScriptWebBundleError>
ScriptWebBundle::CreateOrReuseInline(ScriptElementBase& element,
                                     const String& source_text) {
  Document& document = element.GetDocument();
  auto rule_or_error = ScriptWebBundleRule::ParseJson(
      source_text, document.BaseURL(), document.GetExecutionContext());
  if (absl::holds_alternative<ScriptWebBundleError>(rule_or_error))
    return absl::get<ScriptWebBundleError>(rule_or_error);
  auto& rule = absl::get<ScriptWebBundleRule>(rule_or_error);

  ResourceFetcher* resource_fetcher = document.Fetcher();
  if (!resource_fetcher) {
    return ScriptWebBundleError(ScriptWebBundleError::Type::kSystemError,
                                "Missing resource fetcher.");
  }
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();
  if (active_bundles->GetMatchingBundle(rule.source_url())) {
    ExecutionContext* context = document.GetExecutionContext();
    if (context) {
      context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "A nested bundle is not supported: " +
              rule.source_url().ElidedString()));
    }
    return ScriptWebBundleError(ScriptWebBundleError::Type::kSystemError,
                                "A nested bundle is not supported.");
  }

  if (SubresourceWebBundle* found =
          active_bundles->FindSubresourceWebBundleWhichWillBeReleased(
              rule.source_url(), rule.credentials_mode())) {
    // Re-use the ScriptWebBundle if it has the same bundle URL and is being
    // released.
    DCHECK(found->IsScriptWebBundle());
    ScriptWebBundle* reused_script_web_bundle = To<ScriptWebBundle>(found);
    reused_script_web_bundle->ReusedWith(element, std::move(rule));
    return reused_script_web_bundle;
  }
  return MakeGarbageCollected<ScriptWebBundle>(element, document, rule);
}

ScriptWebBundle::ScriptWebBundle(ScriptElementBase& element,
                                 Document& element_document,
                                 const ScriptWebBundleRule& rule)
    : element_(&element), element_document_(&element_document), rule_(rule) {
  UseCounter::Count(element_document_, WebFeature::kScriptWebBundle);
  if (IsSameOriginBundle()) {
    base::UmaHistogramEnumeration(
        "SubresourceWebBundles.OriginType",
        web_package::ScriptWebBundleOriginType::kSameOrigin);
  } else {
    base::UmaHistogramEnumeration(
        "SubresourceWebBundles.OriginType",
        web_package::ScriptWebBundleOriginType::kCrossOrigin);
  }

  CreateBundleLoaderAndRegister();
}

void ScriptWebBundle::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(element_document_);
  visitor->Trace(bundle_loader_);
  SubresourceWebBundle::Trace(visitor);
}

bool ScriptWebBundle::CanHandleRequest(const KURL& url) const {
  if (WillBeReleased())
    return false;
  if (!url.IsValid())
    return false;
  if (!rule_.ResourcesOrScopesMatch(url))
    return false;
  if (url.Protocol() == "uuid-in-package")
    return true;
  DCHECK(bundle_loader_);
  if (!bundle_loader_->GetSecurityOrigin()->IsSameOriginWith(
          SecurityOrigin::Create(url).get())) {
    OnWebBundleError(url.ElidedString() + " cannot be loaded from WebBundle " +
                     bundle_loader_->url().ElidedString() +
                     ": bundled resource must be same origin with the bundle.");
    return false;
  }

  if (!url.GetString().StartsWith(bundle_loader_->url().BaseAsString())) {
    OnWebBundleError(
        url.ElidedString() + " cannot be loaded from WebBundle " +
        bundle_loader_->url().ElidedString() +
        ": bundled resource path must contain the bundle's path as a prefix.");
    return false;
  }
  return true;
}

const KURL& ScriptWebBundle::GetBundleUrl() const {
  return rule_.source_url();
}
const base::UnguessableToken& ScriptWebBundle::WebBundleToken() const {
  return bundle_loader_->WebBundleToken();
}
String ScriptWebBundle::GetCacheIdentifier() const {
  DCHECK(bundle_loader_);
  return bundle_loader_->url().GetString();
}

void ScriptWebBundle::OnWebBundleError(const String& message) const {
  // |element_document_| might not be alive here.
  if (element_document_) {
    ExecutionContext* context = element_document_->GetExecutionContext();
    if (!context)
      return;
    context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
  }
}

// |bundle_loader_| can be null here, if the script element
// is removed from the document and the microtask already
// cleaned up the pointer to the loader.
void ScriptWebBundle::NotifyLoadingFinished() {
  if (!element_ || !bundle_loader_)
    return;
  if (bundle_loader_->HasLoaded()) {
    element_->DispatchLoadEvent();
  } else if (bundle_loader_->HasFailed()) {
    // Save token because DispatchErrorEvent() may remove the script element.
    base::UnguessableToken web_bundle_token = WebBundleToken();
    element_->DispatchErrorEvent();
    if (ResourceFetcher* resource_fetcher = element_document_->Fetcher()) {
      resource_fetcher->CancelWebBundleSubresourceLoadersFor(web_bundle_token);
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

bool ScriptWebBundle::IsScriptWebBundle() const {
  return true;
}

bool ScriptWebBundle::WillBeReleased() const {
  return will_be_released_;
}

network::mojom::CredentialsMode ScriptWebBundle::GetCredentialsMode() const {
  return rule_.credentials_mode();
}

bool ScriptWebBundle::IsSameOriginBundle() const {
  DCHECK(element_document_);
  DCHECK(element_document_->GetFrame());
  DCHECK(element_document_->GetFrame()->GetSecurityContext());
  const SecurityOrigin* frame_security_origin =
      element_document_->GetFrame()->GetSecurityContext()->GetSecurityOrigin();
  auto bundle_origin = SecurityOrigin::Create(rule_.source_url());
  return frame_security_origin &&
         frame_security_origin->IsSameOriginWith(bundle_origin.get());
}

void ScriptWebBundle::CreateBundleLoaderAndRegister() {
  DCHECK(!bundle_loader_);
  DCHECK(element_document_);
  bundle_loader_ = MakeGarbageCollected<WebBundleLoader>(
      *this, *element_document_, rule_.source_url(), rule_.credentials_mode());
  ResourceFetcher* resource_fetcher = element_document_->Fetcher();
  if (!resource_fetcher)
    return;
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();
  active_bundles->Add(*this);
}

void ScriptWebBundle::ReleaseBundleLoaderAndUnregister() {
  if (bundle_loader_) {
    // Clear receivers explicitly here, instead of waiting for Blink GC.
    bundle_loader_->ClearReceivers();
    bundle_loader_ = nullptr;
  }
  // element_document_ might not be alive.
  if (!element_document_)
    return;
  ResourceFetcher* resource_fetcher = element_document_->Fetcher();
  if (!resource_fetcher)
    return;
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();
  active_bundles->Remove(*this);
}

void ScriptWebBundle::WillReleaseBundleLoaderAndUnregister() {
  // We don't release webbundle resources synchronously here. Instead, enqueue a
  // microtask which will release webbundle resources later.

  // The motivation is that we want to update a mapping rule dynamically without
  // releasing webbundle resources.
  //
  // For example, if we remove <script type=webbundle>, and then add another
  // <script type=webbundle> with the same bundle URL, but with a new mapping
  // rule, within the same microtask scope, the new one can re-use the webbundle
  // resources, instead of releasing them. In other words, we don't fetch the
  // same bundle twice.
  //
  // Tentative spec:
  // https://docs.google.com/document/d/1GEJ3wTERGEeTG_4J0QtAwaNXhPTza0tedd00A7vPVsw/edit#heading=h.y88lpjmx2ndn
  will_be_released_ = true;
  element_ = nullptr;
  if (element_document_) {
    auto task = std::make_unique<ReleaseResourceTask>(*this);
    element_document_->GetAgent().event_loop()->EnqueueMicrotask(
        WTF::BindOnce(&ReleaseResourceTask::Run, std::move(task)));
  } else {
    ReleaseBundleLoaderAndUnregister();
  }
}

// This function updates the WebBundleRule, element_ and cancels the release
// of a reused WebBundle. Also if the reused bundle fired load/error events,
// fire them again as we reuse the bundle.
// TODO(crbug/1263783): Explore corner cases of WebBundle reusing and how
// load/error events should be handled then.
void ScriptWebBundle::ReusedWith(ScriptElementBase& element,
                                 ScriptWebBundleRule rule) {
  DCHECK_EQ(element_document_, element.GetDocument());
  DCHECK(will_be_released_);
  DCHECK(!element_);
  rule_ = std::move(rule);
  will_be_released_ = false;
  element_ = element;
  DCHECK(bundle_loader_);
  if (bundle_loader_->HasLoaded()) {
    element_document_->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ScriptElementBase::DispatchLoadEvent,
                                 WrapPersistent(element_.Get())));
  } else if (bundle_loader_->HasFailed()) {
    element_document_->GetTaskRunner(TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ScriptElementBase::DispatchErrorEvent,
                                 WrapPersistent(element_.Get())));
  }
}

}  // namespace blink
