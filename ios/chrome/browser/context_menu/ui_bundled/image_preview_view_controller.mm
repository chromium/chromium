// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/image_preview_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/context_menu/ui_bundled/constants.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

namespace {
const char kLoadImageHTML[] = R"(
<html>
  <head>
    <meta name="viewport" content="width=device-width, shrink-to-fit=YES">
  </head>
  <body style='margin: 0px;' width='100%' height='100%'>
    <img id='image' width='100%' height='100%'/>
    <script>
      var imageElement = document.getElementById('image');
      function imageLoaded() {
        window.webkit.messageHandlers.imageLoaded.postMessage({
              'success':true,
              'width': imageElement.naturalWidth,
              'height': imageElement.naturalHeight
        });
      }
      function imageLoadedError() {
        window.webkit.messageHandlers.imageLoaded.postMessage({
              'success':false
        });
      }
      imageElement.addEventListener('load', imageLoaded);
      imageElement.addEventListener('error', imageLoadedError);
      imageElement.src = 'data:;base64,IMAGE_DATA_PLACEHOLDER';
    </script>
  </body>
</html>
)";
}

@interface ImagePreviewViewController () <WKScriptMessageHandler>
@end

@implementation ImagePreviewViewController {
  // The URL of the image to load.
  NSURL* _imageURL;

  // A WKWebView to load and display the image. Do not use a webState as the
  // only purpose of the webView is to display a data image.
  WKWebView* _webView;

  // The image data. This is not considered trusted data, so do not process
  // it in Chrome process.
  NSData* _imageData;

  // The WebState that triggered the menu.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithSrcURL:(NSURL*)URL webState:(web::WebState*)webState {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _imageURL = [URL copy];
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (void)loadPreview {
  ImageFetchTabHelper* imageFetcher =
      ImageFetchTabHelper::FromWebState(_webState.get());
  const GURL& lastCommittedURL = _webState->GetLastCommittedURL();
  web::Referrer referrer(lastCommittedURL, web::ReferrerPolicyDefault);

  __weak __typeof(self) weakSelf = self;
  imageFetcher->GetImageData(net::GURLWithNSURL(_imageURL), referrer,
                             ^(NSData* data) {
                               [weakSelf imageDataReceived:data];
                             });
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  if (!_webState || _webState->IsBeingDestroyed()) {
    return;
  }

  self.view.backgroundColor = [UIColor clearColor];
  // Sets the size to 1,1 as the minimum possible size.
  self.preferredContentSize = CGSizeMake(1, 1);
  [self showImage];
}

- (void)imageDataReceived:(NSData*)data {
  _imageData = data;
  [self showImage];
}

#pragma mark - WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  if (!_webState || _webState->IsBeingDestroyed()) {
    return;
  }
  if (![message.body isKindOfClass:[NSDictionary class]]) {
    return;
  }
  NSDictionary* body = message.body;
  if (![body[@"success"] isKindOfClass:[NSNumber class]]) {
    return;
  }
  NSNumber* success = body[@"success"];
  if (!success.boolValue) {
    return;
  }
  if (![body[@"height"] isKindOfClass:[NSNumber class]] ||
      ![body[@"width"] isKindOfClass:[NSNumber class]]) {
    return;
  }
  NSNumber* height = body[@"height"];
  NSNumber* width = body[@"width"];
  self.preferredContentSize = CGSizeMake(width.doubleValue, height.doubleValue);
}

#pragma mark - Private methods.

// Load the image in the WKWebView and display it.
- (void)showImage {
  if (!self.viewLoaded || !_imageData || !_webState ||
      _webState->IsBeingDestroyed()) {
    return;
  }
  NSString* htmlString = [base::SysUTF8ToNSString(kLoadImageHTML)
      stringByReplacingOccurrencesOfString:@"IMAGE_DATA_PLACEHOLDER"
                                withString:
                                    [_imageData
                                        base64EncodedStringWithOptions:0]];
  _webView = [[WKWebView alloc] init];
  _webView.userInteractionEnabled = NO;
  _webView.configuration.ignoresViewportScaleLimits = YES;
  _webView.contentMode = UIViewContentModeScaleToFill;
  _webView.translatesAutoresizingMaskIntoConstraints = NO;
  _webView.accessibilityIdentifier =
      kContextMenuImagePreviewAccessibilityIdentifier;

  [self.view addSubview:_webView];
  AddSameConstraints(self.view, _webView);
  [_webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"imageLoaded"];
  [_webView loadHTMLString:htmlString
                   baseURL:[NSURL URLWithString:@"about:blank"]];
}

@end
