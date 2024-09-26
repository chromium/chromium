// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/image_preview_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/context_menu/ui_bundled/constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

// Default time interval to wait before showing a spinner.
constexpr base::TimeDelta kShowSpinnerDelay = base::Seconds(1);
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

  // Timer for showing the spinner if loading the image takes
  // too much time.
  base::OneShotTimer _showSpinnerTimer;

  // The view showing a spinner if the loading is too long.
  UIView* _spinnerView;
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
  // Sets the size to the minimum possible size.
  self.preferredContentSize = CGSizeMake(40, 40);

  if (_imageData) {
    [self showImage];
  } else {
    __weak __typeof(self) weakSelf = self;
    _showSpinnerTimer.Start(FROM_HERE, kShowSpinnerDelay, base::BindOnce(^{
                              [weakSelf presentSpinner];
                            }));
  }
}

#pragma mark - WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  if (!_webState || _webState->IsBeingDestroyed()) {
    return;
  }
  if (_showSpinnerTimer.IsRunning()) {
    _showSpinnerTimer.Stop();
  }
  _spinnerView.hidden = YES;

  auto imageSize = [self parseResponse:message];
  base::UmaHistogramBoolean("IOS.ContextMenu.ImagePreviewDisplayed",
                            imageSize.has_value());
  if (imageSize.has_value()) {
    _webView.hidden = NO;
    self.preferredContentSize = imageSize.value();
    return;
  }

  // If there is no value, image loading failed. Show the error view.
  UIView* errorView = [[UIView alloc] init];
  [self.view addSubview:errorView];
  errorView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, errorView);
  errorView.backgroundColor = [UIColor colorNamed:kWhiteBlackAlpha50Color];

  UIImageView* errorImageView = [[UIImageView alloc] init];
  [errorView addSubview:errorImageView];
  errorImageView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameCenterConstraints(errorView, errorImageView);
  [errorImageView setImage:DefaultSymbolWithPointSize(kPhotoSymbol, 17)];
  errorImageView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
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
  _webView.backgroundColor = [UIColor clearColor];
  _webView.scrollView.backgroundColor = [UIColor clearColor];
  _webView.opaque = false;
  _webView.hidden = YES;

  [self.view addSubview:_webView];
  AddSameConstraints(self.view, _webView);
  [_webView.configuration.userContentController
      addScriptMessageHandler:self
                         name:@"imageLoaded"];
  [_webView loadHTMLString:htmlString
                   baseURL:[NSURL URLWithString:@"about:blank"]];
}

// Parses the JS message and return the size of the image if `message` is
// correctly formed.
- (std::optional<CGSize>)parseResponse:(WKScriptMessage*)message {
  if (![message.body isKindOfClass:[NSDictionary class]]) {
    return std::nullopt;
  }
  NSDictionary* body = message.body;
  if (![body[@"success"] isKindOfClass:[NSNumber class]]) {
    return std::nullopt;
  }
  NSNumber* success = body[@"success"];
  if (!success.boolValue) {
    return std::nullopt;
  }
  if (![body[@"height"] isKindOfClass:[NSNumber class]] ||
      ![body[@"width"] isKindOfClass:[NSNumber class]]) {
    return std::nullopt;
  }
  CGFloat height = base::apple::ObjCCast<NSNumber>(body[@"height"]).doubleValue;
  CGFloat width = base::apple::ObjCCast<NSNumber>(body[@"width"]).doubleValue;

  if (isnan(height) || isnan(width) || height <= 0 || width <= 0) {
    return std::nullopt;
  }
  return CGSizeMake(width, height);
}

// Called when image data is received.
- (void)imageDataReceived:(NSData*)data {
  _imageData = data;
  [self showImage];
}

// Called if the loading is too long. A spinner is presented.
- (void)presentSpinner {
  _spinnerView = [[UIView alloc] init];
  [self.view addSubview:_spinnerView];
  _spinnerView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, _spinnerView);
  _spinnerView.backgroundColor = [UIColor colorNamed:kWhiteBlackAlpha50Color];

  UIActivityIndicatorView* spinnerIndicatorView =
      [[UIActivityIndicatorView alloc] init];
  spinnerIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  spinnerIndicatorView.color = [UIColor colorNamed:kTextSecondaryColor];
  [_spinnerView addSubview:spinnerIndicatorView];
  AddSameConstraints(_spinnerView, spinnerIndicatorView);
  [spinnerIndicatorView startAnimating];
}

@end
