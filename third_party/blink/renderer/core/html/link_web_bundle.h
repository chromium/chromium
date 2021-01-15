// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace base {
class UnguessableToken;
}

namespace blink {

class WebBundleLoader;

// LinkWebBundle is used in the Subresource loading with Web Bundles feature.
// See crbug.com/1082020 for details.
// A <link rel="webbundle" ...> element creates LinkWebBundle.
class CORE_EXPORT LinkWebBundle final : public LinkResource,
                                        public SubresourceWebBundle {
 public:
  explicit LinkWebBundle(HTMLLinkElement* owner);
  ~LinkWebBundle() override;

  LinkWebBundle(const LinkWebBundle&) = delete;
  LinkWebBundle& operator=(const LinkWebBundle&) = delete;

  void Trace(Visitor* visitor) const override;

  void NotifyLoaded();
  void OnWebBundleError(const String& message) const;

  // LinkResource overrides:
  void Process() override;
  LinkResourceType GetType() const override;
  bool HasLoaded() const override;
  void OwnerRemoved() override;

  // SubresourceWebBundle overrides:
  bool CanHandleRequest(const KURL& url) const override;
  String GetCacheIdentifier() const override;
  const base::UnguessableToken& WebBundleToken() const override;

  // Parse the given |str| as a url. If |str| doesn't meet the criteria which
  // WebBundles specification requires, this returns invalid empty KURL as an
  // error.
  // See
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#name-parsing-the-index-section
  static KURL ParseResourceUrl(const AtomicString& str);

 private:
  bool ResourcesOrScopesMatch(const KURL& url) const;

  Member<WebBundleLoader> bundle_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_
