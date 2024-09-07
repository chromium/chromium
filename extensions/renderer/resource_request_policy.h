// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RESOURCE_REQUEST_POLICY_H_
#define EXTENSIONS_RENDERER_RESOURCE_REQUEST_POLICY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension_guid.h"
#include "extensions/common/extension_id.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace extensions {
class Dispatcher;
class Extension;

// Encapsulates the policy for when chrome-extension:// URLs can be requested.
class ResourceRequestPolicy {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true if `origin` is a special origin from which requests should
    // always be allowed.
    virtual bool ShouldAlwaysAllowRequestForFrameOrigin(
        const url::Origin& frame_origin) = 0;

    // Returns true if a page with the given `page_origin` should be allowed to
    // load the resource at `target_url` because it is a devtools page.
    virtual bool AllowLoadForDevToolsPage(const GURL& page_origin,
                                          const GURL& target_url) = 0;
  };

  ResourceRequestPolicy(Dispatcher* dispatcher,
                        std::unique_ptr<Delegate> delegate);

  ResourceRequestPolicy(const ResourceRequestPolicy&) = delete;
  ResourceRequestPolicy& operator=(const ResourceRequestPolicy&) = delete;

  ~ResourceRequestPolicy();

  void OnExtensionLoaded(const Extension& extension);
  void OnExtensionUnloaded(const ExtensionId& extension);

  // Returns true if the chrome-extension:// `target_url` can be requested
  // from `upstream_url`. In some cases this decision is made based upon how
  // this request was generated. Web triggered transitions are more restrictive
  // than those triggered through UI.
  bool CanRequestResource(const GURL& upstream_url,
                          const GURL& target_url,
                          blink::WebLocalFrame* frame,
                          ui::PageTransition transition_type,
                          const url::Origin* initiator_origin);

 private:
  // Determine if the host is web accessible.
  bool IsWebAccessibleHost(const std::string& host);

  raw_ptr<Dispatcher> dispatcher_;

  std::unique_ptr<Delegate> delegate_;

  // 1:1 mapping of extension IDs with any potentially web- or webview-
  // accessible resources to their corresponding GUIDs.
  using WebAccessibleHostMap = std::map<ExtensionId, ExtensionGuid>;
  WebAccessibleHostMap web_accessible_resources_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RESOURCE_REQUEST_POLICY_H_
