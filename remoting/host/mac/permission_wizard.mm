// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/permission_wizard.h"

#import <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

using remoting::mac::PermissionWizard;
using Delegate = PermissionWizard::Delegate;
using ResultCallback = PermissionWizard::ResultCallback;

namespace {

// Interval between permission checks, used to update the UI when the user
// grants permission.
constexpr base::TimeDelta kPollingInterval = base::TimeDelta::FromSeconds(1);

// The steps of the wizard.
enum class WizardPage {
  ACCESSIBILITY,
  SCREEN_RECORDING,
  ALL_SET,
};

}  // namespace

@interface PermissionWizardController : NSWindowController

- (instancetype)initWithWindow:(NSWindow*)window
                          impl:(PermissionWizard::Impl*)impl;
- (void)hide;
- (void)start;

// Used by C++ PermissionWizardImpl to provide the result of a permission check
// to the WindowController.
- (void)onPermissionCheckResult:(bool)result;

@end

namespace remoting {
namespace mac {

// C++ implementation of the PermissionWizard.
class PermissionWizard::Impl {
 public:
  explicit Impl(std::unique_ptr<PermissionWizard::Delegate> checker);
  ~Impl();

  void SetCompletionCallback(ResultCallback callback);
  void Start();

  std::string GetBundleName();

  // Called by PermissionWizardController to initiate permission checks. The
  // result will be passed back via onPermissionCheckResult().
  void CheckAccessibilityPermission(base::TimeDelta delay);
  void CheckScreenRecordingPermission(base::TimeDelta delay);

  // Called by PermissionWizardController to notify that the wizard was
  // completed/cancelled.
  void NotifyCompletion(bool result);

 private:
  void CheckAccessibilityPermissionNow();
  void CheckScreenRecordingPermissionNow();

  void OnPermissionCheckResult(bool result);

  PermissionWizardController* window_controller_ = nil;
  std::unique_ptr<Delegate> checker_;
  base::OneShotTimer timer_;

  // Notified when the wizard is completed/cancelled. May be null.
  ResultCallback completion_callback_;

  base::WeakPtrFactory<Impl> weak_factory_{this};
};

PermissionWizard::Impl::Impl(
    std::unique_ptr<PermissionWizard::Delegate> checker)
    : checker_(std::move(checker)) {}

PermissionWizard::Impl::~Impl() {
  [window_controller_ hide];
  [window_controller_ release];
}

void PermissionWizard::Impl::SetCompletionCallback(ResultCallback callback) {
  completion_callback_ = std::move(callback);
}

void PermissionWizard::Impl::Start() {
  NSWindow* window =
      [[[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                   styleMask:NSWindowStyleMaskTitled
                                     backing:NSBackingStoreBuffered
                                       defer:NO] autorelease];
  window_controller_ = [[PermissionWizardController alloc] initWithWindow:window
                                                                     impl:this];
  [window_controller_ start];
}

std::string PermissionWizard::Impl::GetBundleName() {
  return checker_->GetBundleName();
}

void PermissionWizard::Impl::CheckAccessibilityPermission(
    base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this, &Impl::CheckAccessibilityPermissionNow);
}

void PermissionWizard::Impl::CheckScreenRecordingPermission(
    base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay, this,
               &Impl::CheckScreenRecordingPermissionNow);
}

void PermissionWizard::Impl::NotifyCompletion(bool result) {
  if (completion_callback_)
    std::move(completion_callback_).Run(result);
}

void PermissionWizard::Impl::CheckAccessibilityPermissionNow() {
  checker_->CheckAccessibilityPermission(base::BindOnce(
      &Impl::OnPermissionCheckResult, weak_factory_.GetWeakPtr()));
}

void PermissionWizard::Impl::CheckScreenRecordingPermissionNow() {
  checker_->CheckScreenRecordingPermission(base::BindOnce(
      &Impl::OnPermissionCheckResult, weak_factory_.GetWeakPtr()));
}

void PermissionWizard::Impl::OnPermissionCheckResult(bool result) {
  [window_controller_ onPermissionCheckResult:result];
}

}  // namespace mac
}  // namespace remoting

@implementation PermissionWizardController {
  NSTextField* _instructionText;
  NSButton* _cancelButton;
  NSButton* _nextButton;

  // The page of the wizard being shown.
  WizardPage _page;

  // Whether the relevant permission has been granted for the current page. If
  // YES, the user will be able to advance to the next page of the wizard.
  BOOL _hasPermission;

  // Set to YES when the user cancels the wizard. This allows code to
  // distinguish between "window hidden because it hasn't been presented yet"
  // and "window hidden because user closed it".
  BOOL _cancelled;

  // If YES, the wizard will automatically move onto the next page instead of
  // showing the "Next" button when permission is granted. This allows the
  // wizard to skip past any pages whose permission is already granted.
  BOOL _autoAdvance;

  // Reference used for permission-checking. Its lifetime should outlast this
  // Controller.
  PermissionWizard::Impl* _impl;
}

