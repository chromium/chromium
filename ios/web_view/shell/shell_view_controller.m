// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_view_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "ios/web_view/shell/shell_auth_service.h"
#import "ios/web_view/shell/shell_autofill_delegate.h"
#import "ios/web_view/shell/shell_translation_delegate.h"
#import "ios/web_view/shell/shell_trusted_vault_provider.h"

// Externed accessibility identifier.
NSString* const kWebViewShellBackButtonAccessibilityLabel = @"Back";
NSString* const kWebViewShellForwardButtonAccessibilityLabel = @"Forward";
NSString* const kWebViewShellAddressFieldAccessibilityLabel = @"Address field";
NSString* const kWebViewShellJavaScriptDialogTextFieldAccessibilityIdentifier =
    @"WebViewShellJavaScriptDialogTextFieldAccessibilityIdentifier";

@interface ShellViewController () <CWVAutofillDataManagerObserver,
                                   CWVDownloadTaskDelegate,
                                   CWVLeakCheckServiceObserver,
                                   CWVNavigationDelegate,
                                   CWVUIDelegate,
                                   CWVSyncControllerDelegate,
                                   UIScrollViewDelegate,
                                   UITextFieldDelegate>
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIView* headerBackgroundView;
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIView* headerContentView;
// Button to navigate backwards.
@property(nonatomic, strong) UIButton* backButton;
// Button to navigate forwards.
@property(nonatomic, strong) UIButton* forwardButton;
// Button that either refresh the page or stops the page load.
@property(nonatomic, strong) UIButton* reloadOrStopButton;
// Button that shows the menu
@property(nonatomic, strong) UIButton* menuButton;
// Text field used for navigating to URLs.
@property(nonatomic, strong) UITextField* field;
// Container for |webView|.
@property(nonatomic, strong) UIView* contentView;
// Handles the autofill of the content displayed in |webView|.
@property(nonatomic, strong) ShellAutofillDelegate* autofillDelegate;
// Handles the translation of the content displayed in |webView|.
@property(nonatomic, strong) ShellTranslationDelegate* translationDelegate;
// The on-going download task if any.
@property(nonatomic, strong, nullable) CWVDownloadTask* downloadTask;
// The path to a local file which the download task is writing to.
@property(nonatomic, strong, nullable) NSString* downloadFilePath;
// A controller to show a "Share" menu for the downloaded file.
@property(nonatomic, strong, nullable)
    UIDocumentInteractionController* documentInteractionController;
// Service that provides authentication to ChromeWebView.
@property(nonatomic, strong) ShellAuthService* authService;
// Provides trusted vault functions to ChromeWebView.
@property(nonatomic, strong) ShellTrustedVaultProvider* trustedVaultProvider;
// The newly opened popup windows e.g., by JavaScript function "window.open()",
// HTML "<a target='_blank'>".
@property(nonatomic, strong) NSMutableArray<CWVWebView*>* popupWebViews;
// A list of active leak checks. These map to a list of passwords since
// you can have multiple passwords that map to the same canonical leak check.
@property(nonatomic, strong)
    NSMutableDictionary<CWVLeakCheckCredential*, NSMutableArray<CWVPassword*>*>*
        pendingLeakChecks;

- (void)back;
- (void)forward;
- (void)reloadOrStop;
// Disconnects and release the |webView|.
- (void)removeWebView;
// Resets translate settings back to default.
- (void)resetTranslateSettings;
@end

@implementation ShellViewController

@synthesize autofillDelegate = _autofillDelegate;
@synthesize backButton = _backButton;
@synthesize contentView = _contentView;
@synthesize field = _field;
@synthesize forwardButton = _forwardButton;
@synthesize reloadOrStopButton = _reloadOrStopButton;
@synthesize menuButton = _menuButton;
@synthesize headerBackgroundView = _headerBackgroundView;
@synthesize headerContentView = _headerContentView;
@synthesize webView = _webView;
@synthesize translationDelegate = _translationDelegate;
@synthesize downloadTask = _downloadTask;
@synthesize downloadFilePath = _downloadFilePath;
@synthesize documentInteractionController = _documentInteractionController;
@synthesize authService = _authService;
@synthesize trustedVaultProvider = _trustedVaultProvider;
@synthesize popupWebViews = _popupWebViews;
@synthesize pendingLeakChecks = _pendingLeakChecks;

