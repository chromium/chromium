// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#import <UIKit/UIKit.h>

#include "base/containers/contains.h"
#include "base/strings/sys_string_conversions.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/shell.h"
#include "ui/display/screen.h"

@interface ContentShellWindowDelegate : UIViewController <UITextFieldDelegate> {
 @private
  raw_ptr<content::Shell> _shell;
}
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

- (id)initWithShell:(content::Shell*)shell;
- (content::Shell*)shell;
- (void)back;
- (void)forward;
- (void)reloadOrStop;
- (void)setURL:(NSString*)url;
- (void)setContents:(UIView*)content;
@end

@implementation ContentShellWindowDelegate
@synthesize backButton = _backButton;
@synthesize contentView = _contentView;
@synthesize field = _field;
@synthesize forwardButton = _forwardButton;
@synthesize reloadOrStopButton = _reloadOrStopButton;
@synthesize menuButton = _menuButton;
@synthesize headerBackgroundView = _headerBackgroundView;
@synthesize headerContentView = _headerContentView;

- (void)viewDidLoad {
  [super viewDidLoad];

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

  [_forwardButton setImage:[UIImage imageNamed:@"ic_forward"]
                  forState:UIControlStateNormal];
  _forwardButton.tintColor = [UIColor whiteColor];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside];

  [_reloadOrStopButton setImage:[UIImage imageNamed:@"ic_reload"]
                       forState:UIControlStateNormal];
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
  UIView* web_contents_view = _shell->web_contents()->GetNativeView();
  [_contentView addSubview:web_contents_view];
}

- (id)initWithShell:(content::Shell*)shell {
  if ((self = [super init])) {
    _shell = shell;
  }
  return self;
}

- (content::Shell*)shell {
  return _shell;
}

- (void)back {
  _shell->GoBackOrForward(-1);
}

- (void)forward {
  _shell->GoBackOrForward(1);
}

- (void)reloadOrStop {
  if (_shell->web_contents()->IsLoading()) {
    _shell->Stop();
  } else {
    _shell->Reload();
  }
}

- (void)showMainMenu {
}

- (void)setURL:(NSString*)url {
  _field.text = url;
}

- (BOOL)textFieldShouldReturn:(UITextField*)field {
  std::string field_value = base::SysNSStringToUTF8(field.text);
  GURL url(field_value);
  if (!url.has_scheme()) {
    // TODOD(dtapuska): Fix this to URL encode the query.
    std::string search_url = "https://www.google.com/search?q=" + field_value;
    url = GURL(search_url);
  }
  _shell->LoadURL(url);
  return YES;
}

- (void)setContents:(UIView*)content {
  [_contentView addSubview:content];
}

@end

namespace content {

struct ShellPlatformDelegate::ShellData {
  UIWindow* window;
};

struct ShellPlatformDelegate::PlatformData {};

ShellPlatformDelegate::ShellPlatformDelegate() = default;
ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  screen_ = std::make_unique<display::ScopedNativeScreen>();
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  UIWindow* window =
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  window.backgroundColor = [UIColor whiteColor];
  window.tintColor = [UIColor darkGrayColor];

  ContentShellWindowDelegate* controller =
      [[ContentShellWindowDelegate alloc] initWithShell:shell];
  // Gives a restoration identifier so that state restoration works.
  controller.restorationIdentifier = @"rootViewController";
  window.rootViewController = controller;

  shell_data.window = window;
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return shell_data.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  //  ShellData& shell_data = shell_data_map_[shell];

  //  UIView* web_contents_view = shell->web_contents()->GetNativeView();
  //  [((ContentShellWindowDelegate *)shell_data.window.rootViewController)
  //  setContents:web_contents_view];
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  DCHECK(base::Contains(shell_data_map_, shell));
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];
  UIButton* button = nil;
  switch (control) {
    case BACK_BUTTON:
      button = [((ContentShellWindowDelegate*)
                     shell_data.window.rootViewController) backButton];
      break;
    case FORWARD_BUTTON:
      button = [((ContentShellWindowDelegate*)
                     shell_data.window.rootViewController) forwardButton];
      break;
    case STOP_BUTTON: {
      NSString* imageName = is_enabled ? @"ic_stop" : @"ic_reload";
      [[((ContentShellWindowDelegate*)shell_data.window.rootViewController)
          reloadOrStopButton] setImage:[UIImage imageNamed:imageName]
                              forState:UIControlStateNormal];
      break;
    }
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  [button setEnabled:is_enabled];
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  if (Shell::ShouldHideToolbar()) {
    return;
  }
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  [((ContentShellWindowDelegate*)shell_data.window.rootViewController)
      setURL:url_string];
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {
  DCHECK(base::Contains(shell_data_map_, shell));
}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  [shell_data.window resignKeyWindow];
  return true;  // The performClose() will do the destruction of Shell.
}

}  // namespace content
