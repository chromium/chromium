// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"

#import "ios/web/public/web_state.h"
#import "ios/web/util/content_type_util.h"
#import "url/gurl.h"

bool CanExtractPageContextForWebState(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }

  const GURL& url = web_state->GetVisibleURL();
  const std::string mime_type = web_state->GetContentsMimeType();
  bool mime_type_ok =
      web::IsContentTypeHtml(mime_type) || web::IsContentTypeImage(mime_type);

  return url.SchemeIsHTTPOrHTTPS() && mime_type_ok;
}