- (void)viewDidLoad {
  [super viewDidLoad];

  self.popupWebViews = [[NSMutableArray alloc] init];

  // View creation.
  self.headerBackgroundView = [[UIView alloc] init];
  self.headerContentView = [[UIView alloc] init];
  self.contentView = [[UIView alloc] init];
  self.backButton = [[UIButton alloc] init];
  self.forwardButton = [[UIButton alloc] init];
  self.reloadOrStopButton = [[UIButton alloc] init];
  self.menuButton = [[UIButton alloc] init];
  self.field = [[UITextField alloc] init];

  // View hierarchy.
  [self.view addSubview:_headerBackgroundView];
  [self.view addSubview:_contentView];
  [_headerBackgroundView addSubview:_headerContentView];
  [_headerContentView addSubview:_backButton];
  [_headerContentView addSubview:_forwardButton];
  [_headerContentView addSubview:_reloadOrStopButton];
  [_headerContentView addSubview:_menuButton];
  [_headerContentView addSubview:_field];

  // Additional view setup.
  _headerBackgroundView.backgroundColor = [UIColor colorWithRed:66.0 / 255.0
                                                          green:133.0 / 255.0
                                                           blue:244.0 / 255.0
                                                          alpha:1.0];

  [_backButton setImage:[UIImage imageNamed:@"ic_back"]
               forState:UIControlStateNormal];
  _backButton.tintColor = [UIColor whiteColor];
  [_backButton addTarget:self
                  action:@selector(back)
        forControlEvents:UIControlEventTouchUpInside];
  [_backButton addTarget:self
                  action:@selector(logBackStack)
        forControlEvents:UIControlEventTouchDragOutside];
  [_backButton setAccessibilityLabel:kWebViewShellBackButtonAccessibilityLabel];

  [_forwardButton setImage:[UIImage imageNamed:@"ic_forward"]
                  forState:UIControlStateNormal];
  _forwardButton.tintColor = [UIColor whiteColor];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton addTarget:self
                     action:@selector(logForwardStack)
           forControlEvents:UIControlEventTouchDragOutside];
  [_forwardButton
      setAccessibilityLabel:kWebViewShellForwardButtonAccessibilityLabel];

  _reloadOrStopButton.tintColor = [UIColor whiteColor];
  [_reloadOrStopButton addTarget:self
                          action:@selector(reloadOrStop)
                forControlEvents:UIControlEventTouchUpInside];

  _menuButton.tintColor = [UIColor whiteColor];
  [_menuButton setImage:[UIImage imageNamed:@"ic_menu"]
               forState:UIControlStateNormal];
  [_menuButton addTarget:self
                  action:@selector(showMainMenu)
        forControlEvents:UIControlEventTouchUpInside];

  _field.placeholder = @"Search or type URL";
  _field.backgroundColor = [UIColor whiteColor];
  _field.tintColor = _headerBackgroundView.backgroundColor;
  [_field setContentHuggingPriority:UILayoutPriorityDefaultLow - 1
                            forAxis:UILayoutConstraintAxisHorizontal];
  _field.delegate = self;
  _field.layer.cornerRadius = 2.0;
  _field.keyboardType = UIKeyboardTypeWebSearch;
  _field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  _field.clearButtonMode = UITextFieldViewModeWhileEditing;
  _field.autocorrectionType = UITextAutocorrectionTypeNo;
  UIView* spacerView = [[UIView alloc] init];
  spacerView.frame = CGRectMake(0, 0, 8, 8);
  _field.leftViewMode = UITextFieldViewModeAlways;
  _field.leftView = spacerView;

  // Constraints.
  _headerBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_headerBackgroundView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [_headerBackgroundView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerBackgroundView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerBackgroundView.bottomAnchor
        constraintEqualToAnchor:_headerContentView.bottomAnchor],
  ]];

  _headerContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_headerContentView.topAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .topAnchor],
    [_headerContentView.leadingAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .leadingAnchor],
    [_headerContentView.trailingAnchor
        constraintEqualToAnchor:_headerBackgroundView.safeAreaLayoutGuide
                                    .trailingAnchor],
    [_headerContentView.heightAnchor constraintEqualToConstant:56.0],
  ]];

  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_contentView.topAnchor
        constraintEqualToAnchor:_headerBackgroundView.bottomAnchor],
    [_contentView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_contentView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_backButton.leadingAnchor
        constraintEqualToAnchor:_headerContentView.safeAreaLayoutGuide
                                    .leadingAnchor
                       constant:16.0],
    [_backButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _forwardButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_forwardButton.leadingAnchor
        constraintEqualToAnchor:_backButton.trailingAnchor
                       constant:16.0],
    [_forwardButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _reloadOrStopButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_reloadOrStopButton.leadingAnchor
        constraintEqualToAnchor:_forwardButton.trailingAnchor
                       constant:16.0],
    [_reloadOrStopButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];
  _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_menuButton.leadingAnchor
        constraintEqualToAnchor:_reloadOrStopButton.trailingAnchor
                       constant:16.0],
    [_menuButton.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
  ]];

  _field.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_field.leadingAnchor constraintEqualToAnchor:_menuButton.trailingAnchor
                                         constant:16.0],
    [_field.centerYAnchor
        constraintEqualToAnchor:_headerContentView.centerYAnchor],
    [_field.trailingAnchor
        constraintEqualToAnchor:_headerContentView.safeAreaLayoutGuide
                                    .trailingAnchor
                       constant:-16.0],
    [_field.heightAnchor constraintEqualToConstant:32.0],
  ]];

  [CWVWebView setUserAgentProduct:@"Dummy/1.0"];
  CWVWebView.chromeContextMenuEnabled = YES;

  CWVWebView.webInspectorEnabled = YES;

  _authService = [[ShellAuthService alloc] init];
  CWVSyncController.dataSource = _authService;

  _trustedVaultProvider =
      [[ShellTrustedVaultProvider alloc] initWithAuthService:_authService];
  CWVSyncController.trustedVaultProvider = _trustedVaultProvider;

  _pendingLeakChecks = [NSMutableDictionary dictionary];

  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  [configuration.autofillDataManager addObserver:self];
  configuration.syncController.delegate = self;
  [configuration.leakCheckService addObserver:self];
  [configuration.userContentController
      addMessageHandler:^(NSDictionary* payload) {
        NSLog(@"message handler payload received =\n%@", payload);
      }
             forCommand:@"messageHandlerCommand"];
  self.webView = [self createWebViewWithConfiguration:configuration];
}

- (void)applicationFinishedRestoringState {
  [super applicationFinishedRestoringState];

  // The scroll view is reset on state restoration. So the delegate must be
  // reassigned.
  self.webView.scrollView.delegate = self;
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"canGoBack"]) {
    _backButton.enabled = [_webView canGoBack];
  } else if ([keyPath isEqualToString:@"canGoForward"]) {
    _forwardButton.enabled = [_webView canGoForward];
  } else if ([keyPath isEqualToString:@"loading"]) {
    NSString* imageName = _webView.loading ? @"ic_stop" : @"ic_reload";
    [_reloadOrStopButton setImage:[UIImage imageNamed:imageName]
                         forState:UIControlStateNormal];
  }
}

- (void)back {
  if ([_webView canGoBack]) {
    [_webView goBack];
  }
}

- (void)logBackStack {
  if (!_webView.canGoBack) {
    return;
  }
  CWVBackForwardList* list = _webView.backForwardList;
  CWVBackForwardListItemArray* backList = list.backList;
  for (size_t i = 0; i < backList.count; i++) {
    CWVBackForwardListItem* item = backList[i];
    NSLog(@"BackStack Item #%ld: <URL='%@', title='%@'>", i, item.URL,
          item.title);
  }
}

