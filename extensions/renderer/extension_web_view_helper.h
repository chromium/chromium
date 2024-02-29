// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_WEB_VIEW_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_WEB_VIEW_HELPER_H_

#include <optional>

#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_observer.h"
#include "url/origin.h"

namespace extensions {

// WebView-level plumbing for extension features.
class ExtensionWebViewHelper : public blink::WebViewObserver {
 public:
  ExtensionWebViewHelper(blink::WebView* web_view,
                         const url::Origin* outermost_origin);

  ExtensionWebViewHelper(const ExtensionWebViewHelper&) = delete;
  ExtensionWebViewHelper& operator=(const ExtensionWebViewHelper&) = delete;

  ~ExtensionWebViewHelper() override;

  const std::optional<url::Origin>& GetOutermostOrigin() const;

  static const ExtensionWebViewHelper* Get(blink::WebView* web_view);

  // blink::WebViewObserver overrides:
  void OnDestruct() override;

 private:
  std::optional<url::Origin> outermost_origin_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_WEB_VIEW_HELPER_H_
