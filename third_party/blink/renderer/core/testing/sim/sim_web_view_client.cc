// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_web_view_client.h"

#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {

SimWebViewClient::SimWebViewClient(content::LayerTreeViewDelegate* delegate)
    : frame_test_helpers::TestWebViewClient(delegate) {}

void SimWebViewClient::DidMeaningfulLayout(
    WebMeaningfulLayout meaningful_layout) {
  switch (meaningful_layout) {
    case WebMeaningfulLayout::kVisuallyNonEmpty:
      visually_non_empty_layout_count_++;
      break;
    case WebMeaningfulLayout::kFinishedParsing:
      finished_parsing_layout_count_++;
      break;
    case WebMeaningfulLayout::kFinishedLoading:
      finished_loading_layout_count_++;
      break;
  }
}

WebView* SimWebViewClient::CreateView(WebLocalFrame* opener,
                                      const WebURLRequest&,
                                      const WebWindowFeatures&,
                                      const WebString& name,
                                      WebNavigationPolicy,
                                      bool,
                                      WebSandboxFlags,
                                      const SessionStorageNamespaceId&) {
  return web_view_helper_.InitializeWithOpener(opener);
}

}  // namespace blink
