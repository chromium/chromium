// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/legacy_snapshot_generator.h"

#import "base/debug/crash_logging.h"
#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "build/blink_buildflags.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

namespace {

// Contains information needed for snapshotting.
struct SnapshotInfo {
  UIView* baseView;
  CGRect snapshotFrameInBaseView;
  CGRect snapshotFrameInWindow;
};

}  // namespace

@implementation LegacySnapshotGenerator {
  // The associated WebState.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    DCHECK(webState);
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (void)generateSnapshotWithCompletion:(void (^)(UIImage*))completion {
  bool showing_native_content =
      web::GetWebClient()->IsAppSpecificURL(_webState->GetLastCommittedURL());
  if (!showing_native_content && _webState->CanTakeSnapshot()) {
    // Take the snapshot using the optimized WKWebView snapshotting API for
    // pages loaded in the web view when the WebState snapshot API is available.
    [self generateWKWebViewSnapshotWithCompletion:completion];
    return;
  }
  // Use the UIKit-based snapshot API as a fallback when the WKWebView API is
  // unavailable.
  UIImage* snapshot = [self generateUIViewSnapshotWithOverlays];
  if (completion) {
    completion(snapshot);
  }
}

- (UIImage*)generateUIViewSnapshot {
  if (![self canTakeSnapshot] || !_webState) {
    return nil;
  }
  [_delegate
      willUpdateSnapshotWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                             initWithWebState:_webState.get()]];

  std::optional<SnapshotInfo> snapshotInfo = [self snapshotInfo];
  if (!snapshotInfo) {
    return nil;
  }
  // Ideally, generate an UIImage by one step with `UIGraphicsImageRenderer`,
  // however, it generates a black image when the size of `baseView` is larger
  // than `frameInBaseView`. So this is a workaround to generate an UIImage by
  // dividing the step into 2 steps; 1) convert an UIView to an UIImage 2) crop
  // an UIImage with `frameInBaseView`.
  UIImage* baseImage = [self convertFromBaseView:snapshotInfo.value().baseView];
  return [self cropImage:baseImage
         frameInBaseView:snapshotInfo.value().snapshotFrameInBaseView];
}

- (UIImage*)generateUIViewSnapshotWithOverlays {
  if (![self canTakeSnapshot]) {
    return nil;
  }
  std::optional<SnapshotInfo> snapshotInfo = [self snapshotInfo];
  if (!snapshotInfo) {
    return nil;
  }
  return [self addOverlays:[self overlays]
                 baseImage:[self generateUIViewSnapshot]
             frameInWindow:snapshotInfo.value().snapshotFrameInWindow];
}

#pragma mark - Private methods

// Asynchronously generates a new snapshot with WebKit-based snapshot API and
// runs a callback with the new snapshot image. It is an error to call this
// method if the web state is showing anything other (e.g., native content) than
// a web view.
- (void)generateWKWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion {
  if (!_webState) {
    return;
  }
  DCHECK(
      !web::GetWebClient()->IsAppSpecificURL(_webState->GetLastCommittedURL()));

  if (![self canTakeSnapshot]) {
    if (completion) {
      // Post a task to the current thread (UI thread).
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(completion, nil));
    }
    return;
  }
  [_delegate
      willUpdateSnapshotWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                             initWithWebState:_webState.get()]];

  std::optional<SnapshotInfo> snapshotInfo = [self snapshotInfo];
  if (!snapshotInfo) {
    return;
  }
  auto wrappedCompletion =
      ^(__weak LegacySnapshotGenerator* generator, UIImage* image) {
        if (!generator) {
          completion(nil);
        }
        UIImage* snapshot =
            [generator addOverlays:[generator overlays]
                         baseImage:image
                     frameInWindow:snapshotInfo.value().snapshotFrameInWindow];
        if (completion) {
          completion(snapshot);
        }
      };

  __weak LegacySnapshotGenerator* weakSelf = self;
  _webState->TakeSnapshot(snapshotInfo.value().snapshotFrameInBaseView,
                          base::BindRepeating(wrappedCompletion, weakSelf));
}


// Returns NO if WebState or the view is not ready for snapshot.
- (BOOL)canTakeSnapshot {
  // This allows for easier unit testing of classes that use SnapshotGenerator.
  if (!_delegate || !_webState) {
    return NO;
  }

  // Do not generate a snapshot if web usage is disabled (as the WebState's
  // view is blank in that case).
  if (!_webState->IsWebUsageEnabled()) {
    return NO;
  }

  return [_delegate
      canTakeSnapshotWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                          initWithWebState:_webState.get()]];
}

