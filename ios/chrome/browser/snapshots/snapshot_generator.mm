// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_generator.h"

// TODO(crbug.com/636188): required to implement ViewHierarchyContainsWKWebView
// for -drawViewHierarchyInRect:afterScreenUpdates:, remove once the workaround
// is no longer needed.
#import <WebKit/WebKit.h>

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/web/public/features.h"
#import "ios/web/public/features.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
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

// Class that contains information used when caching snapshots of a web page.
@interface CoalescingSnapshotContext : NSObject

// Returns the cached snapshot if there is one matching the given parameters.
// Returns nil otherwise.
- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Caches |snapshot| for the given |overlays| and |visibleFrameOnly|.
- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

@implementation CoalescingSnapshotContext {
  UIImage* _cachedSnapshot;
}

// Returns whether a snapshot should be cached in a page loaded context.
// Note: Returns YES if |overlays| is nil or empty and if |visibleFrameOnly| is
// YES as this is the only case when the snapshot taken for the page is reused.
- (BOOL)shouldCacheSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  return visibleFrameOnly && ![overlays count];
}

- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    return _cachedSnapshot;
  }
  return nil;
}

- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    DCHECK(!_cachedSnapshot);
    _cachedSnapshot = snapshot;
  }
}

@end

@interface SnapshotGenerator ()<CRWWebStateObserver>

// Returns the frame of the snapshot. Will return an empty rectangle if the
// WebState is not ready to capture a snapshot.
- (CGRect)snapshotFrameVisibleFrameOnly:(BOOL)visibleFrameOnly;

// Takes a snapshot for the supplied view (which should correspond to the given
// type of web view). Returns an autoreleased image cropped and scaled
// appropriately. The image can also contain overlays (if |overlays| is not
// nil and not empty).
- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays;

// Returns an image of the |snapshot| overlaid with |overlays| with the given
// |frame|.
- (UIImage*)snapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                        snapshot:(UIImage*)snapshot
                           frame:(CGRect)frame;

// Property providing access to the snapshot's cache. May be nil.
@property(nonatomic, readonly) SnapshotCache* snapshotCache;

@end

@implementation SnapshotGenerator {
  CoalescingSnapshotContext* _coalescingSnapshotContext;
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
  NSString* _snapshotSessionId;
  web::WebState* _webState;
}

@synthesize delegate = _delegate;

