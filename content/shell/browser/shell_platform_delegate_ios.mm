// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#import <UIKit/UIKit.h>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_config.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/color_chooser/shell_color_chooser_ios.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_file_select_helper.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"

namespace {

static const char kGraphicsTracingCategories[] =
    "-*,blink,cc,gpu,renderer.scheduler,sequence_manager,v8,toplevel,viz,evdev,"
    "input,benchmark";

static const char kDetailedGraphicsTracingCategories[] =
    "-*,blink,cc,gpu,renderer.scheduler,sequence_manager,v8,toplevel,viz,evdev,"
    "input,benchmark,disabled-by-default-skia,disabled-by-default-skia.gpu,"
    "disabled-by-default-skia.gpu.cache,disabled-by-default-skia.shaders,"
    "disabled-by-default-gpu.dawn,disabled-by-default-gpu.graphite.dawn";

static const char kNavigationTracingCategories[] =
    "-*,benchmark,toplevel,ipc,base,browser,navigation,omnibox,ui,shutdown,"
    "safe_browsing,loading,startup,mojom,renderer_host,"
    "disabled-by-default-system_stats,disabled-by-default-cpu_profiler,dwrite,"
    "fonts,ServiceWorker,passwords,disabled-by-default-file,sql,"
    "disabled-by-default-user_action_samples,disk_cache";

static const char kAllTracingCategories[] = "*";

}  // namespace

@interface TracingHandler : NSObject {
 @private
  std::unique_ptr<perfetto::TracingSession> _tracingSession;
  NSFileHandle* _traceFileHandle;
}

- (void)startWithHandler:(void (^)())startHandler
             stopHandler:(void (^)())stopHandler
              categories:(const char*)categories;
- (void)stop;
- (BOOL)isTracing;

@end

@interface ContentShellWindowDelegate : UIViewController <UITextFieldDelegate> {
 @private
  raw_ptr<content::Shell> _shell;
}
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIStackView* headerBackgroundView;
// Header containing navigation buttons and |field|.
@property(nonatomic, strong) UIStackView* headerContentView;
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
// Manages tracing and tracing state.
@property(nonatomic, strong) TracingHandler* tracingHandler;

+ (UIColor*)backgroundColorDefault;
+ (UIColor*)backgroundColorTracing;
- (id)initWithShell:(content::Shell*)shell;
- (content::Shell*)shell;
- (void)back;
- (void)forward;
- (void)reloadOrStop;
- (void)setURL:(NSString*)url;
- (void)setContents:(UIView*)content;
- (void)stopTracing;
- (void)startTracingWithCategories:(const char*)categories;
- (UIAlertController*)actionSheetWithTitle:(nullable NSString*)title
                                   message:(nullable NSString*)message;
- (void)voiceOverStatusDidChange;
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
@synthesize tracingHandler = _tracingHandler;
std::unique_ptr<content::ScopedAccessibilityMode> _scoped_accessibility_mode;

+ (UIColor*)backgroundColorDefault {
  return [UIColor colorWithRed:66.0 / 255.0
                         green:133.0 / 255.0
                          blue:244.0 / 255.0
                         alpha:1.0];
}

+ (UIColor*)backgroundColorTracing {
  return [UIColor colorWithRed:234.0 / 255.0
                         green:67.0 / 255.0
                          blue:53.0 / 255.0
                         alpha:1.0];
}

#if BUILDFLAG(IS_IOS_TVOS)
// The following methods handle tvOS's focus engine by implementing the
// following behavior:
//
// 1. The content view is focused and receives user input by default.
// 2. Pressing the Menu button in the remote control switches focus to
//    `_headerContentView` so that users can use the toolbar and the location
//    bar.
// 3. Pressing the Menu button again after that will switch to the home screen,
//    and swiping down to focus the content view will reset the behavior
//    described in 1).
- (void)pressesBegan:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    if (press.type == UIPressTypeMenu) {
      if (_shell->web_contents()->GetContentNativeView().Get().focused) {
        _headerContentView.userInteractionEnabled = YES;
        [self setNeedsFocusUpdate];
        return;
      }
    }
  }
  [super pressesBegan:presses withEvent:event];
}

- (void)didUpdateFocusInContext:(UIFocusUpdateContext*)context
       withAnimationCoordinator:(UIFocusAnimationCoordinator*)coordinator {
  if (_shell) {
    const UIView* native_web_contents_view =
        _shell->web_contents()->GetContentNativeView().Get();
    if (context.nextFocusedView == native_web_contents_view) {
      _headerContentView.userInteractionEnabled = NO;
      _shell->web_contents()->Focus();
    }
  }
}

