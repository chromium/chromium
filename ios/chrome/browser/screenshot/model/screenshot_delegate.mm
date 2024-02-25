// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screenshot/model/screenshot_delegate.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

@implementation ScreenshotDelegate {
  id<BrowserProviderInterface> _browserProviderInterface;
}

- (instancetype)initWithBrowserProviderInterface:
    (id<BrowserProviderInterface>)browserProviderInterface {
  self = [super init];
  if (self) {
    _browserProviderInterface = browserProviderInterface;
  }
  return self;
}

#pragma mark - UIScreenshotServiceDelegate

// When there are multiple windows in the foreground UIKit will ask each
// UIScreenshotServiceDelegate for the PDF data and will display the PDF data of
// the widest window in the foreground.
- (void)screenshotService:(UIScreenshotService*)screenshotService
    generatePDFRepresentationWithCompletion:
        (void (^)(NSData*, NSInteger, CGRect))completionHandler {
  Browser* browser = _browserProviderInterface.currentBrowserProvider.browser;

  if (!browser) {
    completionHandler(nil, 0, CGRectZero);
    return;
  }

  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();

  if (!webState) {
    completionHandler(nil, 0, CGRectZero);
    return;
  }

  // Pass the currently viewed frame to maintain scroll position in the
  // screenshot editing tool.
  id<CRWWebViewProxy> webProxy = webState->GetWebViewProxy();
  CRWWebViewScrollViewProxy* scrollProxy = webProxy.scrollViewProxy;
  CGPoint contentOffset = scrollProxy.contentOffset;
  CGSize contentSize = scrollProxy.contentSize;
  CGRect webViewFrame = scrollProxy.frame;
  webViewFrame.origin.x = contentOffset.x;
  webViewFrame.origin.y =
      contentSize.height - webViewFrame.size.height - contentOffset.y;

  base::OnceCallback<void(NSData*)> callback =
      base::BindOnce(^(NSData* pdfDoumentData) {
        completionHandler(pdfDoumentData, 0, webViewFrame);
      });

  webState->CreateFullPagePdf(std::move(callback));
}

@end