- (instancetype)initWithWindow:(NSWindow*)window
                          impl:(PermissionWizard::Impl*)impl {
  DCHECK(window);
  DCHECK(impl);
  self = [super initWithWindow:window];
  if (self) {
    _impl = impl;
    _page = WizardPage::ACCESSIBILITY;
    _autoAdvance = YES;
  }
  return self;
}

- (void)hide {
  [self close];
}

- (void)start {
  [self initializeWindow];

  // Start polling for permission status.
  [self requestPermissionCheck:base::TimeDelta()];
}

- (void)initializeWindow {
  self.window.title =
      l10n_util::GetNSStringF(IDS_MAC_PERMISSION_WIZARD_TITLE,
                              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  _instructionText = [[[NSTextField alloc] init] autorelease];
  _instructionText.translatesAutoresizingMaskIntoConstraints = NO;
  _instructionText.drawsBackground = NO;
  _instructionText.bezeled = NO;
  _instructionText.editable = NO;
  _instructionText.preferredMaxLayoutWidth = 400;

  NSImageView* icon = [[[NSImageView alloc] init] autorelease];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.image = [[NSApplication sharedApplication] applicationIconImage];

  _cancelButton = [[[NSButton alloc] init] autorelease];
  _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  _cancelButton.buttonType = NSButtonTypeMomentaryPushIn;
  _cancelButton.bezelStyle = NSBezelStyleRegularSquare;
  _cancelButton.title =
      l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_CANCEL_BUTTON);
  _cancelButton.action = @selector(onCancel:);
  _cancelButton.target = self;

  _nextButton = [[[NSButton alloc] init] autorelease];
  _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
  _nextButton.buttonType = NSButtonTypeMomentaryPushIn;
  _nextButton.bezelStyle = NSBezelStyleRegularSquare;
  _nextButton.action = @selector(onNext:);
  _nextButton.target = self;

  NSStackView* iconAndTextStack = [[[NSStackView alloc] init] autorelease];
  iconAndTextStack.translatesAutoresizingMaskIntoConstraints = NO;
  iconAndTextStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  iconAndTextStack.alignment = NSLayoutAttributeTop;
  [iconAndTextStack addView:icon inGravity:NSStackViewGravityLeading];
  [iconAndTextStack addView:_instructionText
                  inGravity:NSStackViewGravityLeading];

  NSStackView* buttonsStack = [[[NSStackView alloc] init] autorelease];
  buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  [buttonsStack addView:_cancelButton inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_nextButton inGravity:NSStackViewGravityTrailing];

  NSStackView* mainStack = [[[NSStackView alloc] init] autorelease];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.orientation = NSUserInterfaceLayoutOrientationVertical;
  mainStack.spacing = 20;
  [mainStack addView:iconAndTextStack inGravity:NSStackViewGravityTop];
  [mainStack addView:buttonsStack inGravity:NSStackViewGravityBottom];

  [self.window.contentView addSubview:mainStack];

  NSDictionary* views = @{
    @"iconAndText" : iconAndTextStack,
    @"buttons" : buttonsStack,
    @"mainStack" : mainStack,
  };

  // Expand |iconAndTextStack| to match parent's width.
  [mainStack addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"H:|[iconAndText]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];

  // Expand |buttonsStack| to match parent's width.
  [mainStack addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:@"H:|[buttons]|"
                                                    options:0
                                                    metrics:nil
                                                      views:views]];

  // Expand |mainStack| to fill the window's contentView (with standard margin).
  [self.window.contentView
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:@"H:|-[mainStack]-|"
                                             options:0
                                             metrics:nil
                                               views:views]];
  [self.window.contentView
      addConstraints:[NSLayoutConstraint
                         constraintsWithVisualFormat:@"V:|-[mainStack]-|"
                                             options:0
                                             metrics:nil
                                               views:views]];
}

- (void)onCancel:(id)sender {
  _impl->NotifyCompletion(false);
  _cancelled = YES;
  [self hide];
}

- (void)onNext:(id)sender {
  if (_page == WizardPage::ALL_SET) {
    // OK button closes the window.
    _impl->NotifyCompletion(true);
    [self hide];
    return;
  }
  if (_hasPermission) {
    [self advanceToNextPage];
  } else {
    [self launchSystemPreferences];
  }
}

