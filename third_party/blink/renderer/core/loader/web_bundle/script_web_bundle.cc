// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"
#include "third_party/blink/renderer/core/loader/web_bundle/web_bundle_loader.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle_list.h"
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

ScriptWebBundle* ScriptWebBundle::CreateOrReuseInline(
    Document& element_document,
    const String& source_text) {
  auto rule =
      ScriptWebBundleRule::ParseJson(source_text, element_document.BaseURL());
  if (!rule)
    return nullptr;

  ResourceFetcher* resource_fetcher = element_document.Fetcher();
  if (!resource_fetcher)
    return nullptr;
  SubresourceWebBundleList* active_bundles =
      resource_fetcher->GetOrCreateSubresourceWebBundleList();

  if (SubresourceWebBundle* found =
          active_bundles->FindSubresourceWebBundleWhichWillBeReleased(
              rule->source_url(), rule->credentials_mode())) {
    // Re-use the ScriptWebBundle if it has the same bundle URL and is being
    // released.
    DCHECK(found->IsScriptWebBundle());
    ScriptWebBundle* reused_script_web_bundle = To<ScriptWebBundle>(found);
    DCHECK_EQ(reused_script_web_bundle->element_document_, element_document);
    reused_script_web_bundle->SetRule(std::move(*rule));
    reused_script_web_bundle->CancelRelease();
    return reused_script_web_bundle;
  }
  return MakeGarbageCollected<ScriptWebBundle>(element_document, *rule);
}

ScriptWebBundle::ScriptWebBundle(Document& element_document,
                                 const ScriptWebBundleRule& rule)
    : element_document_(&element_document), rule_(rule) {
  CreateBundleLoaderAndRegister();
}

void ScriptWebBundle::Trace(Visitor* visitor) const {
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
  if (url.Protocol() == "urn" || url.Protocol() == "uuid-in-package")
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

// TODO(crbug.com/1245166): Implement these.
void ScriptWebBundle::OnWebBundleError(const String& message) const {}
void ScriptWebBundle::NotifyLoaded() {}

bool ScriptWebBundle::IsScriptWebBundle() const {
  return true;
}

bool ScriptWebBundle::WillBeReleased() const {
  return will_be_released_;
}

network::mojom::CredentialsMode ScriptWebBundle::GetCredentialsMode() const {
  return rule_.credentials_mode();
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
  auto task = std::make_unique<ReleaseResourceTask>(*this);
  Microtask::EnqueueMicrotask(
      WTF::Bind(&ReleaseResourceTask::Run, std::move(task)));
}

}  // namespace blink
