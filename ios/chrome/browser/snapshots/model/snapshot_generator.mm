// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_generator.h"

// It is required to implement ViewHierarchyContainsWebView to check if
// the view is `WKWebView`.
#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "build/blink_buildflags.h"
#import "ios/chrome/browser/snapshots/model/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_scale.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ui/gfx/geometry/rect_f.h"
#import "ui/gfx/image/image.h"

namespace {

// Contains information needed for snapshotting.
struct SnapshotInfo {
  UIView* baseView;
  CGRect snapshotFrameInBaseView;
  CGRect snapshotFrameInWindow;
};

// Returns YES if `view` or any view it contains is a WKWebView.
BOOL ViewHierarchyContainsWebView(UIView* view) {
  if ([view isKindOfClass:[WKWebView class]]) {
    return YES;
  }
#if BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/1419001): Remove NSClassFromString and use the class
  // directly when possible.
  if ([view isKindOfClass:[NSClassFromString(@"RenderWidgetUIView") class]]) {
    return YES;
  }
#endif  // USE_BLINK
  for (UIView* subview in view.subviews) {
    if (ViewHierarchyContainsWebView(subview)) {
      return YES;
    }
  }
  return NO;
}

}  // namespace

@implementation SnapshotGenerator {
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
  [_delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:_webState.get()];

  SnapshotInfo snapshotInfo = [self snapshotInfo];
  CGRect snapshotFrameInWebView =
      [_webState->GetView() convertRect:snapshotInfo.snapshotFrameInBaseView
                               fromView:snapshotInfo.baseView];
  auto wrappedCompletion =
      ^(__weak SnapshotGenerator* generator, const gfx::Image& image) {
        if (!generator) {
          completion(nil);
        }
        UIImage* snapshot = nil;
        if (!image.IsEmpty()) {
          snapshot = [generator addOverlays:[generator overlays]
                                  baseImage:image.ToUIImage()
                              frameInWindow:snapshotInfo.snapshotFrameInWindow];
        }
        if (completion) {
          completion(snapshot);
        }
      };

  __weak SnapshotGenerator* weakSelf = self;
  _webState->TakeSnapshot(gfx::RectF(snapshotFrameInWebView),
                          base::BindRepeating(wrappedCompletion, weakSelf));
}

- (UIImage*)generateUIViewSnapshot {
  if (![self canTakeSnapshot] || !_webState) {
    return nil;
  }
  [_delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:_webState.get()];

  SnapshotInfo snapshotInfo = [self snapshotInfo];
  // Ideally, generate an UIImage by one step with `UIGraphicsImageRenderer`,
  // however, it generates a black image when the size of `baseView` is larger
  // than `frameInBaseView`. So this is a workaround to generate an UIImage by
  // dividing the step into 2 steps; 1) convert an UIView to an UIImage 2) crop
  // an UIImage with `frameInBaseView`.
  UIImage* baseImage = [self convertFromBaseView:snapshotInfo.baseView];
  return [self cropImage:baseImage
         frameInBaseView:snapshotInfo.snapshotFrameInBaseView];
}

- (UIImage*)generateUIViewSnapshotWithOverlays {
  if (![self canTakeSnapshot]) {
    return nil;
  }
  SnapshotInfo snapshotInfo = [self snapshotInfo];
  return [self addOverlays:[self overlays]
                 baseImage:[self generateUIViewSnapshot]
             frameInWindow:snapshotInfo.snapshotFrameInWindow];
}

#pragma mark - Private methods

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

  return [_delegate snapshotGenerator:self
           canTakeSnapshotForWebState:_webState.get()];
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
        if (baseView.window && ViewHierarchyContainsWebView(baseView)) {
          // `-renderInContext:` is the preferred way to render a snapshot, but
          // it's buggy for WKWebView, which is used for some WebUI pages such
          // as "No internet" or "Site can't be reached". If a
          // WKWebView-containing hierarchy must be snapshotted, the UIView
          // `-drawViewHierarchyInRect:` method is used instead.
          // `drawViewHierarchyInRect:` has undefined behavior when the view is
          // not in the visible view hierarchy. In practice, when this method is
          // called on a view that is part of view controller containment and
          // not in the view hierarchy, an
          // UIViewControllerHierarchyInconsistency exception will be thrown.
          snapshotSuccess = [baseView drawViewHierarchyInRect:baseView.bounds
                                           afterScreenUpdates:YES];
        } else {
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
  return [_delegate snapshotGenerator:self
          snapshotOverlaysForWebState:_webState.get()];
}

// Retrieves information needed for snapshotting.
- (SnapshotInfo)snapshotInfo {
  CHECK(_webState);
  SnapshotInfo snapshotInfo;
  snapshotInfo.baseView = [_delegate snapshotGenerator:self
                                   baseViewForWebState:_webState.get()];
  DCHECK(snapshotInfo.baseView);

  UIEdgeInsets baseViewInsets = [_delegate snapshotGenerator:self
                               snapshotEdgeInsetsForWebState:_webState.get()];
  snapshotInfo.snapshotFrameInBaseView =
      UIEdgeInsetsInsetRect(snapshotInfo.baseView.bounds, baseViewInsets);
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInBaseView));

  snapshotInfo.snapshotFrameInWindow =
      [snapshotInfo.baseView convertRect:snapshotInfo.snapshotFrameInBaseView
                                  toView:nil];
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInWindow));
  return snapshotInfo;
}

@end
