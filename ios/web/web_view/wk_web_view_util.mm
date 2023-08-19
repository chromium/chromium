// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#import "ios/web/public/web_client.h"

namespace web {

void CreateFullPagePdf(WKWebView* web_view,
                       base::OnceCallback<void(NSData*)> callback) {

  if (!web_view) {
    std::move(callback).Run(nil);
    return;
  }

  __block base::OnceCallback<void(NSData*)> callback_for_block =
      std::move(callback);
  WKPDFConfiguration* pdf_configuration = [[WKPDFConfiguration alloc] init];
  [web_view
      createPDFWithConfiguration:pdf_configuration
               completionHandler:^(NSData* pdf_document_data, NSError* error) {
                 std::move(callback_for_block).Run(pdf_document_data);
               }];
}
}  // namespace web
