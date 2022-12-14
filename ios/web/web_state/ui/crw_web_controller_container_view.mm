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
#import "ios/web/public/ui/crw_context_menu_item.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWWebControllerContainerView () <CRWViewportAdjustmentContainer>

// Redefine properties as readwrite.
@property(nonatomic, strong, readwrite)
    CRWWebViewContentView* webViewContentView;

// Convenience getter for the proxy object.
@property(nonatomic, weak, readonly) CRWWebViewProxyImpl* contentViewProxy;

@end

@implementation CRWWebControllerContainerView {
  NSMutableDictionary<NSString*, ProceduralBlock>* _currentMenuItems;
}
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
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  self.contentViewProxy.contentView = nil;
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
    [_webViewContentView setFrame:self.bounds];
    [self addSubview:_webViewContentView];
  }
}

- (CRWWebViewProxyImpl*)contentViewProxy {
  return [_delegate contentViewProxyForContainerView:self];
}

#pragma mark Layout

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    // In case the preferred content size changes, the layout is dirty.
    [self setNeedsLayout];
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // webViewContentView layout.  `-setNeedsLayout` is called in case any webview
  // layout updates need to occur despite the bounds size staying constant.
  self.webViewContentView.frame = self.bounds;
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
      [_webViewContentView setFrame:self.bounds];
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

#pragma mark Custom Context Menu

- (void)showMenuWithItems:(NSArray<CRWContextMenuItem*>*)items
                     rect:(CGRect)rect {
  [self becomeFirstResponder];
  // Remove observer, because showMenuFromView will call it when replacing an
  // existing menu.
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIMenuControllerDidHideMenuNotification
              object:nil];

  _currentMenuItems = [[NSMutableDictionary alloc] init];
  NSMutableArray* menuItems = [[NSMutableArray alloc] init];
  for (CRWContextMenuItem* item in items) {
    UIMenuItem* menuItem =
        [[UIMenuItem alloc] initWithTitle:item.title
                                   action:NSSelectorFromString(item.ID)];
    [menuItems addObject:menuItem];

    _currentMenuItems[item.ID] = item.action;
  }

  UIMenuController* menu = [UIMenuController sharedMenuController];
  menu.menuItems = menuItems;

  [menu showMenuFromView:self rect:rect];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didHideMenuNotification)
             name:UIMenuControllerDidHideMenuNotification
           object:nil];
}

// Called when menu is dismissed for cleanup.
- (void)didHideMenuNotification {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIMenuControllerDidHideMenuNotification
              object:nil];
  _currentMenuItems = nil;
}

// Checks is selector is one for an item of the custom menu and if so, tell objc
// runtime that it exists, even if it doesn't.
- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (_currentMenuItems[NSStringFromSelector(action)]) {
    return YES;
  }
  return [super canPerformAction:action withSender:sender];
}

// Catches a menu item selector and replace with `selectedMenuItemWithID` so it
// passes the test made to check is selector exists.
- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  if (_currentMenuItems[NSStringFromSelector(sel)]) {
    return
        [super methodSignatureForSelector:@selector(selectedMenuItemWithID:)];
  }
  return [super methodSignatureForSelector:sel];
}

// Catches invovation of a menu item selector and forward to
// `selectedMenuItemWithID` tagging on the menu item id for recognition.
- (void)forwardInvocation:(NSInvocation*)invocation {
  NSString* sel = NSStringFromSelector(invocation.selector);
  if (_currentMenuItems[sel]) {
    [self selectedMenuItemWithID:sel];
    return;
  }
  [super forwardInvocation:invocation];
}

// Triggers the action for the menu item with given `ID`.
- (void)selectedMenuItemWithID:(NSString*)ID {
  if (_currentMenuItems[ID]) {
    _currentMenuItems[ID]();
  }
}

@end
