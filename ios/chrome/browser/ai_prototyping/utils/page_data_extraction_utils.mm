// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/page_data_extraction_utils.h"

#import <memory>

#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state.h"

namespace page_data_extraction_utils {

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

optimization_guide::proto::PageContext* GetPageContextForWebState(
    web::WebState* web_state) {
  std::unique_ptr<optimization_guide::proto::PageContext> page_context(
      new optimization_guide::proto::PageContext());

  // TODO(crbug.com/379911543): Add missing PageContext fields (ax_tree_data,
  // inner_text, inner_text_offset, screenshot_data and pdf_data).
  page_context->set_url(web_state->GetVisibleURL().spec());
  page_context->set_title(base::UTF16ToUTF8(web_state->GetTitle()));

  return page_context.release();
}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

}  // namespace page_data_extraction_utils
