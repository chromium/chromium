// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_web_view_helper.h"

#include <set>

#include "base/no_destructor.h"

namespace extensions {

namespace {

using ExtensionWebViewHelperSet = std::set<const ExtensionWebViewHelper*>;

ExtensionWebViewHelperSet* GetWebViewHelperSet() {
  static base::NoDestructor<ExtensionWebViewHelperSet> web_view_helpers;
  return web_view_helpers.get();
}

}  // namespace

ExtensionWebViewHelper::ExtensionWebViewHelper(
    blink::WebView* web_view,
    const url::Origin* outermost_origin)
    : blink::WebViewObserver(web_view) {
  if (outermost_origin)
    outermost_origin_ = *outermost_origin;
  GetWebViewHelperSet()->insert(this);
}

ExtensionWebViewHelper::~ExtensionWebViewHelper() {
  GetWebViewHelperSet()->erase(this);
}

const std::optional<url::Origin>& ExtensionWebViewHelper::GetOutermostOrigin()
    const {
  return outermost_origin_;
}

// static
const ExtensionWebViewHelper* ExtensionWebViewHelper::Get(
    blink::WebView* web_view) {
  for (const ExtensionWebViewHelper* target : *GetWebViewHelperSet()) {
    if (target->GetWebView() == web_view)
      return target;
  }
  return nullptr;
}

void ExtensionWebViewHelper::OnDestruct() {
  delete this;
}

}  // namespace extensions
