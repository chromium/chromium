// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/permission_wizard.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#import "base/task/single_thread_task_runner.h"
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
constexpr base::TimeDelta kPollingInterval = base::Seconds(1);

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

namespace remoting::mac {

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

  PermissionWizardController* __strong window_controller_ = nil;
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
  window_controller_ = nil;
}

void PermissionWizard::Impl::SetCompletionCallback(ResultCallback callback) {
  completion_callback_ = std::move(callback);
}

void PermissionWizard::Impl::Start() {
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.releasedWhenClosed = NO;
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
  if (completion_callback_) {
    std::move(completion_callback_).Run(result);
  }
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

}  // namespace remoting::mac

@implementation PermissionWizardController {
  NSTextField* __strong _instructionText;
  NSButton* __strong _cancelButton;
  NSButton* __strong _launchA11yButton;
  NSButton* __strong _launchScreenRecordingButton;
  NSButton* __strong _nextButton;
  NSButton* __strong _okButton;

  // This class modifies the NSApplicationActivationPolicy in order to show a
  // Dock icon when presenting the dialog window. This is needed because the
  // native-messaging host sets LSUIElement=YES in its plist to hide the Dock
  // icon. This field stores the previous setting so it can be restored when
  // the window is closed (so this class will still do the right thing if it is
  // instantiated from an app that normally shows a Dock icon).
  NSApplicationActivationPolicy _originalActivationPolicy;

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
  raw_ptr<PermissionWizard::Impl> _impl;
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
  _originalActivationPolicy = [NSApp activationPolicy];
  return self;
}

- (void)hide {
  [NSApp setActivationPolicy:_originalActivationPolicy];
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

  _instructionText = [[NSTextField alloc] init];
  _instructionText.translatesAutoresizingMaskIntoConstraints = NO;
  _instructionText.drawsBackground = NO;
  _instructionText.bezeled = NO;
  _instructionText.editable = NO;
  _instructionText.preferredMaxLayoutWidth = 400;

  NSString* appPath = NSBundle.mainBundle.bundlePath;
  NSImage* iconImage = [NSWorkspace.sharedWorkspace iconForFile:appPath];
  [iconImage setSize:NSMakeSize(64, 64)];
  NSImageView* icon = [[NSImageView alloc] init];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  icon.image = iconImage;

  _cancelButton = [[NSButton alloc] init];
  _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
  _cancelButton.buttonType = NSButtonTypeMomentaryPushIn;
  _cancelButton.bezelStyle = NSBezelStyleFlexiblePush;
  _cancelButton.title =
      l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_CANCEL_BUTTON);
  _cancelButton.keyEquivalent = @"\e";
  _cancelButton.action = @selector(onCancel:);
  _cancelButton.target = self;

  _launchA11yButton = [[NSButton alloc] init];
  _launchA11yButton.translatesAutoresizingMaskIntoConstraints = NO;
  _launchA11yButton.buttonType = NSButtonTypeMomentaryPushIn;
  _launchA11yButton.bezelStyle = NSBezelStyleFlexiblePush;
  _launchA11yButton.title =
      l10n_util::GetNSString(IDS_ACCESSIBILITY_PERMISSION_DIALOG_OPEN_BUTTON);
  _launchA11yButton.action = @selector(onLaunchA11y:);
  _launchA11yButton.target = self;

  _launchScreenRecordingButton = [[NSButton alloc] init];
  _launchScreenRecordingButton.translatesAutoresizingMaskIntoConstraints = NO;
  _launchScreenRecordingButton.buttonType = NSButtonTypeMomentaryPushIn;
  _launchScreenRecordingButton.bezelStyle = NSBezelStyleFlexiblePush;
  _launchScreenRecordingButton.title = l10n_util::GetNSString(
      IDS_SCREEN_RECORDING_PERMISSION_DIALOG_OPEN_BUTTON);
  _launchScreenRecordingButton.action = @selector(onLaunchScreenRecording:);
  _launchScreenRecordingButton.target = self;

  _nextButton = [[NSButton alloc] init];
  _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
  _nextButton.buttonType = NSButtonTypeMomentaryPushIn;
  _nextButton.bezelStyle = NSBezelStyleFlexiblePush;
  _nextButton.title =
      l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_NEXT_BUTTON);
  _nextButton.keyEquivalent = @"\r";
  _nextButton.action = @selector(onNext:);
  _nextButton.target = self;

  _okButton = [[NSButton alloc] init];
  _okButton.translatesAutoresizingMaskIntoConstraints = NO;
  _okButton.buttonType = NSButtonTypeMomentaryPushIn;
  _okButton.bezelStyle = NSBezelStyleFlexiblePush;
  _okButton.title = l10n_util::GetNSString(IDS_MAC_PERMISSION_WIZARD_OK_BUTTON);
  _okButton.keyEquivalent = @"\r";
  _okButton.action = @selector(onOk:);
  _okButton.target = self;

  NSStackView* iconAndTextStack = [[NSStackView alloc] init];
  iconAndTextStack.translatesAutoresizingMaskIntoConstraints = NO;
  iconAndTextStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  iconAndTextStack.alignment = NSLayoutAttributeTop;
  [iconAndTextStack addView:icon inGravity:NSStackViewGravityLeading];
  [iconAndTextStack addView:_instructionText
                  inGravity:NSStackViewGravityCenter];

  NSStackView* buttonsStack = [[NSStackView alloc] init];
  buttonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  buttonsStack.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  [buttonsStack addView:_cancelButton inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_launchA11yButton inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_launchScreenRecordingButton
              inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_nextButton inGravity:NSStackViewGravityTrailing];
  [buttonsStack addView:_okButton inGravity:NSStackViewGravityTrailing];

  // Prevent buttonsStack from expanding vertically. This fixes incorrect
  // vertical placement of OK button in All Set page
  // (http://crbug.com/1032157). The parent NSStackView was expanding this
  // view instead of adding space between the gravity-areas.
  [buttonsStack setHuggingPriority:NSLayoutPriorityDefaultHigh
                    forOrientation:NSLayoutConstraintOrientationVertical];

  NSStackView* mainStack = [[NSStackView alloc] init];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.orientation = NSUserInterfaceLayoutOrientationVertical;
  mainStack.spacing = 12;
  [mainStack addView:iconAndTextStack inGravity:NSStackViewGravityTop];
  [mainStack addView:buttonsStack inGravity:NSStackViewGravityBottom];

  [self.window.contentView addSubview:mainStack];

  // Update button visibility, instructional text etc before window is
  // presented, to ensure correct layout. This updates the window's
  // first-responder, so it needs to happen after the child views are added to
  // the contentView.
  [self updateUI];

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

