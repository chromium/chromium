// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/link_resource.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
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
  using CompleteURLCallback = base::RepeatingCallback<KURL(const String&)>;

  static bool IsFeatureEnabled(const ExecutionContext*);

  explicit LinkWebBundle(HTMLLinkElement* owner);
  ~LinkWebBundle() override;

  LinkWebBundle(const LinkWebBundle&) = delete;
  LinkWebBundle& operator=(const LinkWebBundle&) = delete;

  void Trace(Visitor* visitor) const override;

  // LinkResource overrides:
  void Process() override;
  LinkResourceType GetType() const override;
  bool HasLoaded() const override;
  void OwnerRemoved() override;

  // SubresourceWebBundle overrides:
  bool CanHandleRequest(const KURL& url) const override;
  String GetCacheIdentifier() const override;
  const KURL& GetBundleUrl() const override;
  const base::UnguessableToken& WebBundleToken() const override;
  void NotifyLoadingFinished() override;
  void OnWebBundleError(const String& message) const override;
  bool IsScriptWebBundle() const override;
  bool WillBeReleased() const override;
  network::mojom::CredentialsMode GetCredentialsMode() const override;

  // Returns a valid absolute URL if |str| can be parsed as a valid
  // absolute URL, or a relative URL with a given |base_url|.
  // Document::CompleteURL(const String&) does the same thing, however we use
  // this function instead, because we do not always have a Document present,
  // like in html_preload_scanner (only document url).
  // TODO(crbug.com/1244483): For now we ignore Encoding of the document here,
  // as we don't have it available in the html_preload_scanner (the only user
  // of this function as of now). Investigate if this is even fixable and how
  // much it actually impacts the construction of the complete URL.
  static KURL CompleteURL(const KURL& base_url, const String& str);

  // Parse the given |str| as a url. If |str| doesn't meet the criteria which
  // WebBundles specification requires, this returns invalid empty KURL as an
  // error. |complete_url_callback| acts as a callable constructor that
  // returns a complete url, accounting for both cases of |str| being a
  // relative URL and absolute one.
  static KURL ParseResourceUrl(const AtomicString& str,
                               CompleteURLCallback complete_url_callback);

 private:
  void AddConsoleMessage(const String& message) const;
  bool ResourcesOrScopesMatch(const KURL& url) const;
  void ReleaseBundleLoader();

  Member<WebBundleLoader> bundle_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_LINK_WEB_BUNDLE_H_
