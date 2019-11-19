// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/deprecated/crw_native_content_provider.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/controller/crw_legacy_native_content_controller_delegate.h"
#import "ios/web/web_state/web_state_impl.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWLegacyNativeContentController () <CRWNativeContentDelegate>

// The currently displayed native controller, if any.
@property(nonatomic, strong) id<CRWNativeContent> nativeController;
@property(nonatomic, assign) web::WebStateImpl* webStateImpl;
@property(nonatomic, assign, readonly)
    web::NavigationManagerImpl* navigationManagerImpl;
@property(nonatomic, assign, readonly) web::NavigationItemImpl* currentNavItem;
// Set to YES when [self close] is called.
@property(nonatomic, assign) BOOL beingDestroyed;

@end

@implementation CRWLegacyNativeContentController

@synthesize nativeProvider = _nativeProvider;

- (instancetype)initWithWebState:(web::WebStateImpl*)webState {
  self = [super init];
  if (self) {
    DCHECK(webState);
    _webStateImpl = webState;
  }
  return self;
}

#pragma mark - Properties

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  // Check for pointer equality.
  if (_nativeController == nativeController)
    return;

  // Unset the delegate on the previous instance.
  if ([_nativeController respondsToSelector:@selector(setDelegate:)])
    [_nativeController setDelegate:nil];

  id<CRWNativeContent> previousController = _nativeController;
  _nativeController = nativeController;
  [self.delegate legacyNativeContentController:self
                        nativeContentDidChange:previousController];
  [self setNativeControllerWebUsageEnabled:
            [self.delegate legacyNativeContentControllerWebUsageEnabled:self]];
}

- (web::NavigationManagerImpl*)navigationManagerImpl {
  return self.webStateImpl ? &(self.webStateImpl->GetNavigationManagerImpl())
                           : nil;
}

- (web::NavigationItemImpl*)currentNavItem {
  return self.navigationManagerImpl
             ? self.navigationManagerImpl->GetCurrentItemImpl()
             : nullptr;
}

#pragma mark - Public

- (BOOL)hasController {
  return self.nativeController != nil;
}

- (BOOL)shouldLoadURLInNativeView:(const GURL&)URL {
  // App-specific URLs that don't require WebUI are loaded in native views.
  return web::GetWebClient()->IsAppSpecificURL(URL) &&
         !self.webStateImpl->HasWebUI() &&
         [self.nativeProvider hasControllerForURL:URL];
}

- (void)setNativeControllerWebUsageEnabled:(BOOL)webUsageEnabled {
  if ([self.nativeController
          respondsToSelector:@selector(setWebUsageEnabled:)]) {
    [self.nativeController setWebUsageEnabled:webUsageEnabled];
  }
}

- (void)loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
    (BOOL)rendererInitiated {
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // Free the web view.
    [self.delegate legacyNativeContentControllerRemoveWebView:self];
    [self presentNativeContentForNavigationItem:self.currentNavItem];
    [self didLoadNativeContentForNavigationItem:self.currentNavItem
                             placeholderContext:nullptr
                              rendererInitiated:rendererInitiated];
  } else {
    // Just present the native view now. Leave the rest of native content load
    // until the placeholder navigation finishes.
    [self presentNativeContentForNavigationItem:self.currentNavItem];
    web::NavigationContextImpl* context = [self.delegate
         legacyNativeContentController:self
        loadPlaceholderInWebViewForURL:self.currentNavItem->GetVirtualURL()
                     rendererInitiated:rendererInitiated
                            forContext:nullptr];
    context->SetIsNativeContentPresented(true);
  }
}

- (void)webViewDidFinishNavigationWithContext:
            (web::NavigationContextImpl*)context
                                      andItem:(web::NavigationItemImpl*)item {
  // Native content may have already been presented if this navigation is
  // started in
  // |-loadCurrentURLInNativeViewWithRendererInitiatedNavigation:|. If
  // not, present it now.
  if (!context->IsNativeContentPresented()) {
    [self presentNativeContentForNavigationItem:item];
  }
  bool rendererInitiated = context->IsRendererInitiated();
  [self didLoadNativeContentForNavigationItem:item
                           placeholderContext:context
                            rendererInitiated:rendererInitiated];
}

- (void)handleCancelledErrorForContext:(web::NavigationContextImpl*)context {
  // If discarding the non-committed entries results in native content URL,
  // reload it in its native view. For WKBasedNavigationManager, this is not
  // necessary because WKWebView takes care of reloading the placeholder URL,
  // which triggers native view upon completion.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled() &&
      ![self hasController]) {
    GURL lastCommittedURL = self.webStateImpl->GetLastCommittedURL();
    if ([self shouldLoadURLInNativeView:lastCommittedURL]) {
      [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:
                context->IsRendererInitiated()];
    }
  }
}

- (void)handleSSLError {
  // If discarding non-committed items results in a NavigationItem that
  // should be loaded via a native controller, load that URL, as its
  // native controller will need to be recreated.  Note that a
  // successful preload of a page with an certificate error will result
  // in this block executing on a CRWWebController with no
  // NavigationManager.  Additionally, if a page with a certificate
  // error is opened in a new tab, its last committed NavigationItem
  // will be null.
  web::NavigationManager* navigationManager = self.navigationManagerImpl;
  web::NavigationItem* item =
      navigationManager ? navigationManager->GetLastCommittedItem() : nullptr;
  if (item && [self shouldLoadURLInNativeView:item->GetURL()]) {
    // RendererInitiated flag is meaningless for showing previous native
    // content page. RendererInitiated is used as less previledged.
    [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:YES];
  }
}