// Updates the dialog controls according to the object's state.
- (void)updateUI {
  base::string16 bundleName = base::UTF8ToUTF16(_impl->GetBundleName());
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _instructionText.stringValue = l10n_util::GetNSStringF(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_BODY_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          l10n_util::GetStringUTF16(
              IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON),
          bundleName);
      break;
    case WizardPage::SCREEN_RECORDING:
      _instructionText.stringValue = l10n_util::GetNSStringF(
          IDS_SCREEN_RECORDING_PERMISSION_DIALOG_BODY_TEXT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
          l10n_util::GetStringUTF16(
              IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON),
          bundleName);
      break;
    case WizardPage::ALL_SET:
      _instructionText.stringValue =
          l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_FINAL_TEXT);
      break;
    default:
      NOTREACHED();
  }
  _cancelButton.hidden = (_page == WizardPage::ALL_SET);
  [self updateNextButton];
}

// Updates |_nextButton| according to the object's state.
- (void)updateNextButton {
  if (_page == WizardPage::ALL_SET) {
    _nextButton.title =
        l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_OK_BUTTON);
    return;
  }

  if (_hasPermission) {
    _nextButton.title =
        l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_NEXT_BUTTON);
    return;
  }

  // Permission is not granted, so show the appropriate launch text.
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _nextButton.title = l10n_util::GetNSString(
          IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON);
      break;
    case WizardPage::SCREEN_RECORDING:
      _nextButton.title = l10n_util::GetNSString(
          IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON);
      break;
    default:
      NOTREACHED();
  }
}

- (void)advanceToNextPage {
  DCHECK(_hasPermission);
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _page = WizardPage::SCREEN_RECORDING;
      break;
    case WizardPage::SCREEN_RECORDING:
      _page = WizardPage::ALL_SET;
      if ([self window].visible) {
        [self updateUI];
      } else {
        // If the wizard hasn't been shown yet, this means that all permissions
        // were already granted, and the final ALL_SET page will not be shown.
        _impl->NotifyCompletion(true);
      }
      return;
    default:
      NOTREACHED();
  }

  // Kick off a permission check for the new page. The UI will be updated only
  // after the result comes back.
  _hasPermission = NO;
  _autoAdvance = YES;
  [self requestPermissionCheck:base::TimeDelta()];
}

- (void)requestPermissionCheck:(base::TimeDelta)delay {
  DCHECK(!_hasPermission);
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      _impl->CheckAccessibilityPermission(delay);
      break;
    case WizardPage::SCREEN_RECORDING:
      _impl->CheckScreenRecordingPermission(delay);
      return;
    default:
      NOTREACHED();
  }
}

- (void)launchSystemPreferences {
  switch (_page) {
    case WizardPage::ACCESSIBILITY:
      // Launch the Security and Preferences pane with Accessibility selected.
      [[NSWorkspace sharedWorkspace]
          openURL:[NSURL URLWithString:
                             @"x-apple.systempreferences:com.apple."
                             @"preference.security?Privacy_Accessibility"]];
      break;
    case WizardPage::SCREEN_RECORDING:
      // Launch the Security and Preferences pane with Screen Recording
      // selected.
      [[NSWorkspace sharedWorkspace]
          openURL:[NSURL URLWithString:
                             @"x-apple.systempreferences:com.apple."
                             @"preference.security?Privacy_ScreenCapture"]];
      return;
    default:
      NOTREACHED();
  }
}

- (void)onPermissionCheckResult:(bool)result {
  if (_cancelled)
    return;

  _hasPermission = result;

  if (_hasPermission && _autoAdvance) {
    // Skip showing the "Next" button, and immediately kick off a permission
    // check for the next page, if any.
    [self advanceToNextPage];
    return;
  }

  // Update the whole UI, not just the "Next" button, in case a different page
  // was previously shown.
  [self updateUI];

  if (!_hasPermission) {
    // Permission denied, so turn off auto-advance for this page, and present
    // the dialog to the user if needed. After the user grants this permission,
    // they should be able to click "Next" to acknowledge and advance the
    // wizard. Note that, if all permissions are granted, the user will not
    // see the wizard at all (not even the ALL_SET page). A dialog is only
    // shown when a permission-check fails.
    _autoAdvance = NO;
    if (![self window].visible) {
      [self presentWindow];
    }

    // Keep polling until permission is granted.
    [self requestPermissionCheck:kPollingInterval];
  }
}

- (void)presentWindow {
  [self.window makeKeyAndOrderFront:NSApp];
  [self.window center];
  [self showWindow:nil];
}

@end

namespace remoting {
namespace mac {

PermissionWizard::PermissionWizard(std::unique_ptr<Delegate> checker)
    : impl_(std::make_unique<PermissionWizard::Impl>(std::move(checker))) {}

PermissionWizard::~PermissionWizard() {
  ui_task_runner_->DeleteSoon(FROM_HERE, impl_.release());
}

void PermissionWizard::SetCompletionCallback(ResultCallback callback) {
  impl_->SetCompletionCallback(std::move(callback));
}

void PermissionWizard::Start(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  ui_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&Impl::Start, base::Unretained(impl_.get())));
}

}  // namespace mac
}  // namespace remoting