- (void)onLaunchA11y:(id)sender {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Accessibility);
}

- (void)onLaunchScreenRecording:(id)sender {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_ScreenRecording);
}

- (void)onNext:(id)sender {
  [self advanceToNextPage];
}

- (void)onOk:(id)sender {
  // OK button closes the window.
  _impl->NotifyCompletion(true);
  [self hide];
}

// Updates the dialog controls according to the object's state. This also
// updates the first-responder button, so it should only be called when the
// state needs to change.
- (void)updateUI {
  std::u16string bundleName = base::UTF8ToUTF16(_impl->GetBundleName());
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
  [self updateButtons];
}

// Updates the buttons according to the object's state. This updates the
// first-responder, so this should only be called when the buttons need to be
// changed.
- (void)updateButtons {
  // Launch buttons are always visible on their associated pages.
  _launchA11yButton.hidden = (_page != WizardPage::ACCESSIBILITY);
  _launchScreenRecordingButton.hidden = (_page != WizardPage::SCREEN_RECORDING);

  // OK is visible on ALL_SET, Cancel/Next are visible on all other pages.
  _cancelButton.hidden = (_page == WizardPage::ALL_SET);
  _nextButton.hidden = (_page == WizardPage::ALL_SET);
  _okButton.hidden = (_page != WizardPage::ALL_SET);

  // User can only advance if permission is granted.
  _nextButton.enabled = _hasPermission;

  // Give focus to the most appropriate button.
  if (_page == WizardPage::ALL_SET) {
    [self.window makeFirstResponder:_okButton];
  } else if (_hasPermission) {
    [self.window makeFirstResponder:_nextButton];
  } else {
    switch (_page) {
      case WizardPage::ACCESSIBILITY:
        [self.window makeFirstResponder:_launchA11yButton];
        break;
      case WizardPage::SCREEN_RECORDING:
        [self.window makeFirstResponder:_launchScreenRecordingButton];
        break;
      default:
        NOTREACHED();
    }
  }

  // Set the button tab-order (key view loop). Hidden/disabled buttons are
  // skipped, so it is OK to set the overall order for every button. This needs
  // to be done after setting the first-responder, otherwise the system chooses
  // an order which may not be correct.
  _cancelButton.nextKeyView = _launchA11yButton;
  _launchA11yButton.nextKeyView = _launchScreenRecordingButton;
  _launchScreenRecordingButton.nextKeyView = _nextButton;
  _nextButton.nextKeyView = _okButton;
  _okButton.nextKeyView = _cancelButton;
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

  // Kick off a permission check for the new page. Update the UI now, so the
  // Next button is disabled and can't be accidentally double-pressed.
  _hasPermission = NO;
  _autoAdvance = YES;
  [self updateUI];
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

- (void)onPermissionCheckResult:(bool)result {
  if (_cancelled) {
    return;
  }

  _hasPermission = result;

  if (_hasPermission && _autoAdvance) {
    // Skip showing the "Next" button, and immediately kick off a permission
    // check for the next page, if any.
    [self advanceToNextPage];
    return;
  }

  // Don't update the UI if permission denied, because that resets the button
  // focus, preventing the user from tabbing between buttons while polling for
  // permission status.
  if (_hasPermission) {
    // Update the whole UI, not just the "Next" button, in case a different page
    // was previously shown.
    [self updateUI];

    // Bring the window to the front again, to prompt the user to hit Next.
    [self presentWindow];
  } else {
    // Permission denied, so turn off auto-advance for this page, and present
    // the dialog to the user if needed. After the user grants this permission,
    // they should be able to click "Next" to acknowledge and advance the
    // wizard. Note that, if all permissions are granted, the user will not
    // see the wizard at all (not even the ALL_SET page). A dialog is only
    // shown when a permission-check fails.
    _autoAdvance = NO;
    if (![self window].visible) {
      // Only present the window if it was previously hidden. This method will
      // bring the window on top of other windows, which should not happen
      // during regular polling for permission status, as the user is focused on
      // the System Preferences applet.
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
  [NSApp activateIgnoringOtherApps:YES];

  // Show the application icon in the dock.
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

@end

namespace remoting::mac {

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

}  // namespace remoting::mac
