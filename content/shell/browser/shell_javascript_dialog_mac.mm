// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface ShellJavaScriptDialogHelper : NSObject<NSAlertDelegate> {
  NSAlert* __strong _alert;
  NSTextField* __weak _textField;

  // Copies of the fields in ShellJavaScriptDialog because they're private.
  raw_ptr<content::ShellJavaScriptDialogManager> _manager;
  content::JavaScriptDialogManager::DialogClosedCallback _callback;
}

- (id)initHelperWithManager:(content::ShellJavaScriptDialogManager*)manager
   andCallback:(content::JavaScriptDialogManager::DialogClosedCallback)callback;
- (NSAlert*)alert;
- (NSTextField*)textField;
- (void)alertDidEndWithResult:(NSModalResponse)returnCode
                       dialog:(content::ShellJavaScriptDialog*)dialog;
- (void)cancel;

@end

@implementation ShellJavaScriptDialogHelper

- (id)initHelperWithManager:(content::ShellJavaScriptDialogManager*)manager
  andCallback:(content::JavaScriptDialogManager::DialogClosedCallback)callback {
  if (self = [super init]) {
    _manager = manager;
    _callback = std::move(callback);
  }

  return self;
}

- (NSAlert*)alert {
  _alert = [[NSAlert alloc] init];
  return _alert;
}

- (NSTextField*)textField {
  NSTextField* textField =
      [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  textField.cell.lineBreakMode = NSLineBreakByTruncatingTail;

  _alert.accessoryView = textField;
  _alert.window.initialFirstResponder = textField;

  _textField = textField;
  return textField;
}

- (void)alertDidEndWithResult:(NSModalResponse)returnCode
                       dialog:(content::ShellJavaScriptDialog*)dialog {
  if (returnCode == NSModalResponseStop) {
    return;
  }

  bool success = returnCode == NSAlertFirstButtonReturn;
  std::u16string input;
  if (_textField) {
    input = base::SysNSStringToUTF16(_textField.stringValue);
  }

  std::move(_callback).Run(success, input);
  _manager->DialogClosed(dialog);
}

- (void)cancel {
  [NSApp endSheet:_alert.window];
  _alert = nil;
  if (_callback) {
    std::move(_callback).Run(false, std::u16string());
  }
}

@end

namespace content {

ShellJavaScriptDialog::ShellJavaScriptDialog(
    ShellJavaScriptDialogManager* manager,
    gfx::NativeWindow parent_window,
    JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    JavaScriptDialogManager::DialogClosedCallback callback) {
  bool text_field = dialog_type == JAVASCRIPT_DIALOG_TYPE_PROMPT;
  bool one_button = dialog_type == JAVASCRIPT_DIALOG_TYPE_ALERT;

  helper_ = [[ShellJavaScriptDialogHelper alloc]
      initHelperWithManager:manager
                andCallback:std::move(callback)];

  // Show the modal dialog.
  NSAlert* alert = [helper_ alert];
  NSTextField* field = nil;
  if (text_field) {
    field = [helper_ textField];
    field.stringValue = base::SysUTF16ToNSString(default_prompt_text);
  }
  alert.delegate = helper_;
  alert.informativeText = base::SysUTF16ToNSString(message_text);
  alert.messageText = @"Javascript alert";
  [alert addButtonWithTitle:@"OK"];
  if (!one_button) {
    NSButton* other = [alert addButtonWithTitle:@"Cancel"];
    other.keyEquivalent = @"\e";
  }

  [alert beginSheetModalForWindow:nil  // nil here makes it app-modal
                completionHandler:^void(NSModalResponse returnCode) {
                  [helper_ alertDidEndWithResult:returnCode dialog:this];
                }];
}

ShellJavaScriptDialog::~ShellJavaScriptDialog() = default;

void ShellJavaScriptDialog::Cancel() {
  [helper_ cancel];
}

}  // namespace content