- (void)forward {
  if ([_webView canGoForward]) {
    [_webView goForward];
  }
}

- (void)logForwardStack {
  if (!_webView.canGoForward) {
    return;
  }
  CWVBackForwardList* list = _webView.backForwardList;
  CWVBackForwardListItemArray* forwardList = list.forwardList;
  for (size_t i = 0; i < forwardList.count; i++) {
    CWVBackForwardListItem* item = forwardList[i];
    NSLog(@"ForwardStack Item #%ld: <URL='%@', title='%@'>", i, item.URL,
          item.title);
  }
}

- (void)reloadOrStop {
  if (_webView.loading) {
    [_webView stopLoading];
  } else {
    [_webView reload];
  }
}

- (void)showAddressData {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  [dataManager fetchProfilesWithCompletionHandler:^(
                   NSArray<CWVAutofillProfile*>* _Nonnull profiles) {
    NSMutableArray<NSString*>* descriptions = [profiles
        valueForKey:NSStringFromSelector(@selector(debugDescription))];
    NSString* message = [descriptions componentsJoinedByString:@"\n\n"];
    UIAlertController* alertController = [self actionSheetWithTitle:@"Addresses"
                                                            message:message];
    for (CWVAutofillProfile* profile in profiles) {
      NSString* title = [NSString
          stringWithFormat:@"Delete %@", @([profiles indexOfObject:profile])];
      UIAlertAction* action =
          [UIAlertAction actionWithTitle:title
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction* theAction) {
                                   [dataManager deleteProfile:profile];
                                 }];
      [alertController addAction:action];
    }
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Done"
                                           style:UIAlertActionStyleCancel
                                         handler:nil]];

    [self presentViewController:alertController animated:YES completion:nil];
  }];
}

- (void)showCreditCardData {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  [dataManager fetchCreditCardsWithCompletionHandler:^(
                   NSArray<CWVCreditCard*>* _Nonnull creditCards) {
    NSMutableArray<NSString*>* descriptions = [creditCards
        valueForKey:NSStringFromSelector(@selector(debugDescription))];
    NSString* message = [descriptions componentsJoinedByString:@"\n\n"];
    UIAlertController* alertController =
        [self actionSheetWithTitle:@"Credit cards" message:message];
    __weak ShellViewController* weakSelf = self;
    [alertController
        addAction:[UIAlertAction
                      actionWithTitle:@"Manage Google pay cards"
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* action) {
                                __weak ShellViewController* strongSelf =
                                    weakSelf;
                                NSString* URL;
                                if ([CWVFlags sharedInstance]
                                        .usesSyncAndWalletSandbox) {
                                  URL = @"https://pay.sandbox.google.com/"
                                        @"payments/home#paymentMethods";
                                } else {
                                  URL = @"https://pay.google.com/payments/"
                                        @"home#paymentMethods";
                                }
                                NSURLRequest* request = [NSURLRequest
                                    requestWithURL:[NSURL URLWithString:URL]];
                                [strongSelf.webView loadRequest:request];
                              }]];
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Done"
                                           style:UIAlertActionStyleCancel
                                         handler:nil]];
    [self presentViewController:alertController animated:YES completion:nil];
  }];
}

- (void)showPasswordData {
  __weak ShellViewController* weakSelf = self;
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  [dataManager fetchPasswordsWithCompletionHandler:^(
                   NSArray<CWVPassword*>* _Nonnull passwords) {
    NSMutableArray<NSString*>* descriptions = [NSMutableArray array];
    for (CWVPassword* password in passwords) {
      NSString* description = [NSString
          stringWithFormat:@"%@:\n%@", @([passwords indexOfObject:password]),
                           password.debugDescription];
      [descriptions addObject:description];
    }
    NSString* message = [descriptions componentsJoinedByString:@"\n\n"];

    UIAlertController* alertController = [self actionSheetWithTitle:@"Passwords"
                                                            message:message];
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Add new"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf showAddNewPasswordDialog];
                                         }]];

    for (CWVPassword* password in passwords) {
      NSString* title = [NSString
          stringWithFormat:@"Select %@", @([passwords indexOfObject:password])];
      UIAlertAction* action =
          [UIAlertAction actionWithTitle:title
                                   style:UIAlertActionStyleDefault
                                 handler:^(UIAlertAction* theAction) {
                                   [weakSelf showMenuForPassword:password];
                                 }];
      [alertController addAction:action];
    }
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Done"
                                           style:UIAlertActionStyleCancel
                                         handler:nil]];
    [self presentViewController:alertController animated:YES completion:nil];
  }];
}

