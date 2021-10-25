// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WEB_BUNDLE_SCRIPT_WEB_BUNDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WEB_BUNDLE_SCRIPT_WEB_BUNDLE_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace WTF {
class String;
}

namespace blink {

class Document;
class KURL;
class WebBundleLoader;

// ScriptLoader creates this for a script whose type is "webbundle".
//
// https://github.com/WICG/webpackage/blob/main/explainers/subresource-loading.md#script-based-api
class CORE_EXPORT ScriptWebBundle final
    : public GarbageCollected<ScriptWebBundle>,
      public SubresourceWebBundle {
 public:
  static ScriptWebBundle* CreateOrReuseInline(Document& element_document,
                                              const String& inline_text);

  ScriptWebBundle(Document& element_document, const ScriptWebBundleRule& rule);

  void Trace(Visitor* visitor) const override;

  // SubresourceWebBundle overrides:
  bool CanHandleRequest(const KURL& url) const override;
  String GetCacheIdentifier() const override;
  const KURL& GetBundleUrl() const override;
  const base::UnguessableToken& WebBundleToken() const override;
  void NotifyLoaded() override;
  void OnWebBundleError(const String& message) const override;
  bool IsScriptWebBundle() const override;
  bool WillBeReleased() const override;
  network::mojom::CredentialsMode GetCredentialsMode() const override;

  void CreateBundleLoaderAndRegister();
  void ReleaseBundleLoaderAndUnregister();

  void WillReleaseBundleLoaderAndUnregister();
  void CancelRelease() { will_be_released_ = false; }

  void SetRule(ScriptWebBundleRule rule) { rule_ = std::move(rule); }

  class ReleaseResourceTask;

 private:
  bool will_be_released_ = false;
  WeakMember<Document> element_document_;
  ScriptWebBundleRule rule_;
  Member<WebBundleLoader> bundle_loader_;
};

template <>
struct DowncastTraits<ScriptWebBundle> {
  static bool AllowFrom(const SubresourceWebBundle& subresource_web_bundle) {
    return subresource_web_bundle.IsScriptWebBundle();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WEB_BUNDLE_SCRIPT_WEB_BUNDLE_H_