- (NSArray<id<UIFocusEnvironment>>*)preferredFocusEnvironments {
  // `userInteractionEnabled` is false when we create `_headerContentView` so
  // that we focus on `_contentView` by default instead of the Back button in
  // the toolbar.
  // We set it to true when explicitly pressing the Back button on the remote
  // control in order to focus the toolbar.
  return _headerContentView.userInteractionEnabled ? @[ _headerContentView ]
                                                   : @[ _contentView ];
}
#endif

- (void)viewDidLoad {
  [super viewDidLoad];

  // View creation.
  self.headerBackgroundView = [[UIStackView alloc] init];
  self.headerContentView = [[UIStackView alloc] init];
  self.contentView = [[UIView alloc] init];
  self.backButton = [UIButton buttonWithType:UIButtonTypeSystem];
  self.forwardButton = [UIButton buttonWithType:UIButtonTypeSystem];
  self.reloadOrStopButton = [UIButton buttonWithType:UIButtonTypeSystem];
  self.menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
  self.field = [[UITextField alloc] init];
  self.tracingHandler = [[TracingHandler alloc] init];

  // View hierarchy.
  [self.view addSubview:_headerBackgroundView];
  [self.view addSubview:_contentView];
  [_headerBackgroundView addArrangedSubview:_headerContentView];
  [_headerContentView addArrangedSubview:_backButton];
  [_headerContentView addArrangedSubview:_forwardButton];
  [_headerContentView addArrangedSubview:_reloadOrStopButton];
  [_headerContentView addArrangedSubview:_menuButton];
  [_headerContentView addArrangedSubview:_field];

  self.view.accessibilityElements = @[ _headerBackgroundView, _contentView ];
  self.view.isAccessibilityElement = NO;

  // |_headerBackgroundView| is a 1-item UIStackView. We use a UIStackView so
  // that we can:
  // 1. Easily hide |_headerContentView| when entering fullscreen mode in a way
  // that removes it from the layout.
  // 2. Let UIStackView figure out most constraints for |_headerContentView| so
  // that we do not have to do it manually.
  _headerBackgroundView.backgroundColor =
      [ContentShellWindowDelegate backgroundColorDefault];
  _headerBackgroundView.alignment = UIStackViewAlignmentBottom;
  _headerBackgroundView.axis = UILayoutConstraintAxisHorizontal;
  // Use the root view's layout margins (which account for safe areas and the
  // system's minimum margins).
  _headerBackgroundView.layoutMarginsRelativeArrangement = YES;
  _headerBackgroundView.preservesSuperviewLayoutMargins = YES;

#if BUILDFLAG(IS_IOS_TVOS)
  // On tvOS, make it impossible to focus `_headerContentView` by simply
  // swiping up on the remote control since this behavior is not intuitive.
  _headerContentView.userInteractionEnabled = NO;
#endif

  _headerContentView.alignment = UIStackViewAlignmentCenter;
  _headerContentView.axis = UILayoutConstraintAxisHorizontal;
  _headerContentView.spacing = 16.0;

  [_backButton setImage:[UIImage imageNamed:@"ic_back"]
               forState:UIControlStateNormal];
  _backButton.tintColor = [UIColor whiteColor];
  [_backButton addTarget:self
                  action:@selector(back)
        forControlEvents:UIControlEventTouchUpInside |
                         UIControlEventPrimaryActionTriggered];

  [_forwardButton setImage:[UIImage imageNamed:@"ic_forward"]
                  forState:UIControlStateNormal];
  _forwardButton.tintColor = [UIColor whiteColor];
  [_forwardButton addTarget:self
                     action:@selector(forward)
           forControlEvents:UIControlEventTouchUpInside |
                            UIControlEventPrimaryActionTriggered];

  [_reloadOrStopButton setImage:[UIImage imageNamed:@"ic_reload"]
                       forState:UIControlStateNormal];
  _reloadOrStopButton.tintColor = [UIColor whiteColor];
  [_reloadOrStopButton addTarget:self
                          action:@selector(reloadOrStop)
                forControlEvents:UIControlEventTouchUpInside |
                                 UIControlEventPrimaryActionTriggered];

  _menuButton.tintColor = [UIColor whiteColor];
  [_menuButton setImage:[UIImage imageNamed:@"ic_menu"]
               forState:UIControlStateNormal];
  [_menuButton addTarget:self
                  action:@selector(showMainMenu)
        forControlEvents:UIControlEventTouchUpInside |
                         UIControlEventPrimaryActionTriggered];

  _field.placeholder = @"Search or type URL";
  _field.tintColor = _headerBackgroundView.backgroundColor;
  [_field setContentHuggingPriority:UILayoutPriorityDefaultLow - 1
                            forAxis:UILayoutConstraintAxisHorizontal];
  _field.delegate = self;
  _field.borderStyle = UITextBorderStyleRoundedRect;
  _field.keyboardType = UIKeyboardTypeWebSearch;
  _field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  _field.clearButtonMode = UITextFieldViewModeWhileEditing;
  _field.autocorrectionType = UITextAutocorrectionTypeNo;

  // Constraints.
  _headerBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_headerBackgroundView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [_headerBackgroundView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerBackgroundView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  _headerContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    // This height constraint is somewhat arbitrary: the idea is that it gives
    // us enough space to centralize the buttons inside |_headerContentView|
    // while having enough top and bottom margins.
    // Twice the size of a button also accounts for platforms such as tvOS,
    // where focused buttons are larger and have a drop shadow.
    [_headerContentView.heightAnchor
        constraintEqualToAnchor:_backButton.heightAnchor
                     multiplier:2.0],
  ]];

  _contentView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_contentView.topAnchor
        constraintEqualToAnchor:_headerBackgroundView.bottomAnchor],
    [_contentView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [_contentView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
    [_contentView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
  ]];

  // Enable Accessibility if VoiceOver is already running.
  if (UIAccessibilityIsVoiceOverRunning()) {
    _scoped_accessibility_mode =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForProcess(ui::kAXModeComplete |
                                         ui::AXMode::kFromPlatform);
  }

  // Register for VoiceOver notifications.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(voiceOverStatusDidChange)
             name:UIAccessibilityVoiceOverStatusDidChangeNotification
           object:nil];

  UIView* web_contents_view = _shell->web_contents()->GetNativeView().Get();
  [_contentView addSubview:web_contents_view];

  if (@available(ios 17.0, *)) {
    NSArray<UITrait>* traits = @[ UITraitUserInterfaceStyle.self ];
    [self registerForTraitChanges:traits
                       withTarget:self
                           action:@selector(darkModeDidChange)];
  }
  [self darkModeDidChange];
}