- (void)showAddNewPasswordDialog {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Add password"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Username";
      }];
  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Password";
      }];
  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Site";
      }];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  __weak UIAlertController* weakAlertController = alertController;
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Done"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              NSString* username =
                                  weakAlertController.textFields[0].text;
                              NSString* password =
                                  weakAlertController.textFields[1].text;
                              NSString* site =
                                  weakAlertController.textFields[2].text;
                              [dataManager addNewPasswordForUsername:username
                                                            password:password
                                                                site:site];
                            }]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showMenuForPassword:(CWVPassword*)password {
  UIAlertController* alertController =
      [self actionSheetWithTitle:password.title
                         message:password.debugDescription];
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;

  __weak ShellViewController* weakSelf = self;
  UIAlertAction* update =
      [UIAlertAction actionWithTitle:@"Update"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* theAction) {
                               [weakSelf showUpdateDialogForPassword:password];
                             }];
  [alertController addAction:update];

  UIAlertAction* delete =
      [UIAlertAction actionWithTitle:@"Delete"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* theAction) {
                               [dataManager deletePassword:password];
                             }];
  [alertController addAction:delete];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Done"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showUpdateDialogForPassword:(CWVPassword*)password {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:password.title
                                          message:password.debugDescription
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"New username";
      }];
  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"New password";
      }];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  __weak UIAlertController* weakAlertController = alertController;
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Done"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         NSString* newUsername =
                                             weakAlertController.textFields
                                                 .firstObject.text;
                                         NSString* newPassword =
                                             weakAlertController.textFields
                                                 .lastObject.text;
                                         [dataManager
                                             updatePassword:password
                                                newUsername:newUsername
                                                newPassword:newPassword];
                                       }]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showSyncMenu {
  CWVSyncController* syncController = _webView.configuration.syncController;

  NSString* message = [NSString
      stringWithFormat:@"Passphrase required: %@\nTrusted vault keys required: "
                       @"%@\nTrusted vault recoverability degraded: %@",
                       @(syncController.passphraseNeeded),
                       @(syncController.trustedVaultKeysRequired),
                       @(syncController.trustedVaultRecoverabilityDegraded)];
  UIAlertController* alertController = [self actionSheetWithTitle:@"Sync menu"
                                                          message:message];

  CWVIdentity* currentIdentity = syncController.currentIdentity;
  __weak ShellViewController* weakSelf = self;
  if (currentIdentity) {
    NSString* title = [NSString
        stringWithFormat:@"Stop syncing for %@", currentIdentity.email];
    [alertController
        addAction:[UIAlertAction
                      actionWithTitle:title
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* action) {
                                [syncController stopSyncAndClearIdentity];
                              }]];

    if (syncController.passphraseNeeded) {
      [alertController
          addAction:[UIAlertAction
                        actionWithTitle:@"Unlock using passphrase"
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [weakSelf showPassphraseUnlockAlert];
                                }]];
    } else if (syncController.trustedVaultKeysRequired) {
      [alertController
          addAction:[UIAlertAction
                        actionWithTitle:@"Fetch trusted vault keys"
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [weakSelf.trustedVaultProvider
                                      showFetchKeysFlowForIdentity:
                                          currentIdentity
                                                fromViewController:weakSelf];
                                }]];
    } else if (syncController.trustedVaultRecoverabilityDegraded) {
      [alertController
          addAction:
              [UIAlertAction
                  actionWithTitle:@"Fix degraded recoverability"
                            style:UIAlertActionStyleDefault
                          handler:^(UIAlertAction* action) {
                            [weakSelf.trustedVaultProvider
                                showFixDegradedRecoverabilityFlowForIdentity:
                                    currentIdentity
                                                          fromViewController:
                                                              weakSelf];
                          }]];
    }
  } else {
    for (CWVIdentity* identity in [_authService identities]) {
      NSString* title =
          [NSString stringWithFormat:@"Start sync with %@", identity.email];
      [alertController
          addAction:[UIAlertAction
                        actionWithTitle:title
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [syncController
                                      startSyncWithIdentity:identity];
                                }]];
    }

    NSString* sandboxTitle = [CWVFlags sharedInstance].usesSyncAndWalletSandbox
                                 ? @"Use production sync/wallet"
                                 : @"Use sandbox sync/wallet";
    [alertController
        addAction:[UIAlertAction actionWithTitle:sandboxTitle
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [CWVFlags sharedInstance]
                                               .usesSyncAndWalletSandbox ^= YES;
                                         }]];
  }
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Show autofill data"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showAddressData];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Show credit card data"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showCreditCardData];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Show password data"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showPasswordData];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Check leaked passwords"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf checkLeakedPasswords];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Check weak passwords"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf checkWeakPasswords];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Check reused passwords"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf checkReusedPasswords];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showPassphraseUnlockAlert {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"Unlock sync"
                                          message:@"Enter passphrase"
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak UIAlertController* weakAlertController = alertController;
  CWVSyncController* syncController = _webView.configuration.syncController;
  UIAlertAction* submit = [UIAlertAction
      actionWithTitle:@"Unlock"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                UITextField* textField =
                    weakAlertController.textFields.firstObject;
                NSString* passphrase = textField.text;
                BOOL result = [syncController unlockWithPassphrase:passphrase];
                NSLog(@"Sync passphrase unlock result: %d", result);
              }];

  [alertController addAction:submit];

  UIAlertAction* cancel =
      [UIAlertAction actionWithTitle:@"Cancel"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:cancel];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"passphrase";
        textField.keyboardType = UIKeyboardTypeDefault;
      }];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showMainMenu {
  UIAlertController* alertController = [self actionSheetWithTitle:@"Main menu"
                                                          message:nil];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  __weak ShellViewController* weakSelf = self;

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
                              weakSelf.webView = [weakSelf
                                  createWebViewWithConfiguration:configuration];
                            }]];

  // Developers can choose to use system or Chrome context menu in the shell
  // app. This will also recreate the web view.
  BOOL chromeContextMenuEnabled = CWVWebView.chromeContextMenuEnabled;
  NSString* contextMenuSwitchActionTitle = [NSString
      stringWithFormat:@"%@ Chrome context menu",
                       chromeContextMenuEnabled ? @"Disable" : @"Enable"];
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:contextMenuSwitchActionTitle
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              CWVWebView.chromeContextMenuEnabled =
                                  !chromeContextMenuEnabled;
                              NSLog(@"Chrome context menu is %@ now.",
                                    !chromeContextMenuEnabled ? @"OFF" : @"ON");
                              CWVWebViewConfiguration* configuration =
                                  weakSelf.webView.configuration;
                              [weakSelf removeWebView];
                              weakSelf.field.text = @"";
                              weakSelf.webView = [weakSelf
                                  createWebViewWithConfiguration:configuration];
                            }]];

  // Resets all translation settings to default values.
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Reset translate settings"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf resetTranslateSettings];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Request translation offer"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf requestTranslationOffer];
                                       }]];

  // Shows sync menu.
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Sync menu"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showSyncMenu];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Show Certificate Details"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showCertificateDetails];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Evaluate JavaScript"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showEvaluateJavaScriptUI];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"User Scripts"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf showUserScriptsUI];
                                       }]];

  if (self.downloadTask) {
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Cancel download"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf.downloadTask cancel];
                                         }]];
  }

  // Using native FiP through is only available for 16.0, otherwise fallback
  // to iGA JS FiP.
  if (@available(iOS 16.0, *)) {
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Start Find In Page"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf startFindInPageSession];
                                         }]];
  }

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)showCertificateDetails {
  CWVX509Certificate* certificate = [[_webView visibleSSLStatus] certificate];
  NSString* message;

  if (certificate) {
    message = [NSString stringWithFormat:@"Issuer: %@\nExpires: %@",
                                         certificate.issuerDisplayName,
                                         certificate.validExpiry];
  } else {
    message = @"No Certificate";
  }

  UIAlertController* alertController =
      [self actionSheetWithTitle:@"Certificate Details" message:message];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Done"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)checkLeakedPasswords {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;

  // Request a check for any password in autofill data manager that is not
  // currently being requested.
  [dataManager fetchPasswordsWithCompletionHandler:^(
                   NSArray<CWVPassword*>* _Nonnull passwords) {
    NSMutableArray<CWVLeakCheckCredential*>* credentialsToCheck =
        [NSMutableArray array];
    for (CWVPassword* password in passwords) {
      CWVLeakCheckCredential* credential = [CWVLeakCheckCredential
          canonicalLeakCheckCredentialWithPassword:password];
      NSMutableArray<CWVPassword*>* passwordsForCredential =
          self.pendingLeakChecks[credential];
      if (!passwordsForCredential) {
        passwordsForCredential = [NSMutableArray array];
        self.pendingLeakChecks[credential] = passwordsForCredential;
        [credentialsToCheck addObject:credential];
      }
      [passwordsForCredential addObject:password];
    }

    NSLog(@"Checking leaks for %@ credentials.", @(credentialsToCheck.count));
    [self.webView.configuration.leakCheckService
        checkCredentials:credentialsToCheck];
  }];
}