- (void)stopLoading {
  web::NavigationItem* item = self.currentNavItem;
  GURL navigationURL = item ? item->GetVirtualURL() : GURL::EmptyGURL();
  // If discarding the non-committed entries results in an app-specific URL,
  // reload it in its native view.
  if (![self hasController] && [self shouldLoadURLInNativeView:navigationURL]) {
    // RendererInitiated flag is meaningless for showing previous native
    // content page. RendererInitiated is used as less previledged.
    [self loadCurrentURLInNativeViewWithRendererInitiatedNavigation:YES];
  }
}

- (void)resetNativeController {
  _nativeController = nil;
}

- (void)wasShown {
  if ([self.nativeController respondsToSelector:@selector(wasShown)]) {
    [self.nativeController wasShown];
  }
}

- (void)wasHidden {
  if ([self.nativeController respondsToSelector:@selector(wasHidden)]) {
    [self.nativeController wasHidden];
  }
}

- (void)close {
  self.beingDestroyed = YES;
  self.nativeProvider = nil;
  if ([self.nativeController respondsToSelector:@selector(close)])
    [self.nativeController close];
  if ([self.nativeController respondsToSelector:@selector(setDelegate:)])
    [self.nativeController setDelegate:nil];
}

- (const GURL&)URL {
  if ([self.nativeController respondsToSelector:@selector(virtualURL)]) {
    return [self.nativeController virtualURL];
  } else {
    return [self.nativeController url];
  }
}

- (void)reload {
  [self.nativeController reload];
}

- (CGPoint)contentOffset {
  if ([self.nativeController respondsToSelector:@selector(contentOffset)])
    return [self.nativeController contentOffset];
  return CGPointZero;
}

- (UIEdgeInsets)contentInset {
  if ([self.nativeController respondsToSelector:@selector(contentInset)])
    return [self.nativeController contentInset];
  return UIEdgeInsetsZero;
}

#pragma mark - CRWNativeContentDelegate methods

- (void)nativeContent:(id)content titleDidChange:(NSString*)title {
  [self.delegate legacyNativeContentController:self
                         setNativeContentTitle:title];
}

- (void)nativeContent:(id)content
    handleContextMenu:(const web::ContextMenuParams&)params {
  if (self.beingDestroyed) {
    return;
  }
  self.webStateImpl->HandleContextMenu(params);
}

#pragma mark - Private

// Presents native content using the native controller for |item| without
// notifying WebStateObservers. This method does not modify the underlying web
// view. It simply covers the web view with the native content.
// |-didLoadNativeContentForNavigationItem| must be called some time later
// to notify WebStateObservers.
- (void)presentNativeContentForNavigationItem:(web::NavigationItem*)item {
  const GURL targetURL = item ? item->GetURL() : GURL::EmptyGURL();
  id<CRWNativeContent> nativeContent =
      [self.nativeProvider controllerForURL:targetURL
                                   webState:self.webStateImpl];
  // Unlike the WebView case, always create a new controller and view.
  // TODO(crbug.com/759178): What to do if this does return nil?
  [self setNativeController:nativeContent];
  if ([nativeContent respondsToSelector:@selector(virtualURL)]) {
    item->SetVirtualURL([nativeContent virtualURL]);
  }

  NSString* title = [self.nativeController title];
  if (title && item) {
    base::string16 newTitle = base::SysNSStringToUTF16(title);
    item->SetTitle(newTitle);
  }
}

// Notifies WebStateObservers the completion of this navigation.
- (void)didLoadNativeContentForNavigationItem:(web::NavigationItemImpl*)item
                           placeholderContext:
                               (web::NavigationContextImpl*)placeholderContext
                            rendererInitiated:(BOOL)rendererInitiated {
  DCHECK(!placeholderContext || placeholderContext->IsPlaceholderNavigation());
  const GURL targetURL = item ? item->GetURL() : GURL::EmptyGURL();
  const web::Referrer referrer;
  ui::PageTransition transition = self.currentNavItem
                                      ? self.currentNavItem->GetTransitionType()
                                      : ui::PageTransitionFromInt(0);

  std::unique_ptr<web::NavigationContextImpl> context =
      [self.delegate legacyNativeContentController:self
                         registerLoadRequestForURL:targetURL
                                          referrer:referrer
                                        transition:transition
                            sameDocumentNavigation:NO
                                    hasUserGesture:YES
                                 rendererInitiated:rendererInitiated
                             placeholderNavigation:NO];

  self.webStateImpl->OnNavigationStarted(context.get());
  [self.delegate legacyNativeContentControllerDidStartLoading:self];
  if (placeholderContext && placeholderContext->GetItem()) {
    DCHECK_EQ(placeholderContext->GetItem(), item);
    self.navigationManagerImpl->CommitPendingItem(
        placeholderContext->ReleaseItem());
  } else {
    self.navigationManagerImpl->CommitPendingItem();
  }
  context->SetHasCommitted(true);
  self.webStateImpl->OnNavigationFinished(context.get());

  if (item && web::GetWebClient()->IsAppSpecificURL(item->GetURL())) {
    // Report the successful navigation to the ErrorRetryStateMachine.
    item->error_retry_state_machine().SetNoNavigationError();
  }

  NSString* title = [self.nativeController title];
  if (title) {
    [self.delegate legacyNativeContentController:self
                           setNativeContentTitle:title];
  }

  if ([self.nativeController respondsToSelector:@selector(setDelegate:)]) {
    [self.nativeController setDelegate:self];
  }

  [self.delegate legacyNativeContentController:self
             nativeContentLoadDidFinishWithURL:targetURL
                                       context:context.get()];
}

@end
