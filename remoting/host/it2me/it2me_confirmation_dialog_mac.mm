// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "remoting/host/it2me/it2me_confirmation_dialog.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/location.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface It2MeConfirmationDialogMacController : NSObject {
 @private
  base::scoped_nsobject<NSAlert> confirmation_alert_;
  base::string16 username_;
  remoting::It2MeConfirmationDialog::ResultCallback dialog_action_callback_;
}

- (id)initWithCallback:
          (const remoting::It2MeConfirmationDialog::ResultCallback&)callback
              username:(const std::string&)username;
- (void)show;
- (void)hide;
- (void)onCancel:(id)sender;
- (void)onAccept:(id)sender;
@end

namespace remoting {

namespace {
// Time to wait before closing the dialog and cancelling the connection.
constexpr base::TimeDelta kDialogTimeout = base::TimeDelta::FromMinutes(1);
}

// Bridge between C++ and ObjC implementations of It2MeConfirmationDialog.
class It2MeConfirmationDialogMac : public It2MeConfirmationDialog {
 public:
  It2MeConfirmationDialogMac();
  ~It2MeConfirmationDialogMac() override;

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            const ResultCallback& callback) override;

 private:
  void OnDialogAction(Result result);

  base::scoped_nsobject<It2MeConfirmationDialogMacController> controller_;

  ResultCallback result_callback_;

  base::OneShotTimer dialog_timer_;

  DISALLOW_COPY_AND_ASSIGN(It2MeConfirmationDialogMac);
};

It2MeConfirmationDialogMac::It2MeConfirmationDialogMac() {}

It2MeConfirmationDialogMac::~It2MeConfirmationDialogMac() {
  dialog_timer_.Stop();

  if (controller_) {
    @autoreleasepool {
      [controller_ hide];
    }
  }
}

void It2MeConfirmationDialogMac::Show(const std::string& remote_user_email,
                                      const ResultCallback& callback) {
  result_callback_ = callback;

  dialog_timer_.Start(FROM_HERE, kDialogTimeout,
                      base::Bind(&It2MeConfirmationDialogMac::OnDialogAction,
                                 base::Unretained(this), Result::CANCEL));

  ResultCallback dialog_action_callback = base::Bind(
      &It2MeConfirmationDialogMac::OnDialogAction, base::Unretained(this));

  @autoreleasepool {
    controller_.reset([[It2MeConfirmationDialogMacController alloc]
        initWithCallback:dialog_action_callback
                username:remote_user_email]);
    [controller_ show];
  }
}

void It2MeConfirmationDialogMac::OnDialogAction(Result result) {
  dialog_timer_.Stop();

  if (controller_) {
    @autoreleasepool {
      [controller_ hide];
      controller_.reset();
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

- (id)initWithCallback:
          (const remoting::It2MeConfirmationDialog::ResultCallback&)callback
              username:(const std::string&)username {
  if ((self = [super init])) {
    username_ = base::UTF8ToUTF16(username);
    dialog_action_callback_ = callback;
  }
  return self;
}

- (void)show {
  confirmation_alert_.reset([[NSAlert alloc] init]);

  base::string16 dialog_text =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SHARE_CONFIRM_DIALOG_MESSAGE_WITH_USERNAME),
          username_);
  [confirmation_alert_ setMessageText:base::SysUTF16ToNSString(dialog_text)];

  NSButton* cancel_button = [confirmation_alert_
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_SHARE_CONFIRM_DIALOG_DECLINE)];
  [cancel_button setAction:@selector(onCancel:)];
  [cancel_button setTarget:self];

  NSButton* confirm_button = [confirmation_alert_
      addButtonWithTitle:l10n_util::GetNSString(
                             IDS_SHARE_CONFIRM_DIALOG_CONFIRM)];
  [confirm_button setAction:@selector(onAccept:)];
  [confirm_button setTarget:self];

  NSBundle* bundle = [NSBundle bundleForClass:[self class]];
  NSString* imagePath = [bundle pathForResource:@"chromoting128" ofType:@"png"];
  base::scoped_nsobject<NSImage> image(
      [[NSImage alloc] initByReferencingFile:imagePath]);
  [confirmation_alert_ setIcon:image];
  [confirmation_alert_ layout];

  // Force alert to be at the proper level and location.
  NSWindow* confirmation_window = [confirmation_alert_ window];
  [confirmation_window center];
  [confirmation_window setTitle:l10n_util::GetNSString(IDS_PRODUCT_NAME)];
  [confirmation_window setLevel:NSNormalWindowLevel];
  [confirmation_window orderFrontRegardless];
  [confirmation_window makeKeyWindow];
}

- (void)hide {
  if (confirmation_alert_) {
    [[confirmation_alert_ window] close];
    confirmation_alert_.reset();
  }
}

- (void)onCancel:(id)sender {
  [self hide];
  if (dialog_action_callback_) {
    std::move(dialog_action_callback_)
        .Run(remoting::It2MeConfirmationDialog::Result::CANCEL);
  }
}

- (void)onAccept:(id)sender {
  [self hide];
  if (dialog_action_callback_) {
    std::move(dialog_action_callback_)
        .Run(remoting::It2MeConfirmationDialog::Result::OK);
  }
}

@end