- (void)checkWeakPasswords {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  [dataManager fetchPasswordsWithCompletionHandler:^(
                   NSArray<CWVPassword*>* _Nonnull passwords) {
    NSLog(@"Checking weak status of %@ passwords.", @(passwords.count));
    for (CWVPassword* password in passwords) {
      dispatch_async(dispatch_get_main_queue(), ^{
        BOOL isWeak = [CWVWeakCheckUtils isPasswordWeak:password.password];
        NSLog(@"Weak password status for %@ at %@ is %s", password.username,
              password.site, isWeak ? "true" : "false");
      });
    }
  }];
}

- (void)checkReusedPasswords {
  CWVAutofillDataManager* dataManager =
      _webView.configuration.autofillDataManager;
  [dataManager fetchPasswordsWithCompletionHandler:^(
                   NSArray<CWVPassword*>* _Nonnull passwords) {
    NSLog(@"Checking reuse status of %@ passwords.", @(passwords.count));
    [self.webView.configuration.reuseCheckService
        checkReusedPasswords:passwords
           completionHandler:^(NSSet<NSString*>* reusedPasswords) {
             int useCount = 0;
             for (CWVPassword* password in passwords) {
               if ([reusedPasswords containsObject:password.password]) {
                 useCount++;
               }
             }
             NSLog(@"%@ reused password(s).", @(reusedPasswords.count));
             NSLog(@"Used %d times.", useCount);

             UIAlertController* alertController = [UIAlertController
                 alertControllerWithTitle:@"Reuse Check Complete"
                                  message:[NSString
                                              stringWithFormat:
                                                  @"%@ reused password(s). "
                                                  @"Used %d times.",
                                                  @(reusedPasswords.count),
                                                  useCount]
                           preferredStyle:UIAlertControllerStyleAlert];
             [alertController
                 addAction:[UIAlertAction
                               actionWithTitle:@"OK"
                                         style:UIAlertActionStyleDefault
                                       handler:nil]];
             [self presentViewController:alertController
                                animated:YES
                              completion:nil];
           }];
  }];
}

- (void)showEvaluateJavaScriptUI {
  UIAlertController* alertController =
      [self alertControllerWithTitle:@"Evaluate JavaScript"
                             message:nil
                      preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"alert('Hello')";
      }];

  __weak UIAlertController* weakAlertController = alertController;
  __weak ShellViewController* weakSelf = self;
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Evaluate"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              NSString* javascript =
                                  weakAlertController.textFields[0].text;
                              [weakSelf evaluateJavaScript:javascript];
                            }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)evaluateJavaScript:(NSString*)javascript {
  [self.webView
      evaluateJavaScript:javascript
              completion:^(id result, NSError* error) {
                if (error) {
                  NSLog(
                      @"JavaScript evaluation FAILED with error: %@ result: %@",
                      error, result);
                } else {
                  NSLog(@"JavaScript evaluation finished with result: %@",
                        result);
                }
              }];
}

- (void)showUserScriptsUI {
  UIAlertController* alertController =
      [self alertControllerWithTitle:@"Add or Remove User Scripts"
                             message:@"This will also recreate the web view."
                      preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"All frames script";
      }];

  [alertController
      addTextFieldWithConfigurationHandler:^(UITextField* textField) {
        textField.placeholder = @"Main frame script";
      }];

  __weak UIAlertController* weakAlertController = alertController;
  __weak ShellViewController* weakSelf = self;
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Add"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              NSString* allFramesSource =
                                  weakAlertController.textFields[0].text;
                              NSString* mainFrameSource =
                                  weakAlertController.textFields[1].text;
                              [weakSelf
                                  addUserScriptForAllFrames:allFramesSource
                                           forMainFrameOnly:mainFrameSource];
                            }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Remove All"
                                         style:UIAlertActionStyleDestructive
                                       handler:^(UIAlertAction* action) {
                                         [weakSelf removeAllUserScripts];
                                       }]];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)addUserScriptForAllFrames:(nullable NSString*)allFramesSource
                 forMainFrameOnly:(nullable NSString*)mainFrameSource {
  CWVWebViewConfiguration* configuration = self.webView.configuration;
  [self removeWebView];
  if (allFramesSource.length) {
    CWVUserScript* allFramesScript =
        [[CWVUserScript alloc] initWithSource:allFramesSource
                             forMainFrameOnly:NO];
    [configuration.userContentController addUserScript:allFramesScript];
  }
  if (mainFrameSource.length) {
    CWVUserScript* mainFrameScript =
        [[CWVUserScript alloc] initWithSource:mainFrameSource
                             forMainFrameOnly:YES];
    [configuration.userContentController addUserScript:mainFrameScript];
  }
  self.webView = [self createWebViewWithConfiguration:configuration];
}

