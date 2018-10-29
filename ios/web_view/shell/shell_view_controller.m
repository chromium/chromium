// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/web_view/shell/shell_autofill_delegate.h"
#import "ios/web_view/shell/shell_translation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Externed accessibility identifier.
NSString* const kWebViewShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebViewShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebViewShellAddressFieldAccessibilityLabel = @"Address field";
NSString* const kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier =
    @"WebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier";

@interface ShellViewController ()<CWVNavigationDelegate,
                                  CWVUIDelegate,
                                  CWVScriptCommandHandler,
                                  UITextFieldDelegate>
// Container for |webView|.
@property(nonatomic, strong) UIView* containerView;
// Text field used for navigating to URLs.
@property(nonatomic, strong) UITextField* field;
// Button to navigate backwards.
@property(nonatomic, strong) UIButton* backButton;
// Button to navigate forwards.
@property(nonatomic, strong) UIButton* forwardButton;
// Toolbar containing navigation buttons and |field|.
@property(nonatomic, strong) UIToolbar* toolbar;
// Handles the autofill of the content displayed in |webView|.
@property(nonatomic, strong) ShellAutofillDelegate* autofillDelegate;
// Handles the translation of the content displayed in |webView|.
@property(nonatomic, strong) ShellTranslationDelegate* translationDelegate;

- (void)back;
- (void)forward;
- (void)stopLoading;
// Disconnects and release the |webView|.
- (void)removeWebView;
// Resets translate settings back to default.
- (void)resetTranslateSettings;
@end

@implementation ShellViewController

@synthesize autofillDelegate = _autofillDelegate;
@synthesize backButton = _backButton;
@synthesize containerView = _containerView;
@synthesize field = _field;
@synthesize forwardButton = _forwardButton;
@synthesize toolbar = _toolbar;
@synthesize webView = _webView;
@synthesize translationDelegate = _translationDelegate;

- (void)viewDidLoad {
  [super viewDidLoad];

  CGRect bounds = self.view.bounds;

  // Set up the toolbar.
  self.toolbar = [[UIToolbar alloc] init];
  [_toolbar setBarTintColor:[UIColor colorWithRed:0.337
                                            green:0.467
                                             blue:0.988
                                            alpha:1.0]];
  [_toolbar setFrame:CGRectMake(0, 20, CGRectGetWidth(bounds), 44)];
  [_toolbar setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleBottomMargin];
  [self.view addSubview:_toolbar];

  // Set up the container view.
  self.containerView = [[UIView alloc] init];
  [_containerView setFrame:CGRectMake(0, 64, CGRectGetWidth(bounds),
                                      CGRectGetHeight(bounds) - 64)];
  [_containerView setBackgroundColor:[UIColor lightGrayColor]];
  [_containerView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                      UIViewAutoresizingFlexibleHeight];
  [self.view addSubview:_containerView];

  const int kButtonCount = 4;
  const CGFloat kButtonSize = 44;

  // Text field.
  self.field = [[UITextField alloc]
      initWithFrame:CGRectMake(kButtonCount * kButtonSize, 6,
                               CGRectGetWidth([_toolbar frame]) -
                                   kButtonCount * kButtonSize - 10,
                               31)];
  [_field setDelegate:self];
  [_field setBackground:[[UIImage imageNamed:@"textfield_background"]
                            resizableImageWithCapInsets:UIEdgeInsetsMake(
                                                            12, 12, 12, 12)]];
  [_field setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
  [_field setKeyboardType:UIKeyboardTypeWebSearch];
  [_field setAutocorrectionType:UITextAutocorrectionTypeNo];
  [_field setClearButtonMode:UITextFieldViewModeWhileEditing];
  [_field setAccessibilityLabel:kWebViewShellAddressFieldAccessibilityLabel];

  // Set up the toolbar buttons.
  self.backButton = [[UIButton alloc] init];
  [_backButton setImage:[UIImage imageNamed:@"toolbar_back"]
               forState:UIControlStateNormal];
  [_backButton addTarget:self
                  action:@selector(back)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton setAccessibilityLabel:kWebViewShellBackButtonAccessibilityLabel];
  [_backButton.widthAnchor constraintEqualToConstant:44].active = YES;

  self.forwardButton = [[UIButton alloc] init];
  [_forwardButton setImage:[UIImage imageNamed:@"toolbar_forward"]
                  forState:UIControlStateNormal];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton
      setAccessibilityLabel:kWebViewShellForwardButtonAccessibilityLabel];
  [_forwardButton.widthAnchor constraintEqualToConstant:44].active = YES;

  UIButton* stopButton = [[UIButton alloc] init];
  [stopButton setImage:[UIImage imageNamed:@"toolbar_stop"]
              forState:UIControlStateNormal];
  [stopButton addTarget:self
                 action:@selector(stopLoading)
       forControlEvents:UIControlEventTouchUpInside];
  [stopButton.widthAnchor constraintEqualToConstant:44].active = YES;

  UIButton* menuButton = [[UIButton alloc] init];
  [menuButton setImage:[UIImage imageNamed:@"toolbar_more_horiz"]
              forState:UIControlStateNormal];
  [menuButton addTarget:self
                 action:@selector(showMenu)
       forControlEvents:UIControlEventTouchUpInside];
  [menuButton.widthAnchor constraintEqualToConstant:44].active = YES;

  [_toolbar setItems:@[
    [[UIBarButtonItem alloc] initWithCustomView:_backButton],
    [[UIBarButtonItem alloc] initWithCustomView:_forwardButton],
    [[UIBarButtonItem alloc] initWithCustomView:stopButton],
    [[UIBarButtonItem alloc] initWithCustomView:menuButton],
    [[UIBarButtonItem alloc] initWithCustomView:_field]
  ]];

  [CWVWebView setUserAgentProduct:@"Dummy/1.0"];

  [self createWebViewWithConfiguration:[CWVWebViewConfiguration
                                           defaultConfiguration]];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"canGoBack"]) {
    _backButton.enabled = [_webView canGoBack];
  } else if ([keyPath isEqualToString:@"canGoForward"]) {
    _forwardButton.enabled = [_webView canGoForward];
  }
}