- (void)darkModeDidChange {
  BOOL darkModeEnabled =
      (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
  _field.backgroundColor =
      darkModeEnabled ? [UIColor darkGrayColor] : [UIColor whiteColor];
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
  UIAlertController* alertController = [self actionSheetWithTitle:@"Main menu"
                                                          message:nil];

  [alertController
      addAction:[UIAlertAction actionWithTitle:@"Cancel"
                                         style:UIAlertActionStyleCancel
                                       handler:nil]];

  __weak ContentShellWindowDelegate* weakSelf = self;

  bool jit_enabled = content::ShellContentBrowserClient::Get()->IsJITEnabled();
  NSString* jit_label = jit_enabled ? @"Disable JIT" : @"Enable JIT";
  [alertController
      addAction:[UIAlertAction
                    actionWithTitle:jit_label
                              style:UIAlertActionStyleDefault
                            handler:^(UIAlertAction* action) {
                              content::ShellContentBrowserClient::Get()
                                  ->SetJITEnabled(!jit_enabled);
                            }]];

  if ([_tracingHandler isTracing]) {
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"End Tracing"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf stopTracing];
                                         }]];
  } else {
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Begin Graphics Tracing"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf
                                               startTracingWithCategories:
                                                   kGraphicsTracingCategories];
                                         }]];
    [alertController
        addAction:[UIAlertAction
                      actionWithTitle:@"Begin Detailed Graphics Tracing"
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* action) {
                                [weakSelf
                                    startTracingWithCategories:
                                        kDetailedGraphicsTracingCategories];
                              }]];
    [alertController
        addAction:[UIAlertAction
                      actionWithTitle:@"Begin Navigation Tracing"
                                style:UIAlertActionStyleDefault
                              handler:^(UIAlertAction* action) {
                                [weakSelf startTracingWithCategories:
                                              kNavigationTracingCategories];
                              }]];
    [alertController
        addAction:[UIAlertAction actionWithTitle:@"Begin Tracing All Categories"
                                           style:UIAlertActionStyleDefault
                                         handler:^(UIAlertAction* action) {
                                           [weakSelf startTracingWithCategories:
                                                         kAllTracingCategories];
                                         }]];
  }

  [self presentViewController:alertController animated:YES completion:nil];
}

- (void)updateBackground {
  _headerBackgroundView.backgroundColor =
      [_tracingHandler isTracing]
          ? [ContentShellWindowDelegate backgroundColorTracing]
          : [ContentShellWindowDelegate backgroundColorDefault];
}

- (void)stopTracing {
  [_tracingHandler stop];
}