- (void)removeAllUserScripts {
  CWVWebViewConfiguration* configuration = self.webView.configuration;
  [self removeWebView];
  [configuration.userContentController removeAllUserScripts];
  self.webView = [self createWebViewWithConfiguration:configuration];
}

- (void)resetTranslateSettings {
  CWVWebViewConfiguration* configuration =
      [CWVWebViewConfiguration defaultConfiguration];
  [configuration.preferences resetTranslationSettings];
}

- (void)requestTranslationOffer {
  BOOL offered = [_webView.translationController requestTranslationOffer];
  NSLog(@"Manual translation was offered: %d", offered);
}

- (void)toggleIncognito {
  BOOL wasPersistent = _webView.configuration.persistent;
  [self removeWebView];
  CWVWebViewConfiguration* newConfiguration =
      wasPersistent ? [CWVWebViewConfiguration nonPersistentConfiguration]
                    : [CWVWebViewConfiguration defaultConfiguration];
  self.webView = [self createWebViewWithConfiguration:newConfiguration];
}

- (void)startFindInPageSession {
  // Using native FiP through is only available for 16.0, otherwise fallback
  // to iGA JS FiP.
  if (@available(iOS 16.0, *)) {
    if ([_webView.findInPageController canFindInPage]) {
      [_webView.findInPageController startFindInPage];
    }
  }
}

- (CWVWebView*)createWebViewWithConfiguration:
    (CWVWebViewConfiguration*)configuration {
  // Set a non empty CGRect to avoid DCHECKs that occur when a load happens
  // after state restoration, and before the view hierarchy is laid out for the
  // first time.
  // https://source.chromium.org/chromium/chromium/src/+/main:ios/web/web_state/ui/crw_web_request_controller.mm;l=518;drc=df887034106ef438611326745a7cd276eedd4953
  CGRect frame = CGRectMake(0, 0, 1, 1);
  CWVWebView* webView = [[CWVWebView alloc] initWithFrame:frame
                                            configuration:configuration];
  [_contentView addSubview:webView];

  // Gives a restoration identifier so that state restoration works.
  webView.restorationIdentifier = @"webView";

  // Configure delegates.
  webView.navigationDelegate = self;
  webView.UIDelegate = self;
  _translationDelegate = [[ShellTranslationDelegate alloc] init];
  webView.translationController.delegate = _translationDelegate;
  _autofillDelegate = [[ShellAutofillDelegate alloc] init];
  webView.autofillController.delegate = _autofillDelegate;
  webView.scrollView.delegate = self;

  // Constraints.
  webView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [webView.topAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.topAnchor],
    [webView.leadingAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.leadingAnchor],
    [webView.trailingAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide
                                    .trailingAnchor],
    [webView.bottomAnchor
        constraintEqualToAnchor:_contentView.safeAreaLayoutGuide.bottomAnchor],
  ]];

  [webView addObserver:self
            forKeyPath:@"canGoBack"
               options:NSKeyValueObservingOptionNew |
                       NSKeyValueObservingOptionInitial
               context:nil];
  [webView addObserver:self
            forKeyPath:@"canGoForward"
               options:NSKeyValueObservingOptionNew |
                       NSKeyValueObservingOptionInitial
               context:nil];
  [webView addObserver:self
            forKeyPath:@"loading"
               options:NSKeyValueObservingOptionNew |
                       NSKeyValueObservingOptionInitial
               context:nil];

  [webView
      addMessageHandler:^(NSDictionary* payload) {
        NSLog(@"webview message handler payload received =\n%@", payload);
      }
             forCommand:@"webViewMessageHandlerCommand"];

  return webView;
}

- (void)removeWebView {
  [_webView removeFromSuperview];
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeObserver:self forKeyPath:@"loading"];
  [_webView removeMessageHandlerForCommand:@"webViewMessageHandlerCommand"];

  _webView = nil;
}

- (void)dealloc {
  [_webView removeObserver:self forKeyPath:@"canGoBack"];
  [_webView removeObserver:self forKeyPath:@"canGoForward"];
  [_webView removeObserver:self forKeyPath:@"loading"];
  [_webView removeMessageHandlerForCommand:@"webViewMessageHandlerCommand"];
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  CWVOmniboxInput* input = [[CWVOmniboxInput alloc] initWithText:field.text
                                   shouldUseHTTPSAsDefaultScheme:NO];
  NSURL* URL = input.URL;
  if (input.type != CWVOmniboxInputTypeURL) {
    NSString* enteredText = field.text;
    enteredText =
        [enteredText stringByAddingPercentEncodingWithAllowedCharacters:
                         [NSCharacterSet URLQueryAllowedCharacterSet]];
    enteredText = [NSString
        stringWithFormat:@"https://www.google.com/search?q=%@", enteredText];
    URL = [NSURL URLWithString:enteredText];
  }
  NSURLRequest* request = [NSURLRequest requestWithURL:URL];
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

- (UIAlertController*)actionSheetWithTitle:(nullable NSString*)title
                                   message:(nullable NSString*)message {
  return [self alertControllerWithTitle:title
                                message:message
                         preferredStyle:UIAlertControllerStyleActionSheet];
}

- (UIAlertController*)alertControllerWithTitle:(nullable NSString*)title
                                       message:(nullable NSString*)message
                                preferredStyle:
                                    (UIAlertControllerStyle)preferredStyle {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:preferredStyle];
  alertController.popoverPresentationController.sourceView = _menuButton;
  alertController.popoverPresentationController.sourceRect =
      CGRectMake(CGRectGetWidth(_menuButton.bounds) / 2,
                 CGRectGetHeight(_menuButton.bounds), 1, 1);
  return alertController;
}

