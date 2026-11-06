// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/util/wk_web_view_util.h"

namespace web {

namespace {

// Returns whether `document_rect` is non-null, finite, and has a width and
// height of at least one CSS pixel. WebKit truncates the rect to integers and
// never completes the request when the truncated size is empty.
bool IsValidPdfDocumentRect(CGRect document_rect) {
  return !CGRectIsNull(document_rect) && !CGRectIsInfinite(document_rect) &&
         document_rect.size.width >= 1 && document_rect.size.height >= 1;
}

}  // namespace

void CreatePdf(WKWebView* web_view,
               std::optional<CGRect> document_rect,
               base::OnceCallback<void(NSData*)> callback) {
  const bool has_invalid_rect =
      document_rect.has_value() && !IsValidPdfDocumentRect(*document_rect);
  if (!web_view || has_invalid_rect) {
    std::move(callback).Run(nil);
    return;
  }

  WKPDFConfiguration* pdf_configuration = [[WKPDFConfiguration alloc] init];
  if (document_rect.has_value()) {
    // WebKit's `WebPage::drawToPDF` paints `WKPDFConfiguration.rect` unscaled
    // in document coordinates (despite Apple's header describing it as view
    // coordinates).
    pdf_configuration.rect = *document_rect;
  }

  __block base::OnceCallback<void(NSData*)> callback_for_block =
      std::move(callback);
  [web_view
      createPDFWithConfiguration:pdf_configuration
               completionHandler:^(NSData* pdf_document_data, NSError* error) {
                 std::move(callback_for_block).Run(pdf_document_data);
               }];
}

}  // namespace web