// Converts an UIView to an UIImage. The size of generated UIImage is the same
// as `baseView`.
- (UIImage*)convertFromBaseView:(UIView*)baseView {
  DCHECK(baseView);

  // Disable the automatic view dimming UIKit performs if a view is presented
  // modally over `baseView`.
  baseView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;

  // Note: When not using device scale, the output image size may slightly
  // differ from the input size due to rounding.
  const CGFloat kScale = [SnapshotImageScale floatImageScaleForDevice];
  DCHECK_GE(kScale, 1.0);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = kScale;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithBounds:baseView.bounds
                                               format:format];

  __block BOOL snapshotSuccess = YES;
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
          // Render the view's layer via `-renderInContext:`.
          // To mitigate against crashes like crbug.com/1429512, ensure that
          // the layer's position is valid. If not, mark the snapshotting as
          // failed.
          CALayer* layer = baseView.layer;
          CGPoint pos = layer.position;
          if (isnan(pos.x) || isnan(pos.y)) {
            snapshotSuccess = NO;
          } else {
            [layer renderInContext:UIContext.CGContext];
          }
      }];

  if (!snapshotSuccess) {
    image = nil;
  }

  // Set the mode to UIViewTintAdjustmentModeAutomatic.
  baseView.tintAdjustmentMode = UIViewTintAdjustmentModeAutomatic;

  return image;
}

// Crops an UIImage to `frameInBaseView`.
- (UIImage*)cropImage:(UIImage*)baseImage
      frameInBaseView:(CGRect)frameInBaseView {
  if (!baseImage) {
    return nil;
  }
  DCHECK(!CGRectIsEmpty(frameInBaseView));

  // Scale `frameInBaseView` to handle an image with 2x scale.
  CGFloat scale = baseImage.scale;
  frameInBaseView.origin.x *= scale;
  frameInBaseView.origin.y *= scale;
  frameInBaseView.size.width *= scale;
  frameInBaseView.size.height *= scale;

  // Perform cropping.
  CGImageRef imageRef =
      CGImageCreateWithImageInRect(baseImage.CGImage, frameInBaseView);

  // Convert back to an UIImage.
  UIImage* image = [UIImage imageWithCGImage:imageRef
                                       scale:scale
                                 orientation:baseImage.imageOrientation];

  // Clean up a reference pointer.
  CGImageRelease(imageRef);

  return image;
}

// Returns an image of the `baseImage` overlaid with `overlays` with the given
// `frameInWindow`.
- (UIImage*)addOverlays:(NSArray<UIView*>*)overlays
              baseImage:(UIImage*)baseImage
          frameInWindow:(CGRect)frameInWindow {
  DCHECK(!CGRectIsEmpty(frameInWindow));
  if (!baseImage) {
    return nil;
  }
  // Note: If the baseImage scale differs from device scale, the baseImage size
  // may slightly differ from frameInWindow size due to rounding. Do not attempt
  // to compare the baseImage size and frameInWindow size.
  if (overlays.count == 0) {
    return baseImage;
  }
  const CGFloat kScale = [SnapshotImageScale floatImageScaleForDevice];
  DCHECK_GE(kScale, 1.0);

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = kScale;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:frameInWindow.size
                                             format:format];

  return
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
        CGContextRef context = UIContext.CGContext;

        // The base image is already a cropped snapshot so it is drawn at the
        // origin of the new image.
        [baseImage drawInRect:(CGRect){.origin = CGPointZero,
                                       .size = frameInWindow.size}];

        // This shifts the origin of the context so that future drawings can be
        // in window coordinates. For example, suppose that the desired snapshot
        // area is at (0, 99) in the window coordinate space. Drawing at (0, 99)
        // will appear as (0, 0) in the resulting image.
        CGContextTranslateCTM(context, -frameInWindow.origin.x,
                              -frameInWindow.origin.y);
        [self drawOverlays:overlays context:context];
      }];
}

// Draws `overlays` onto `context` at offsets relative to the window.
- (void)drawOverlays:(NSArray<UIView*>*)overlays context:(CGContext*)context {
  for (UIView* overlay in overlays) {
    CGContextSaveGState(context);
    CGRect frameInWindow = [overlay.superview convertRect:overlay.frame
                                                   toView:nil];
    // This shifts the context so that drawing starts at the overlay's offset.
    CGContextTranslateCTM(context, frameInWindow.origin.x,
                          frameInWindow.origin.y);
    [[overlay layer] renderInContext:context];
    CGContextRestoreGState(context);
  }
}

// Retrieves the overlays laid down on the WebState.
- (NSArray<UIView*>*)overlays {
  if (!_webState) {
    return nil;
  }
  return [_delegate
      snapshotOverlaysWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                           initWithWebState:_webState.get()]];
}

// Retrieves information needed for snapshotting.
- (std::optional<SnapshotInfo>)snapshotInfo {
  CHECK(_webState);
  SnapshotInfo snapshotInfo;
  snapshotInfo.baseView = [_delegate
      baseViewWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                   initWithWebState:_webState.get()]];
  DCHECK(snapshotInfo.baseView);

  UIEdgeInsets baseViewInsets = [_delegate
      snapshotEdgeInsetsWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                             initWithWebState:_webState.get()]];
  snapshotInfo.snapshotFrameInBaseView =
      UIEdgeInsetsInsetRect(snapshotInfo.baseView.bounds, baseViewInsets);
  if (CGRectIsEmpty(snapshotInfo.snapshotFrameInBaseView)) {
    return std::nullopt;
  }

  snapshotInfo.snapshotFrameInWindow =
      [snapshotInfo.baseView convertRect:snapshotInfo.snapshotFrameInBaseView
                                  toView:nil];
  if (CGRectIsEmpty(snapshotInfo.snapshotFrameInWindow)) {
    return std::nullopt;
  }
  return snapshotInfo;
}

@end