- (void)closePopupWebView {
  if (self.popupWebViews.count) {
    [self.popupWebViews.lastObject removeFromSuperview];
    [self.popupWebViews removeLastObject];
  }
}

#pragma mark CWVUIDelegate methods

- (CWVWebView*)webView:(CWVWebView*)webView
    createWebViewWithConfiguration:(CWVWebViewConfiguration*)configuration
               forNavigationAction:(CWVNavigationAction*)action {
  NSLog(@"Create new CWVWebView for %@. User initiated? %@", action.request.URL,
        action.userInitiated ? @"Yes" : @"No");

  CWVWebView* newWebView = [self createWebViewWithConfiguration:configuration];
  [self.popupWebViews addObject:newWebView];

  UIButton* closeWindowButton = [[UIButton alloc] init];
  [closeWindowButton setImage:[UIImage imageNamed:@"ic_stop"]
                     forState:UIControlStateNormal];
  closeWindowButton.tintColor = [UIColor blackColor];
  closeWindowButton.backgroundColor = [UIColor whiteColor];
  [closeWindowButton addTarget:self
                        action:@selector(closePopupWebView)
              forControlEvents:UIControlEventTouchUpInside];

  [newWebView addSubview:closeWindowButton];
  closeWindowButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [closeWindowButton.topAnchor constraintEqualToAnchor:newWebView.topAnchor
                                                constant:16.0],
    [closeWindowButton.centerXAnchor
        constraintEqualToAnchor:newWebView.centerXAnchor],
  ]];

  return newWebView;
}

- (void)webViewDidClose:(CWVWebView*)webView {
  NSLog(@"webViewDidClose");
}

- (void)webView:(CWVWebView*)webView
    requestMediaCapturePermissionForType:(CWVMediaCaptureType)type
                         decisionHandler:
                             (void (^)(CWVPermissionDecision decision))
                                 decisionHandler {
  NSString* mediaCaptureType;
  switch (type) {
    case CWVMediaCaptureTypeCamera:
      mediaCaptureType = @"Camera";
      break;
    case CWVMediaCaptureTypeMicrophone:
      mediaCaptureType = @"Microphone";
      break;
    case CWVMediaCaptureTypeCameraAndMicrophone:
      mediaCaptureType = @"Camera and Microphone";
      break;
  }

  NSString* title =
      [NSString stringWithFormat:@"Request %@ Permission", mediaCaptureType];
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];

  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Grant"
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              decisionHandler(CWVPermissionDecisionGrant);
                            }]];
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:@"Deny"
                              style:UIAlertActionStyleDestructive
                            handler:^(UIAlertAction* action) {
                              decisionHandler(CWVPermissionDecisionDeny);
                            }]];

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    contextMenuConfigurationForElement:(CWVHTMLElement*)element
                     completionHandler:(void (^)(UIContextMenuConfiguration*))
                                           completionHandler {
  void (^copyHandler)(UIAction*) = ^(UIAction* action) {
    NSDictionary* item = @{
      (NSString*)(UTTypeURL) : element.hyperlink.absoluteString,
      (NSString*)(UTTypeUTF8PlainText) : [element.hyperlink.absoluteString
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
                            handler:^(id ignore){
                            }]
        ];
        NSString* menuTitle =
            [NSString stringWithFormat:@"iOS13 Context Menu: %@",
                                       element.hyperlink.absoluteString];
        return [UIMenu menuWithTitle:menuTitle children:actions];
      }];

  completionHandler(configuration);
}

- (void)webView:(CWVWebView*)webView
    contextMenuWillCommitWithAnimator:
        (id<UIContextMenuInteractionCommitAnimating>)animator {
  NSLog(@"webView:contextMenuWillCommitWithAnimator:");
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
        kWebViewShellJavaScriptDialogTextFieldAccessibilityIdentifier;
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

// Deprecated: this method will not work when `-[CWVNavigationDelegate
// webView:decidePolicyForNavigationAction:decisionHandler:]` is implemented
- (BOOL)webView:(CWVWebView*)webView
    shouldStartLoadWithRequest:(NSURLRequest*)request
                navigationType:(CWVNavigationType)navigationType {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

// Deprecated: this method will not work when `-[CWVNavigationDelegate
// webView:decidePolicyForNavigationResponse:decisionHandler:]` is implemented
- (BOOL)webView:(CWVWebView*)webView
    shouldContinueLoadWithResponse:(NSURLResponse*)response
                      forMainFrame:(BOOL)forMainFrame {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  return YES;
}

- (void)webView:(CWVWebView*)webView
    decidePolicyForNavigationAction:(CWVNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(CWVNavigationActionPolicy))decisionHandler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  decisionHandler(CWVNavigationActionPolicyAllow);
}

- (void)webView:(CWVWebView*)webView
    decidePolicyForNavigationResponse:(CWVNavigationResponse*)navigationResponse
                      decisionHandler:(void (^)(CWVNavigationResponsePolicy))
                                          decisionHandler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  decisionHandler(CWVNavigationResponsePolicyAllow);
}

- (void)webViewDidStartNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidCommitNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webViewDidFinishNavigation:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  // TODO(crbug.com/41294395): Add some visual indication that the page load has
  // finished.
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView
    didFailNavigationWithError:(NSError*)error {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [self updateToolbar];
}

