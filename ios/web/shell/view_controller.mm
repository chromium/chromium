// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <stdint.h>

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web/shell/shell_browser_state.h"
#import "ios/web/shell/shell_web_client.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

NSString* const kWebShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebShellAddressFieldAccessibilityLabel = @"Address field";

using web::NavigationManager;

@interface ViewController ()<CRWWebStateDelegate,
                             CRWWebStateObserver,
                             UITextFieldDelegate,
                             UIToolbarDelegate> {
  raw_ptr<web::BrowserState> _browserState;
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegate;
}
@property(nonatomic, assign, readonly) NavigationManager* navigationManager;
@property(nonatomic, readwrite, strong) UITextField* field;
@end

@implementation ViewController

@synthesize field = _field;
@synthesize containerView = _containerView;
@synthesize toolbarView = _toolbarView;

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState.reset();
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  CGRect bounds = self.view.bounds;

  // Set up the toolbar.
  _toolbarView = [[UIToolbar alloc] init];
  _toolbarView.barTintColor =
      [UIColor colorWithRed:0.337 green:0.467 blue:0.988 alpha:1.0];
  _toolbarView.frame = CGRectMake(0, 20, CGRectGetWidth(bounds), 44);
  _toolbarView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;
  _toolbarView.delegate = self;
  [self.view addSubview:_toolbarView];

  // Set up the container view.
  _containerView = [[UIView alloc] init];
  _containerView.frame =
      CGRectMake(0, 64, CGRectGetWidth(bounds), CGRectGetHeight(bounds) - 64);
  _containerView.backgroundColor = [UIColor lightGrayColor];
  _containerView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self.view addSubview:_containerView];

  // Set up the toolbar buttons.
  UIBarButtonItem* back = [[UIBarButtonItem alloc]
      initWithImage:[UIImage imageNamed:@"toolbar_back"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(back)];
  [back setAccessibilityLabel:kWebShellBackButtonAccessibilityLabel];

  UIBarButtonItem* forward = [[UIBarButtonItem alloc]
      initWithImage:[UIImage imageNamed:@"toolbar_forward"]
              style:UIBarButtonItemStylePlain
             target:self
             action:@selector(forward)];
  [forward setAccessibilityLabel:kWebShellForwardButtonAccessibilityLabel];

  UITextField* field = [[UITextField alloc]
      initWithFrame:CGRectMake(88, 6, CGRectGetWidth([_toolbarView frame]) - 98,
                               31)];
  [field setDelegate:self];
  [field setBackground:[[UIImage imageNamed:@"textfield_background"]
                           resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                           12, 12, 12, 12)]];
  [field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [field setKeyboardType:UIKeyboardTypeWebSearch];
  [field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [field setAccessibilityLabel:kWebShellAddressFieldAccessibilityLabel];
  [field setClearButtonMode:UITextFieldViewModeWhileEditing];
  self.field = field;

  [_toolbarView setItems:@[
    back, forward, [[UIBarButtonItem alloc] initWithCustomView:field]
  ]];

  web::ShellWebClient* client =
      static_cast<web::ShellWebClient*>(web::GetWebClient());
  _browserState = client->browser_state();

  web::WebState::CreateParams webStateCreateParams(_browserState);
  _webState = web::WebState::Create(webStateCreateParams);

  _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
  _webState->AddObserver(_webStateObserver.get());

  _webStateDelegate = std::make_unique<web::WebStateDelegateBridge>(self);
  _webState->SetDelegate(_webStateDelegate.get());

  UIView* view = _webState->GetView();
  [view setFrame:[_containerView bounds]];
  [_containerView addSubview:view];
}

- (NavigationManager*)navigationManager {
  return _webState->GetNavigationManager();
}

- (web::WebState*)webState {
  return _webState.get();
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbarView) {
    return UIBarPositionTopAttached;
  }
  return UIBarPositionAny;
}

- (void)back {
  if (self.navigationManager->CanGoBack()) {
    self.navigationManager->GoBack();
  }
}

- (void)forward {
  if (self.navigationManager->CanGoForward()) {
    self.navigationManager->GoForward();
  }
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  GURL URL = GURL(base::SysNSStringToUTF8([field text]));

  // Do not try to load invalid URLs.
  if (URL.is_valid()) {
    NavigationManager::WebLoadParams params(URL);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    self.navigationManager->LoadURLWithParams(params);
  }

  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  const GURL& visibleURL = _webState->GetVisibleURL();
  [_field setText:base::SysUTF8ToNSString(visibleURL.spec())];
}