- (instancetype)initWithWebState:(web::WebState*)webState
               snapshotSessionId:(NSString*)snapshotSessionId {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(snapshotSessionId);
    _webState = webState;
    _snapshotSessionId = snapshotSessionId;

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

- (CGSize)snapshotSize {
  return [self snapshotFrameVisibleFrameOnly:YES].size;
}

- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled {
  if (snapshotCoalescingEnabled) {
    DCHECK(!_coalescingSnapshotContext);
    _coalescingSnapshotContext = [[CoalescingSnapshotContext alloc] init];
  } else {
    DCHECK(_coalescingSnapshotContext);
    _coalescingSnapshotContext = nil;
  }
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [SnapshotGenerator defaultSnapshotImage];
    }
    callback(image);
  };

  SnapshotCache* snapshotCache = self.snapshotCache;
  if (snapshotCache) {
    [snapshotCache retrieveImageForSessionID:_snapshotSessionId
                                    callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  __weak SnapshotGenerator* weakSelf = self;
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [weakSelf updateSnapshotWithOverlays:YES visibleFrameOnly:YES];
      if (image)
        image = GreyImage(image);
    }
    callback(image);
  };

  SnapshotCache* snapshotCache = self.snapshotCache;
  if (snapshotCache) {
    [snapshotCache retrieveGreyImageForSessionID:_snapshotSessionId
                                        callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (UIImage*)updateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  UIImage* snapshot = [self generateSnapshotWithOverlays:shouldAddOverlay
                                        visibleFrameOnly:visibleFrameOnly];
  // Return default snapshot without caching it if the generation failed.
  if (!snapshot) {
    return [[self class] defaultSnapshotImage];
  }

  [self.snapshotCache setImage:snapshot withSessionID:_snapshotSessionId];
  return snapshot;
}

- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(_webState);
  CGRect snapshotFrame = [self snapshotFrameVisibleFrameOnly:YES];
  // WebState's |TakeSnapshot()| accepts a |rect| in the web view's coordinate
  // space, but |-snapshotFrameVisibleFrameOnly:| returns a frame in the BVC's
  // coordinate space.
  CGRect webViewSnapshotFrame =
      CGRectMake(0, 0, snapshotFrame.size.width, snapshotFrame.size.height);
  if (CGRectIsEmpty(webViewSnapshotFrame)) {
    if (completion) {
      base::PostTaskWithTraits(FROM_HERE, {web::WebThread::UI},
                               base::BindOnce(^{
                                 completion(nil);
                               }));
    }
    return;
  }
  CGSize size = webViewSnapshotFrame.size;
  DCHECK(std::isnormal(size.width) && (size.width > 0))
      << ": webViewSnapshotFrame.size.width=" << size.width;
  DCHECK(std::isnormal(size.height) && (size.height > 0))
      << ": webViewSnapshotFrame.size.height=" << size.height;
  NSArray<SnapshotOverlay*>* overlays =
      [_delegate snapshotOverlaysForWebState:_webState];
  UIImage* snapshot =
      [_coalescingSnapshotContext cachedSnapshotWithOverlays:overlays
                                            visibleFrameOnly:YES];
  if (snapshot) {
    if (completion) {
      base::PostTaskWithTraits(FROM_HERE, {web::WebThread::UI},
                               base::BindOnce(^{
                                 completion(nil);
                               }));
    }
    return;
  }

  [_delegate willUpdateSnapshotForWebState:_webState];
  __weak SnapshotGenerator* weakSelf = self;
  _webState->TakeSnapshot(
      webViewSnapshotFrame, base::BindOnce(^(gfx::Image image) {
        SnapshotGenerator* strongSelf = weakSelf;
        if (!strongSelf || !_webState)
          return;
        UIImage* snapshot = image.ToUIImage();
        if (overlays.count > 0) {
          snapshot = [strongSelf snapshotWithOverlays:overlays
                                             snapshot:snapshot
                                                frame:snapshotFrame];
        }
        [strongSelf.snapshotCache setImage:snapshot
                             withSessionID:_snapshotSessionId];
        [_coalescingSnapshotContext setCachedSnapshot:snapshot
                                         withOverlays:overlays
                                     visibleFrameOnly:YES];
        [_delegate didUpdateSnapshotForWebState:_webState withImage:snapshot];
        if (completion)
          completion(snapshot);
      }));
}

- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                        visibleFrameOnly:(BOOL)visibleFrameOnly {
  CGRect frame = [self snapshotFrameVisibleFrameOnly:visibleFrameOnly];
  if (CGRectIsEmpty(frame))
    return nil;

  NSArray<SnapshotOverlay*>* overlays =
      shouldAddOverlay ? [_delegate snapshotOverlaysForWebState:_webState]
                       : nil;
  UIImage* snapshot =
      [_coalescingSnapshotContext cachedSnapshotWithOverlays:overlays
                                            visibleFrameOnly:visibleFrameOnly];

  if (snapshot)
    return snapshot;

  [_delegate willUpdateSnapshotForWebState:_webState];
  UIView* view = [_delegate viewForWebState:_webState];
  snapshot =
      [self generateSnapshotForView:view withRect:frame overlays:overlays];
  [_coalescingSnapshotContext setCachedSnapshot:snapshot
                                   withOverlays:overlays
                               visibleFrameOnly:visibleFrameOnly];
  [_delegate didUpdateSnapshotForWebState:_webState withImage:snapshot];
  return snapshot;
}

- (void)removeSnapshot {
  [self.snapshotCache removeImageWithSessionID:_snapshotSessionId];
}

+ (UIImage*)defaultSnapshotImage {
  static UIImage* defaultSnapshotImage = nil;
  if (!defaultSnapshotImage) {
    CGRect frame = CGRectMake(0, 0, 2, 2);
    UIGraphicsBeginImageContext(frame.size);
    [[UIColor whiteColor] setFill];
    CGContextFillRect(UIGraphicsGetCurrentContext(), frame);

    UIImage* result = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    defaultSnapshotImage =
        [result stretchableImageWithLeftCapWidth:1 topCapHeight:1];
  }
  return defaultSnapshotImage;
}