- (UIBarPosition)positionForBar:(id<UIBarPositioning>)bar {
  if (bar == _toolbar) {
    return UIBarPositionTopAttached;
  }
  return UIBarPositionAny;
}

- (void)back {
  if ([_webView canGoBack]) {
    [_webView goBack];
  }
}

- (void)forward {
  if ([_webView canGoForward]) {
    [_webView goForward];
  }
}

- (void)stopLoading {
  [_webView stopLoading];
}

- (void)showMenu {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  __weak ShellViewController* weakSelf = self;

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Reload"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf.webView reload];
                                       }]];

  // Toggles the incognito mode.
  NSString* incognitoActionTitle = _webView.configuration.persistent
                                       ? @"Enter incognito"
                                       : @"Exit incognito";
  [alertController
      addAction:[UIAlertAction actionWithTitle:incognitoActionTitle
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf toggleIncognito];
                                       }]];

  // Removes the web view from the view hierarchy, releases it, and recreates
  // the web view with the same configuration. This is for testing deallocation
  // and sharing configuration.
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Recreate web view"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              CWVWebViewConfiguration* configuration =
                                  weakSelf.webView.configuration;
                              [weakSelf removeWebView];
                              [weakSelf
                                  createWebViewWithConfiguration:configuration];
                            }]];

  // Resets all translation settings to default values.
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Reset translate settings"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf resetTranslateSettings];
                                       }]];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)resetTranslateSettings {
  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  [configuration.preferences resetTranslationSettings];
}

- (void)toggleIncognito {
  BOOL wasPersistent = _webView.configuration.persistent;
  [self removeWebView];
  CWVWebViewConfiguration* newConfiguration =
      wasPersistent ? [CWVWebViewConfiguration incognitoConfiguration]
                    : [CWVWebViewConfiguration defaultConfiguration];
  [self createWebViewWithConfiguration:newConfiguration];
}

- (void)createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration {
  self.webView = [[CWVWebView alloc] initWithFrame:[_containerView bounds]
                                     configuration:configuration];
  // Gives a restoration identifier so that state restoration works.
  _webView.restorationIdentifier = @"webView";
  _webView.navigationDelegate = self;
  _webView.UIDelegate = self;
  _translationDelegate = [[ShellTranslationDelegate alloc] init];
  _webView.translationController.delegate = _translationDelegate;
  _autofillDelegate = [[ShellAutofillDelegate alloc] init];
  _webView.autofillController.delegate = _autofillDelegate;

  [_webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
  [_containerView addSubview:_webView];

  [_webView addObserver:self
             forKeyPath:@"canGoBack"
                options:NSKeyValueObservingOptionNew
                context:nil];
  [_webView addObserver:self
             forKeyPath:@"canGoForward"
                options:NSKeyValueObservingOptionNew
                context:nil];

  [_webView addScriptCommandHandler:self commandPrefix:@"test"];
}

- (void)removeWebView {
  [_webView removeFromSuperview];
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeScriptCommandHandlerForCommandPrefix:@"test"];

  _webView = nil;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeScriptCommandHandlerForCommandPrefix:@"test"];
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:[field text]]];
  [_webView loadRequest:request];
  [field resignFirstResponder];
  [self updateToolbar];
  return YES;
}

