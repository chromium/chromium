// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/context_menu/ui_bundled/image_preview_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/context_menu/ui_bundled/constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"

namespace {
// Default time interval to wait before showing a spinner.
constexpr base::TimeDelta kShowSpinnerDelay = base::Seconds(1);
}  // namespace

@interface ImagePreviewViewController ()
@end

@implementation ImagePreviewViewController {
  // The URL of the image to load.
  NSURL* _imageURL;

  // Image transcoder used to convert untrusted image data into safe
  // locally-encoded image data, which can then be safely decoded using UIImage
  // in Chrome process.
  web::JavaScriptImageTranscoder _imageTranscoder;

  // Image view that contains the preview image.
  UIImageView* _imageView;

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

#pragma mark - Private methods.

// Called when safe image data is received from the image data transcoder.
- (void)safeImageDataReceived:(NSData*)safeImageData error:(NSError*)error {
  if (!_webState || _webState->IsBeingDestroyed()) {
    return;
  }
  if (_showSpinnerTimer.IsRunning()) {
    _showSpinnerTimer.Stop();
  }
  _spinnerView.hidden = YES;

  UIImage* image = [UIImage imageWithData:safeImageData];
  base::UmaHistogramBoolean("IOS.ContextMenu.ImagePreviewDisplayed", !error);
  if (!error) {
    _imageView.image = image;
    _imageView.hidden = NO;
    self.preferredContentSize = image.size;
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

// Load the image in the WKWebView and display it.
- (void)showImage {
  if (!self.viewLoaded || !_imageData || !_webState ||
      _webState->IsBeingDestroyed()) {
    return;
  }

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.accessibilityIdentifier =
      kContextMenuImagePreviewAccessibilityIdentifier;
  _imageView.backgroundColor = [UIColor clearColor];
  _imageView.opaque = NO;
  _imageView.hidden = YES;

  [self.view addSubview:_imageView];
  AddSameConstraints(self.view, _imageView);
  __weak __typeof(self) weakSelf = self;
  // Invoke the transcoder which will convert `_imageData` to safe image data
  // and update `_imageView.image` accordingly once safe image data is ready.
  _imageTranscoder.TranscodeImage(
      _imageData, @"image/png", nil, nil, nil,
      base::BindOnce(
          [](ImagePreviewViewController* imagePreviewViewController,
             NSData* safeData, NSError* error) {
            [imagePreviewViewController safeImageDataReceived:safeData
                                                        error:error];
          },
          weakSelf));
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