#pragma mark - Private methods

- (CGRect)snapshotFrameVisibleFrameOnly:(BOOL)visibleFrameOnly {
  // Do not generate a snapshot if web usage is disabled (as the WebState's
  // view is blank in that case).
  if (!_webState->IsWebUsageEnabled())
    return CGRectZero;

  // Do not generate a snapshot if the delegate says the WebState view is
  // not ready (this generally mean a placeholder is displayed).
  if (_delegate && ![_delegate canTakeSnapshotForWebState:_webState])
    return CGRectZero;

  UIView* view = [_delegate viewForWebState:_webState];
  CGRect frame = [view bounds];
  UIEdgeInsets headerInsets = UIEdgeInsetsZero;
  if (visibleFrameOnly) {
    headerInsets = [_delegate snapshotEdgeInsetsForWebState:_webState];
  } else if (base::FeatureList::IsEnabled(
                 web::features::kBrowserContainerFullscreen)) {
    headerInsets = UIEdgeInsetsMake(StatusBarHeight(), 0, 0, 0);
  }
  frame = UIEdgeInsetsInsetRect(frame, headerInsets);

  return frame;
}

- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays {
  DCHECK(view);
  CGSize size = rect.size;
  DCHECK(std::isnormal(size.width) && (size.width > 0))
      << ": size.width=" << size.width;
  DCHECK(std::isnormal(size.height) && (size.height > 0))
      << ": size.height=" << size.height;
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  DCHECK(context);

  // TODO(crbug.com/636188): -drawViewHierarchyInRect:afterScreenUpdates: is
  // buggy on iOS 8/9/10 (and state is unknown for iOS 11) causing GPU glitches,
  // screen redraws during animations, broken pinch to dismiss on tablet, etc.
  // For the moment, only use it for WKWebView with depends on it. Remove this
  // check and always use -drawViewHierarchyInRect:afterScreenUpdates: once it
  // is working correctly in all version of iOS supported.
  BOOL useDrawViewHierarchy = ViewHierarchyContainsWKWebView(view);

  BOOL snapshotSuccess = YES;
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, -rect.origin.x, -rect.origin.y);
  if (useDrawViewHierarchy) {
    snapshotSuccess =
        [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:NO];
  } else {
    [[view layer] renderInContext:context];
  }
  if ([overlays count]) {
    for (SnapshotOverlay* overlay in overlays) {
      // Render the overlay view at the desired offset. It is achieved
      // by shifting origin of context because view frame is ignored when
      // drawing to context.
      CGContextSaveGState(context);
      CGContextTranslateCTM(context, 0, overlay.yOffset);
      if (useDrawViewHierarchy) {
        [overlay.view drawViewHierarchyInRect:overlay.view.bounds
                           afterScreenUpdates:YES];
      } else {
        [[overlay.view layer] renderInContext:context];
      }
      CGContextRestoreGState(context);
    }
  }
  UIImage* image = nil;
  if (snapshotSuccess)
    image = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return image;
}

- (UIImage*)snapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                        snapshot:(UIImage*)snapshot
                           frame:(CGRect)frame {
  CGSize size = frame.size;
  DCHECK(std::isnormal(size.width) && (size.width > 0))
      << ": size.width=" << size.width;
  DCHECK(std::isnormal(size.height) && (size.height > 0))
      << ": size.height=" << size.height;
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  DCHECK(context);
  CGContextSaveGState(context);
  [snapshot drawAtPoint:CGPointZero];
  for (SnapshotOverlay* overlay in overlays) {
    // Render the overlay view at the desired offset. It is achieved
    // by shifting origin of context because view frame is ignored when
    // drawing to context.
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0, overlay.yOffset - frame.origin.y);
    [overlay.view drawViewHierarchyInRect:overlay.view.bounds
                       afterScreenUpdates:YES];
    CGContextRestoreGState(context);
  }
  UIImage* snapshotWithOverlays = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return snapshotWithOverlays;
}

#pragma mark - Properties.

- (SnapshotCache*)snapshotCache {
  return SnapshotCacheFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(_webState->GetBrowserState()));
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