- (void)updateToolbar {
  // Do not update the URL if the text field is currently being edited.
  if ([_field isFirstResponder]) {
    return;
  }

  [_field setText:[[_webView visibleURL] absoluteString]];
}

#pragma mark CWVUIDelegate methods

- (CWVWebView*)webView:(CWVWebView*)webView
    createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration
               forNavigationAction:(CWVNavigationAction*)action {
  NSLog(@"Create new CWVWebView for %@. User initiated? %@", action.request.URL,
        action.userInitiated ? @"Yes" : @"No");
  return nil;
}

- (void)webViewDidClose:(CWVWebView*)webView {
  NSLog(@"webViewDidClose");
}

- (void)webView:(CWVWebView*)webView
    runContextMenuWithTitle:(NSString*)menuTitle
             forHTMLElement:(CWVHTMLElement*)element
                     inView:(UIView*)view
        userGestureLocation:(CGPoint)location {
  if (!element.hyperlink) {
    return;
  }

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:menuTitle
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(location.x, location.y, 1.0, 1.0);

  void (^copyHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    NSDictionary* item = @{
      (NSString*)(kUTTypeURL) : element.hyperlink,
      (NSString*)(kUTTypeUTF8PlainText) : [[element.hyperlink absoluteString]
          dataUsingEncoding:NSUTF8StringEncoding],
    };
    [[UIPasteboard generalPasteboard] setItems:@[ item ]];
  };
  [alert addAction:[UIAlertAction actionWithTitle:@"Copy Link"
                                            style:UIAlertActionStyleDefault
                                          handler:copyHandler]];

  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:nil]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                               pageURL:(NSURL*)URL
                     completionHandler:(void (^)(void))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler();
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                                 pageURL:(NSURL*)URL
                       completionHandler:(void (^)(BOOL))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addAction:[UIAlertAction actionWithTitle:@"Ok"
                                            style:UIAlertActionStyleDefault
                                          handler:^(UIAlertAction* action) {
                                            handler(YES);
                                          }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(NO);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    runJavaScriptTextInputPanelWithPrompt:(NSString*)prompt
                              defaultText:(NSString*)defaultText
                                  pageURL:(NSURL*)URL
                        completionHandler:(void (^)(NSString*))handler {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:nil
                                          message:prompt
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alert addTextFieldWithConfigurationHandler:^(UITextField* textField) {
    textField.text = defaultText;
    textField.accessibilityIdentifier =
        kWebViewShellJavaScriptDialogTextFieldAccessibiltyIdentifier;
  }];

  __weak UIAlertController* weakAlert = alert;
  [alert addAction:[UIAlertAction
                       actionWithTitle:@"Ok"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 NSString* textInput =
                                     weakAlert.textFields.firstObject.text;
                                 handler(textInput);
                               }]];
  [alert addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                            style:UIAlertActionStyleCancel
                                          handler:^(UIAlertAction* action) {
                                            handler(nil);
                                          }]];

  [self presentViewController:alert animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    didLoadFavicons:(NSArray<CWVFavicon*>*)favIcons {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

#pragma mark CWVNavigationDelegate methods

- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(CWVNavigationType)navigationType {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response
                      forMainFrame:(BOOL)forMainFrame {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (void)webViewDidStartProvisionalNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidCommitNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  // TODO(crbug.com/679895): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView
    didFailNavigationWithError:(NSError*)error {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (BOOL)webView:(CWVWebView*)webView
    shouldPreviewElement:(CWVPreviewElementInfo*)elementInfo {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (UIViewController*)webView:(CWVWebView*)webView
    previewingViewControllerForElement:(CWVPreviewElementInfo*)elementInfo {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return nil;
}

- (void)webView:(CWVWebView*)webView
    commitPreviewingViewController:(UIViewController*)previewingViewController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)webView:(CWVWebView*)webView
    didFailNavigationWithSSLError:(NSError*)error
                      overridable:(BOOL)overridable
                  decisionHandler:
                      (void (^)(CWVSSLErrorDecision))decisionHandler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  decisionHandler(CWVSSLErrorDecisionDoNothing);
}

#pragma mark CWVScriptCommandHandler

- (BOOL)webView:(CWVWebView*)webView
    handleScriptCommand:(nonnull CWVScriptCommand*)command
          fromMainFrame:(BOOL)fromMainFrame {
  NSLog(@"%@ command.content=%@", NSStringFromSelector(_cmd), command.content);
  return YES;
}

@end
