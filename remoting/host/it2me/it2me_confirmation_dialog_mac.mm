// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface It2MeConfirmationDialogMacController : NSObject {
 @private
  NSAlert* __strong _confirmation_alert;
  std::u16string _username;
  remoting::It2MeConfirmationDialog::ResultCallback _dialog_action_callback;
}

- (instancetype)initWithCallback:
                    (remoting::It2MeConfirmationDialog::ResultCallback)callback
                        username:(const std::string&)username;
- (void)show;
- (void)hide;
- (void)onCancel:(id)sender;
- (void)onAccept:(id)sender;
@end

namespace remoting {

namespace {
// Time to wait before closing the dialog and cancelling the connection.
constexpr base::TimeDelta kDialogTimeout = base::Minutes(1);
}  // namespace

// Bridge between C++ and ObjC implementations of It2MeConfirmationDialog.
class It2MeConfirmationDialogMac : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogMac();

  It2MeConfirmationDialogMac(const It2MeConfirmationDialogMac&) = delete;
  It2MeConfirmationDialogMac& operator=(const It2MeConfirmationDialogMac&) =
      delete;

  ~It2MeConfirmationDialogMac() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override;

 private:
  void OnDialogAction(Result result);

  It2MeConfirmationDialogMacController* __strong controller_;

  ResultCallback result_callback_;

  base::OneShotTimer dialog_timer_;
};

It2MeConfirmationDialogMac::It2MeConfirmationDialogMac() = default;

It2MeConfirmationDialogMac::~It2MeConfirmationDialogMac() {
  dialog_timer_.Stop();

  if (controller_) {
    @autoreleasepool {
      [controller_ hide];
    }
  }
}

void It2MeConfirmationDialogMac::Show(const std::string& remote_user_email,
                                      ResultCallback callback) {
  result_callback_ = std::move(callback);

  dialog_timer_.Start(
      FROM_HERE, kDialogTimeout,
      base::BindOnce(&It2MeConfirmationDialogMac::OnDialogAction,
                     base::Unretained(this), Result::CANCEL));

  ResultCallback dialog_action_callback = base::BindOnce(
      &It2MeConfirmationDialogMac::OnDialogAction, base::Unretained(this));

  @autoreleasepool {
    controller_ = [[It2MeConfirmationDialogMacController alloc]
        initWithCallback:std::move(dialog_action_callback)
                username:remote_user_email];
    [controller_ show];
  }
}

void It2MeConfirmationDialogMac::OnDialogAction(Result result) {
  dialog_timer_.Stop();

  if (controller_) {
    @autoreleasepool {
      [controller_ hide];
      controller_ = nil;
    }
  }

  if (result_callback_) {
    std::move(result_callback_).Run(result);
  }
}

std::unique_ptr<It2MeConfirmationDialog>
It2MeConfirmationDialogFactory::Create() {
  return std::make_unique<It2MeConfirmationDialogMac>();
}

}  // namespace remoting

@implementation It2MeConfirmationDialogMacController

- (instancetype)initWithCallback:
                    (remoting::It2MeConfirmationDialog::ResultCallback)callback
                        username:(const std::string&)username {
  if ((self = [super init])) {
    _username = base::UTF8ToUTF16(username);
    _dialog_action_callback = std::move(callback);
  }
  return self;
}

- (void)show {
  _confirmation_alert = [[NSAlert alloc] init];

  std::u16string dialog_text =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME),
          _username);
  _confirmation_alert.messageText = base::SysUTF16ToNSString(dialog_text);

  NSButton* cancel_button = [_confirmation_alert
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_SHARE_CONFIRM_DIALOG_DECLINE)];
  cancel_button.action = @selector(onCancel:);
  cancel_button.target = self;

  NSButton* confirm_button = [_confirmation_alert
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_SHARE_CONFIRM_DIALOG_CONFIRM)];
  confirm_button.action = @selector(onAccept:);
  confirm_button.target = self;

  NSBundle* bundle = [NSBundle bundleForClass:[self class]];
  NSString* imagePath = [bundle pathForResource:@"chromoting128" ofType:@"png"];
  NSImage* image = [[NSImage alloc] initByReferencingFile:imagePath];
  _confirmation_alert.icon = image;
  [_confirmation_alert layout];

  // Force alert to be at the proper level and location.
  NSWindow* confirmation_window = _confirmation_alert.window;
  [confirmation_window center];
  confirmation_window.title = l10n_util::GetNSString(IDS_PRODUCT_NAME);
  confirmation_window.level = NSNormalWindowLevel;
  [confirmation_window orderFrontRegardless];
  [confirmation_window makeKeyWindow];
}

- (void)hide {
  if (_confirmation_alert) {
    [_confirmation_alert.window close];
    _confirmation_alert = nil;
  }
}

- (void)onCancel:(id)sender {
  [self hide];
  if (_dialog_action_callback) {
    std::move(_dialog_action_callback)
        .Run(remoting::It2MeConfirmationDialog::Result::CANCEL);
  }
}

- (void)onAccept:(id)sender {
  [self hide];
  if (_dialog_action_callback) {
    std::move(_dialog_action_callback)
        .Run(remoting::It2MeConfirmationDialog::Result::OK);
  }
}

@end
