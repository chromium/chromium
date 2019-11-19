// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_generator.h"

#include <algorithm>

// TODO(crbug.com/636188): required to implement ViewHierarchyContainsWKWebView
// for -drawViewHierarchyInRect:afterScreenUpdates:, remove once the workaround
// is no longer needed.
#import <WebKit/WebKit.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Contains information needed for snapshotting.
struct SnapshotInfo {
  UIView* baseView;
  CGRect snapshotFrameInBaseView;
  CGRect snapshotFrameInWindow;
  NSArray<UIView*>* overlays;
};

// Returns YES if |view| or any view it contains is a WKWebView.
BOOL ViewHierarchyContainsWKWebView(UIView* view) {
  if ([view isKindOfClass:[WKWebView class]])
    return YES;
  for (UIView* subview in view.subviews) {
    if (ViewHierarchyContainsWKWebView(subview))
      return YES;
  }
  return NO;
}

}  // namespace

@interface SnapshotGenerator ()<CRWWebStateObserver>

// Property providing access to the snapshot's cache. May be nil.
@property(nonatomic, readonly) SnapshotCache* snapshotCache;

// The unique ID for the web state.
@property(nonatomic, copy) NSString* sessionID;

// The associated web state.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation SnapshotGenerator {
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
}

- (instancetype)initWithWebState:(web::WebState*)webState
               snapshotSessionId:(NSString*)snapshotSessionId {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(snapshotSessionId);
    _webState = webState;
    _sessionID = snapshotSessionId;

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (self.snapshotCache) {
    [self.snapshotCache retrieveImageForSessionID:self.sessionID
                                         callback:callback];
  } else {
    callback(nil);
  }
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  __weak SnapshotGenerator* weakSelf = self;
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [weakSelf updateSnapshot];
      if (image)
        image = GreyImage(image);
    }
    callback(image);
  };

  SnapshotCache* snapshotCache = self.snapshotCache;
  if (snapshotCache) {
    [snapshotCache retrieveGreyImageForSessionID:self.sessionID
                                        callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (UIImage*)updateSnapshot {
  UIImage* snapshot = [self generateSnapshotWithOverlays:YES];
  [self updateSnapshotCacheWithImage:snapshot];
  return snapshot;
}

- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(!web::GetWebClient()->IsAppSpecificURL(
      self.webState->GetLastCommittedURL()));

  if (![self canTakeSnapshot]) {
    if (completion) {
      base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                       completion(nil);
                     }));
    }
    return;
  }
  SnapshotInfo snapshotInfo = [self getSnapshotInfo];
  CGRect snapshotFrameInWebView =
      [self.webState->GetView() convertRect:snapshotInfo.snapshotFrameInBaseView
                                   fromView:snapshotInfo.baseView];
  [self.delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:self.webState];
  __weak SnapshotGenerator* weakSelf = self;
  self.webState->TakeSnapshot(
      gfx::RectF(snapshotFrameInWebView),
      base::BindRepeating(^(const gfx::Image& image) {
        UIImage* snapshot = nil;
        if (!image.IsEmpty()) {
          snapshot = [weakSelf
              snapshotWithOverlays:snapshotInfo.overlays
                         baseImage:image.ToUIImage()
                     frameInWindow:snapshotInfo.snapshotFrameInWindow];
        }
        [weakSelf updateSnapshotCacheWithImage:snapshot];
        if (completion)
          completion(snapshot);
      }));
}

- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay {
  if (![self canTakeSnapshot])
    return nil;
  SnapshotInfo snapshotInfo = [self getSnapshotInfo];
  [self.delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:self.webState];
  UIImage* baseImage =
      [self snapshotBaseView:snapshotInfo.baseView
             frameInBaseView:snapshotInfo.snapshotFrameInBaseView];
  return [self
      snapshotWithOverlays:(shouldAddOverlay ? snapshotInfo.overlays : nil)
                 baseImage:baseImage
             frameInWindow:snapshotInfo.snapshotFrameInWindow];
}

- (void)removeSnapshot {
  [self.snapshotCache removeImageWithSessionID:self.sessionID];
}

#pragma mark - Private methods

// Returns NO if WebState or the view is not ready for snapshot.
- (BOOL)canTakeSnapshot {
  // This allows for easier unit testing of classes that use SnapshotGenerator.
  if (!self.delegate)
    return NO;

  // Do not generate a snapshot if web usage is disabled (as the WebState's
  // view is blank in that case).
  if (!self.webState->IsWebUsageEnabled())
    return NO;

  return [self.delegate snapshotGenerator:self
               canTakeSnapshotForWebState:self.webState];
}

