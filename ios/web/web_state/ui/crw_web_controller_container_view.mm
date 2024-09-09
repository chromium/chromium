// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller_container_view.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_viewport_adjustment_container.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/features.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

@interface CRWWebControllerContainerView () <CRWViewportAdjustmentContainer>

// Redefine properties as readwrite.
@property(nonatomic, strong, readwrite)
    CRWWebViewContentView* webViewContentView;

// Convenience getter for the proxy object.
@property(nonatomic, weak, readonly) CRWWebViewProxyImpl* contentViewProxy;

@end

@implementation CRWWebControllerContainerView

@synthesize webViewContentView = _webViewContentView;
@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:
    (id<CRWWebControllerContainerViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    if (@available(iOS 17, *)) {
      __weak __typeof(self) weakSelf = self;
      UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                       UITraitCollection* previousCollection) {
        [weakSelf updateUIOnTraitChange:previousCollection];
      };
      NSArray<UITrait>* traits = @[
        UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self,
        UITraitPreferredContentSizeCategory.self
      ];
      [self registerForTraitChanges:traits withHandler:handler];
    }
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (void)dealloc {
  [self.contentViewProxy clearContentViewAndAddPlaceholder:NO];
}

#pragma mark Accessors

- (UIView<CRWViewportAdjustment>*)fullscreenViewportAdjuster {
  if (![self.webViewContentView
          conformsToProtocol:@protocol(CRWViewportAdjustment)]) {
    return nil;
  }
  return self.webViewContentView;
}

- (void)setWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  if (![_webViewContentView isEqual:webViewContentView]) {
    [_webViewContentView removeFromSuperview];
    _webViewContentView = webViewContentView;
    [self updateWebViewContentViewFrame];
    [self addSubview:_webViewContentView];
  }
}

- (CRWWebViewProxyImpl*)contentViewProxy {
  return [_delegate contentViewProxyForContainerView:self];
}

#pragma mark Layout

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateUIOnTraitChange:previousTraitCollection];
}
#endif

- (void)layoutSubviews {
  [super layoutSubviews];

  // webViewContentView layout.  `-setNeedsLayout` is called in case any webview
  // layout updates need to occur despite the bounds size staying constant.
  [self updateWebViewContentViewFrame];
  [self.webViewContentView setNeedsLayout];
}

- (BOOL)isViewAlive {
  return self.webViewContentView;
}

- (void)willMoveToWindow:(UIWindow*)newWindow {
  [super willMoveToWindow:newWindow];
  [self updateWebViewContentViewForContainerWindow:newWindow];
}

- (void)updateWebViewContentViewForContainerWindow:(UIWindow*)containerWindow {
  if (!base::FeatureList::IsEnabled(web::features::kKeepsRenderProcessAlive))
    return;

  if (!self.webViewContentView)
    return;

  // If there's a containerWindow or `webViewContentView` is inactive, put it
  // back where it belongs.
  if (containerWindow ||
      ![_delegate shouldKeepRenderProcessAliveForContainerView:self]) {
    if (self.webViewContentView.superview != self) {
      [self updateWebViewContentViewFrame];
      // Insert the content view on the back of the container view so any view
      // that was presented on top of the content view can still appear.
      [self insertSubview:_webViewContentView atIndex:0];
    }
    return;
  }

  // There's no window and `webViewContentView` is active, stash it.
  [_delegate containerView:self storeWebViewInWindow:self.webViewContentView];
}

#pragma mark Content Setters

- (void)resetContentForShutdown:(BOOL)shutdown {
  self.webViewContentView = nil;
  [self.contentViewProxy clearContentViewAndAddPlaceholder:!shutdown];
}

- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  DCHECK(webViewContentView);
  self.webViewContentView = webViewContentView;
  self.contentViewProxy.contentView = self.webViewContentView;
  [self updateWebViewContentViewForContainerWindow:self.window];
  [self setNeedsLayout];
}

- (void)updateWebViewContentViewFullscreenState:
    (CrFullscreenState)fullscreenState {
  DCHECK(_webViewContentView);
  [self.webViewContentView updateFullscreenState:fullscreenState];
}

#pragma mark UIView (printing)

// Only print the web view by returning the web view printformatter.
- (UIViewPrintFormatter*)viewPrintFormatter {
  return [self.webViewContentView.webView viewPrintFormatter];
}

- (void)drawRect:(CGRect)rect
    forViewPrintFormatter:(UIViewPrintFormatter*)formatter {
  [self.webViewContentView.webView drawRect:rect];
}

#pragma mark - UIView overrides

- (void)safeAreaInsetsDidChange {
  // Update the frame to take into account the safe area inset as they are set
  // fractionally later than the rest of the view loading.
  [self updateWebViewContentViewFrame];
}

#pragma mark - Private helpers

// Update the content view frame.
- (void)updateWebViewContentViewFrame {
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    [self.webViewContentView setFrame:self.bounds];
  } else {
    if (self.cover) {
      [self.webViewContentView setFrame:self.bounds];
    } else {
      [self.webViewContentView
          setFrame:UIEdgeInsetsInsetRect(self.bounds, self.safeAreaInsets)];
    }
  }
}

//
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if ((self.traitCollection.verticalSizeClass !=
       previousTraitCollection.verticalSizeClass) ||
      (self.traitCollection.horizontalSizeClass !=
       previousTraitCollection.horizontalSizeClass) ||
      self.traitCollection.preferredContentSizeCategory !=
          previousTraitCollection.preferredContentSizeCategory) {
    // Reset zoom scale when the window is resized (portrait to landscape,
    // landscape to portrait or multi-window resizing), or if text size is
    // modified as websites can adjust to the preferred content size (using
    // font: -apple-system-body;). It avoids being in a different zoomed
    // position from where the user initially zoomed.
    UIScrollView* scrollView = self.contentViewProxy.contentView.scrollView;
    scrollView.zoomScale = scrollView.minimumZoomScale;
  }
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    // In case the preferred content size changes, the layout is dirty.
    [self setNeedsLayout];
  }
}

@end