- (void)startTracingWithCategories:(const char*)categories {
  __weak ContentShellWindowDelegate* weakSelf = self;
  [_tracingHandler
      startWithHandler:^{
        [weakSelf updateBackground];
      }
      stopHandler:^{
        [weakSelf updateBackground];
      }
      categories:categories];
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
  [_field resignFirstResponder];
  _shell->LoadURL(url);
  return YES;
}

- (void)setContents:(UIView*)content {
  [_contentView addSubview:content];
}

- (UIAlertController*)actionSheetWithTitle:(nullable NSString*)title
                                   message:(nullable NSString*)message {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:title
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];
  alertController.popoverPresentationController.sourceView = _menuButton;
  alertController.popoverPresentationController.sourceRect =
      CGRectMake(CGRectGetWidth(_menuButton.bounds) / 2,
                 CGRectGetHeight(_menuButton.bounds), 1, 1);
  return alertController;
}

- (void)voiceOverStatusDidChange {
  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();
  if (UIAccessibilityIsVoiceOverRunning()) {
    _scoped_accessibility_mode =
        accessibility_state->CreateScopedModeForProcess(
            ui::kAXModeComplete | ui::AXMode::kFromPlatform |
            ui::AXMode::kScreenReader);
  } else {
    _scoped_accessibility_mode.reset();
  }
}
@end

@implementation TracingHandler

- (void)startWithHandler:(void (^)())startHandler
             stopHandler:(void (^)())stopHandler
              categories:(const char*)categories {
  int i = 0;
  NSString* filename;
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* path = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES)[0];

  do {
    filename =
        [path stringByAppendingPathComponent:
                  [NSString stringWithFormat:@"trace_%d.pftrace.gz", i++]];
  } while ([fileManager fileExistsAtPath:filename]);

  if (![fileManager createFileAtPath:filename contents:nil attributes:nil]) {
    NSLog(@"Failed to create tracefile: %@", filename);
    return;
  }

  _traceFileHandle = [NSFileHandle fileHandleForWritingAtPath:filename];
  if (_traceFileHandle == nil) {
    NSLog(@"Failed to open tracefile: %@", filename);
    return;
  }

  NSLog(@"Will trace to file: %@", filename);

  perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
      base::trace_event::TraceConfig(categories, ""),
      /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/true);

  perfetto_config.set_write_into_file(true);
  _tracingSession =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);

  _tracingSession->Setup(perfetto_config, [_traceFileHandle fileDescriptor]);

  __weak TracingHandler* weakSelf = self;
  auto runner = base::SequencedTaskRunner::GetCurrentDefault();

  _tracingSession->SetOnStartCallback([runner, startHandler]() {
    runner->PostTask(FROM_HERE, base::BindOnce(^{
                       startHandler();
                     }));
  });

  _tracingSession->SetOnStopCallback([runner, weakSelf, stopHandler]() {
    runner->PostTask(FROM_HERE, base::BindOnce(^{
                       [weakSelf onStopped];
                       stopHandler();
                     }));
  });

  _tracingSession->Start();
}

- (void)stop {
  _tracingSession->Stop();
}

- (void)onStopped {
  [_traceFileHandle closeFile];
  _traceFileHandle = nil;
  _tracingSession.reset();
}

- (id)init {
  _traceFileHandle = nil;
  return self;
}

- (BOOL)isTracing {
  return !!_tracingSession.get();
}

@end

namespace content {

struct ShellPlatformDelegate::ShellData {
  UIWindow* window;
  bool fullscreen = false;
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

  return gfx::NativeWindow(shell_data.window);
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
  return false;  // We have not destroyed the shell here.
}

std::unique_ptr<ColorChooser> ShellPlatformDelegate::OpenColorChooser(
    WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  return ShellColorChooserIOS::OpenColorChooser(web_contents, color,
                                                suggestions);
}

void ShellPlatformDelegate::RunFileChooser(
    RenderFrameHost* render_frame_host,
    scoped_refptr<FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  ShellFileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                        params);
}

void ShellPlatformDelegate::ToggleFullscreenModeForTab(
    Shell* shell,
    WebContents* web_contents,
    bool enter_fullscreen) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  if (shell_data.fullscreen == enter_fullscreen) {
    return;
  }
  shell_data.fullscreen = enter_fullscreen;
  [((ContentShellWindowDelegate*)shell_data.window.rootViewController)
      headerContentView]
      .hidden = enter_fullscreen;
}

bool ShellPlatformDelegate::IsFullscreenForTabOrPending(
    Shell* shell,
    const WebContents* web_contents) const {
  DCHECK(base::Contains(shell_data_map_, shell));
  auto iter = shell_data_map_.find(shell);
  return iter->second.fullscreen;
}

}  // namespace content