- (void)webView:(CWVWebView*)webView
    handleSSLErrorWithHandler:(CWVSSLErrorHandler*)handler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  [handler displayErrorPageWithHTML:handler.error.localizedDescription];

  if (!handler.overridable) {
    return;
  }

  UIAlertController* alertController =
      [self actionSheetWithTitle:@"SSL error encountered"
                         message:@"Would you like to continue anyways?"];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Yes"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [handler overrideErrorAndReloadPage];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    handleLookalikeURLWithHandler:(CWVLookalikeURLHandler*)handler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  NSString* html =
      [NSString stringWithFormat:@"%@ requested, did you mean %@?",
                                 handler.requestURL, handler.safeURL];
  [handler displayInterstitialPageWithHTML:html];

  UIAlertController* alertController =
      [self actionSheetWithTitle:@"Lookalike URL encountered"
                         message:@"Choose how to proceed."];
  [alertController
      addAction:
          [UIAlertAction
              actionWithTitle:@"Proceed to request URL"
                        style:UIAlertActionStyleDefault
                      handler:^(UIAlertAction* action) {
                        CWVLookalikeURLHandlerDecision decision =
                            CWVLookalikeURLHandlerDecisionProceedToRequestURL;
                        [handler commitDecision:decision];
                      }]];
  [alertController
      addAction:
          [UIAlertAction
              actionWithTitle:@"Proceed to safe URL"
                        style:UIAlertActionStyleDefault
                      handler:^(UIAlertAction* action) {
                        [handler
                            commitDecision:
                                CWVLookalikeURLHandlerDecisionProceedToSafeURL];
                      }]];
  [alertController
      addAction:
          [UIAlertAction
              actionWithTitle:@"Go back or close"
                        style:UIAlertActionStyleDefault
                      handler:^(UIAlertAction* action) {
                        [handler
                            commitDecision:
                                CWVLookalikeURLHandlerDecisionGoBackOrClose];
                      }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)webView:(CWVWebView*)webView
    handleUnsafeURLWithHandler:(CWVUnsafeURLHandler*)handler {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  NSString* html =
      [NSString stringWithFormat:@"%@ requested %@ which might be unsafe.",
                                 handler.mainFrameURL, handler.requestURL];
  [handler displayInterstitialPageWithHTML:html];

  UIAlertController* alertController =
      [self actionSheetWithTitle:@"Unsafe URL encountered"
                         message:@"Choose how to proceed."];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Proceed"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [handler proceed];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Go back or close"
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction* action) {
                                         [handler goBack];
                                       }]];
  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];
  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)webViewWebContentProcessDidTerminate:(CWVWebView*)webView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)webView:(CWVWebView*)webView
    didRequestDownloadWithTask:(CWVDownloadTask*)task {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  self.downloadTask = task;
  NSString* documentDirectoryPath = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES)[0];
  self.downloadFilePath = [documentDirectoryPath
      stringByAppendingPathComponent:task.suggestedFileName];
  task.delegate = self;
  [task startDownloadToLocalFileAtPath:self.downloadFilePath];
}

#pragma mark CWVAutofillDataManagerObserver

- (void)autofillDataManagerDataDidChange:
    (CWVAutofillDataManager*)autofillDataManager {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)autofillDataManager:(CWVAutofillDataManager*)autofillDataManager
    didChangePasswordsByAdding:(NSArray<CWVPassword*>*)added
                      updating:(NSArray<CWVPassword*>*)updated
                      removing:(NSArray<CWVPassword*>*)removed {
  NSLog(@"%@: added %@, updated %@, and removed %@ passwords",
        NSStringFromSelector(_cmd), @(added.count), @(updated.count),
        @(removed.count));
}

#pragma mark CWVDownloadTaskDelegate

- (void)downloadTask:(CWVDownloadTask*)downloadTask
    didFinishWithError:(nullable NSError*)error {
  NSLog(@"%@", NSStringFromSelector(_cmd));
  if (!error) {
    NSURL* url = [NSURL fileURLWithPath:self.downloadFilePath];
    self.documentInteractionController =
        [UIDocumentInteractionController interactionControllerWithURL:url];
    [self.documentInteractionController presentOptionsMenuFromRect:CGRectZero
                                                            inView:self.view
                                                          animated:YES];
  }
  self.downloadTask = nil;
  self.downloadFilePath = nil;
}

- (void)downloadTaskProgressDidChange:(CWVDownloadTask*)downloadTask {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

#pragma mark CWVSyncControllerDelegate

- (void)syncControllerDidStartSync:(CWVSyncController*)syncController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)syncController:(CWVSyncController*)syncController
      didFailWithError:(NSError*)error {
  NSLog(@"%@:%@", NSStringFromSelector(_cmd), error);
}

- (void)syncControllerDidStopSync:(CWVSyncController*)syncController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

- (void)syncControllerDidUpdateState:(CWVSyncController*)syncController {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

#pragma mark CWVLeakCheckServiceObserver

- (void)leakCheckServiceDidChangeState:(CWVLeakCheckService*)leakCheckService {
  NSLog(@"%@:%d", NSStringFromSelector(_cmd), (int)leakCheckService.state);
  if (leakCheckService.state != CWVLeakCheckServiceStateRunning) {
    [self.pendingLeakChecks removeAllObjects];
  }
}

- (void)leakCheckService:(CWVLeakCheckService*)leakCheckService
      didCheckCredential:(CWVLeakCheckCredential*)credential
                isLeaked:(BOOL)isLeaked {
  NSMutableArray<CWVPassword*>* passwordsForCredential =
      [self.pendingLeakChecks objectForKey:credential];
  if (!passwordsForCredential) {
    NSLog(@"No passwords for CWVLeakCheckCredential!");
    return;
  }

  [self.pendingLeakChecks removeObjectForKey:credential];

  NSMutableArray<NSString*>* passwordDescriptions = [passwordsForCredential
      valueForKey:NSStringFromSelector(@selector(debugDescription))];
  NSString* passwordsDescription =
      [passwordDescriptions componentsJoinedByString:@"\n\n"];
  NSString* message = [NSString
      stringWithFormat:@"Leak check returned %@ for %@ passwords:\n%@",
                       isLeaked ? @"LEAKED" : @"OK",
                       @(passwordsForCredential.count), passwordsDescription];
  NSLog(@"%@", message);
  NSLog(@"%@ Leak checks remaining...", @(self.pendingLeakChecks.count));
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  NSLog(@"%@", NSStringFromSelector(_cmd));
}

@end