// Returns a snapshot of |baseView| with |frameInBaseView|.
- (UIImage*)snapshotBaseView:(UIView*)baseView
             frameInBaseView:(CGRect)frameInBaseView {
  DCHECK(baseView);
  DCHECK(!CGRectIsEmpty(frameInBaseView));
  // Note: When not using device scale, the output image size may slightly
  // differ from the input size due to rounding.
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(frameInBaseView.size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  // This shifts the origin of the context to be the origin of the snapshot
  // frame.
  CGContextTranslateCTM(context, -frameInBaseView.origin.x,
                        -frameInBaseView.origin.y);
  BOOL snapshotSuccess = YES;

  // TODO(crbug.com/636188): |-drawViewHierarchyInRect:afterScreenUpdates:| is
  // buggy on iOS 8/9/10 (and state is unknown for iOS 11) causing GPU glitches,
  // screen redraws during animations, broken pinch to dismiss on tablet, etc.
  // Ensure iOS 11 is not affected by these issues before turning on
  // |kSnapshotDrawView| experiment. On the other hand, |-renderInContext:| is
  // buggy for WKWebView, which is used for some Chromium pages such as "No
  // internet" or "Site can't be reached".
  BOOL useDrawViewHierarchy = ViewHierarchyContainsWKWebView(baseView) ||
                              base::FeatureList::IsEnabled(kSnapshotDrawView);
  // |drawViewHierarchyInRect:| has undefined behavior when the view is not
  // in the visible view hierarchy. In practice, when this method is called
  // on a view that is part of view controller containment and not in the view
  // hierarchy, an UIViewControllerHierarchyInconsistency exception will be
  // thrown.
  if (useDrawViewHierarchy && baseView.window) {
    snapshotSuccess = [baseView drawViewHierarchyInRect:baseView.bounds
                                     afterScreenUpdates:YES];
  } else {
    [[baseView layer] renderInContext:context];
  }
  UIImage* image = nil;
  if (snapshotSuccess)
    image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

// Returns an image of the |baseImage| overlaid with |overlays| with the given
// |frameInWindow|.
- (UIImage*)snapshotWithOverlays:(NSArray<UIView*>*)overlays
                       baseImage:(UIImage*)baseImage
                   frameInWindow:(CGRect)frameInWindow {
  DCHECK(!CGRectIsEmpty(frameInWindow));
  if (!baseImage)
    return nil;
  // Note: If the baseImage scale differs from device scale, the baseImage size
  // may slightly differ from frameInWindow size due to rounding. Do not attempt
  // to compare the baseImage size and frameInWindow size.
  if (overlays.count == 0)
    return baseImage;
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(frameInWindow.size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  // The base image is already a cropped snapshot so it is drawn at the origin
  // of the new image.
  [baseImage drawAtPoint:CGPointZero];
  // This shifts the origin of the context so that future drawings can be in
  // window coordinates. For example, suppose that the desired snapshot area is
  // at (0, 99) in the window coordinate space. Drawing at (0, 99) will appear
  // as (0, 0) in the resulting image.
  CGContextTranslateCTM(context, -frameInWindow.origin.x,
                        -frameInWindow.origin.y);
  [self drawOverlays:overlays context:context];
  UIImage* snapshot = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return snapshot;
}

// Updates the snapshot cache with |snapshot|.
- (void)updateSnapshotCacheWithImage:(UIImage*)snapshot {
  if (snapshot) {
    [self.snapshotCache setImage:snapshot withSessionID:self.sessionID];
  } else {
    // Remove any stale snapshot since the snapshot failed.
    [self.snapshotCache removeImageWithSessionID:self.sessionID];
  }
}

// Draws |overlays| onto |context| at offsets relative to the window.
- (void)drawOverlays:(NSArray<UIView*>*)overlays context:(CGContext*)context {
  for (UIView* overlay in overlays) {
    CGContextSaveGState(context);
    CGRect frameInWindow = [overlay.superview convertRect:overlay.frame
                                                   toView:nil];
    // This shifts the context so that drawing starts at the overlay's offset.
    CGContextTranslateCTM(context, frameInWindow.origin.x,
                          frameInWindow.origin.y);
    // |drawViewHierarchyInRect:| has undefined behavior when the view is not
    // in the visible view hierarchy. In practice, when this method is called
    // on a view that is part of view controller containment, an
    // UIViewControllerHierarchyInconsistency exception will be thrown.
    if (base::FeatureList::IsEnabled(kSnapshotDrawView) && overlay.window) {
      // The rect's origin is ignored. Only size is used.
      [overlay drawViewHierarchyInRect:overlay.bounds afterScreenUpdates:YES];
    } else {
      [[overlay layer] renderInContext:context];
    }
    CGContextRestoreGState(context);
  }
}

// Retrieves information needed for snapshotting.
- (SnapshotInfo)getSnapshotInfo {
  SnapshotInfo snapshotInfo;
  snapshotInfo.baseView = [self.delegate snapshotGenerator:self
                                       baseViewForWebState:self.webState];
  DCHECK(snapshotInfo.baseView);
  UIEdgeInsets baseViewInsets = [self.delegate snapshotGenerator:self
                                   snapshotEdgeInsetsForWebState:self.webState];
  snapshotInfo.snapshotFrameInBaseView =
      UIEdgeInsetsInsetRect(snapshotInfo.baseView.bounds, baseViewInsets);
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInBaseView));
  snapshotInfo.snapshotFrameInWindow =
      [snapshotInfo.baseView convertRect:snapshotInfo.snapshotFrameInBaseView
                                  toView:nil];
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInWindow));
  snapshotInfo.overlays = [self.delegate snapshotGenerator:self
                               snapshotOverlaysForWebState:self.webState];
  return snapshotInfo;
}

#pragma mark - Properties

- (SnapshotCache*)snapshotCache {
  return SnapshotCacheFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(
          self.webState->GetBrowserState()));
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
