// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_utils.h"

#import "base/strings/utf_string_conversions.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/web_state.h"
#import "ui/gfx/favicon_size.h"

namespace gemini {

UIImage* GetDefaultFavicon() {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:gfx::kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  return DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
}

GeminiPageContext* CreatePartialPageContextForWebState(web::WebState* web_state,
                                                       bool is_eligible) {
  GeminiPageContext* context = [[GeminiPageContext alloc] init];

  const web::FaviconStatus& favicon_status = web_state->GetFaviconStatus();
  if (favicon_status.valid && !favicon_status.image.IsEmpty()) {
    context.favicon = favicon_status.image.ToUIImage();
  } else {
    context.favicon = GetDefaultFavicon();
  }

  if (!is_eligible) {
    context.geminiPageContextComputationState =
        ios::provider::GeminiPageContextComputationState::kBlocked;
    return context;
  }

  context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kPending;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url(web_state->GetVisibleURL().spec());
  page_context->set_title(base::UTF16ToUTF8(web_state->GetTitle()));
  page_context->mutable_annotated_page_content()->set_tab_id(
      web_state->GetUniqueIdentifier().identifier());
  context.uniquePageContext = std::move(page_context);

  return context;
}

}  // namespace gemini