// -----------------------------------------------------------------------
#pragma mark Bikeshedding Implementation

// Overridden to allow this view controller to receive motion events by being
// first responder when no other views are.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (void)motionEnded:(UIEventSubtype)motion withEvent:(UIEvent*)event {
  if (event.subtype == UIEventSubtypeMotionShake) {
    [self updateToolbarColor];
  }
}

- (void)updateToolbarColor {
  // Cycle through the following set of colors:
  NSArray* colors = @[
    // Vanilla Blue.
    [UIColor colorWithRed:0.337 green:0.467 blue:0.988 alpha:1.0],
    // Vanilla Red.
    [UIColor colorWithRed:0.898 green:0.110 blue:0.137 alpha:1.0],
    // Blue Grey.
    [UIColor colorWithRed:0.376 green:0.490 blue:0.545 alpha:1.0],
    // Brown.
    [UIColor colorWithRed:0.475 green:0.333 blue:0.282 alpha:1.0],
    // Purple.
    [UIColor colorWithRed:0.612 green:0.153 blue:0.690 alpha:1.0],
    // Teal.
    [UIColor colorWithRed:0.000 green:0.737 blue:0.831 alpha:1.0],
    // Deep Orange.
    [UIColor colorWithRed:1.000 green:0.341 blue:0.133 alpha:1.0],
    // Indigo.
    [UIColor colorWithRed:0.247 green:0.318 blue:0.710 alpha:1.0],
    // Vanilla Green.
    [UIColor colorWithRed:0.145 green:0.608 blue:0.141 alpha:1.0],
    // Pinkerton.
    [UIColor colorWithRed:0.914 green:0.118 blue:0.388 alpha:1.0],
  ];

  NSUInteger currentIndex = [colors indexOfObject:_toolbarView.barTintColor];
  if (currentIndex == NSNotFound) {
    currentIndex = 0;
  }
  NSUInteger newIndex = currentIndex + 1;
  if (newIndex >= [colors count]) {
    // TODO(rohitrao): Out of colors!  Consider prompting the user to pick their
    // own color here.  Also consider allowing the user to choose the entire set
    // of colors or allowing the user to choose color randomization.
    newIndex = 0;
  }
  _toolbarView.barTintColor = [colors objectAtIndex:newIndex];
}

// -----------------------------------------------------------------------
// WebStateObserver implementation.

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateToolbar];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self updateToolbar];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState.get(), webState);
  [self updateToolbar];
}

// -----------------------------------------------------------------------
// WebStateDelegate implementation.

- (void)webState:(web::WebState*)webView
    contextMenuConfigurationForParams:(const web::ContextMenuParams&)params
                    completionHandler:(void (^)(UIContextMenuConfiguration*))
                                          completionHandler {
  GURL link = params.link_url;
  void (^copyHandler)(UIAction*) = ^(UIAction* action) {
    NSDictionary* item = @{
      UTTypeURL.identifier : net::NSURLWithGURL(link),
      UTTypeUTF8PlainText.identifier : [base::SysUTF8ToNSString(link.spec())
          dataUsingEncoding:NSUTF8StringEncoding],
    };
    [[UIPasteboard generalPasteboard] setItems:@[ item ]];
  };

  UIContextMenuConfiguration* configuration = [UIContextMenuConfiguration
      configurationWithIdentifier:nil
      previewProvider:^{
        UIViewController* controller = [[UIViewController alloc] init];
        CGRect frame = CGRectMake(10, 200, 200, 21);
        UILabel* label = [[UILabel alloc] initWithFrame:frame];
        label.text = @"iOS13 Preview Page";
        [controller.view addSubview:label];
        return controller;
      }
      actionProvider:^(id _) {
        NSArray* actions = @[
          [UIAction actionWithTitle:@"Copy Link"
                              image:nil
                         identifier:nil
                            handler:copyHandler],
          [UIAction actionWithTitle:@"Cancel"
                              image:nil
                         identifier:nil
                            handler:^(id ignored){
                            }]
        ];
        NSString* menuTitle = [NSString
            stringWithFormat:@"iOS13 Context Menu: %s", link.spec().c_str()];
        return [UIMenu menuWithTitle:menuTitle children:actions];
      }];

  completionHandler(configuration);
}

- (void)webStateDestroyed:(web::WebState*)webState {
  // The WebState is owned by the current instance, and the observer bridge
  // is unregistered before the WebState is destroyed, so this event should
  // never happen.
  NOTREACHED_IN_MIGRATION();
}

@end
