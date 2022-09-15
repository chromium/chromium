// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_javascript_dialog.h"

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface ShellJavaScriptDialogHelper : NSObject<NSAlertDelegate> {
 @private
  base::scoped_nsobject<NSAlert> _alert;
  NSTextField* _textField;  // WEAK; owned by alert_

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
  _alert.reset([[NSAlert alloc] init]);
  return _alert;
}

- (NSTextField*)textField {
  _textField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  [[_textField cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [_alert setAccessoryView:_textField];
  [[_alert window] setInitialFirstResponder:_textField];
  [_textField release];

  return _textField;
}

- (void)alertDidEndWithResult:(NSModalResponse)returnCode
                       dialog:(content::ShellJavaScriptDialog*)dialog {
  if (returnCode == NSModalResponseStop)
    return;

  bool success = returnCode == NSAlertFirstButtonReturn;
  std::u16string input;
  if (_textField)
    input = base::SysNSStringToUTF16([_textField stringValue]);

  std::move(_callback).Run(success, input);
  _manager->DialogClosed(dialog);
}

- (void)cancel {
  [NSApp endSheet:[_alert window]];
  _alert.reset();
  if (_callback)
    std::move(_callback).Run(false, std::u16string());
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
    [field setStringValue:base::SysUTF16ToNSString(default_prompt_text)];
  }
  [alert setDelegate:helper_];
  [alert setInformativeText:base::SysUTF16ToNSString(message_text)];
  [alert setMessageText:@"Javascript alert"];
  [alert addButtonWithTitle:@"OK"];
  if (!one_button) {
    NSButton* other = [alert addButtonWithTitle:@"Cancel"];
    [other setKeyEquivalent:@"\e"];
  }

  [alert beginSheetModalForWindow:nil  // nil here makes it app-modal
                completionHandler:^void(NSModalResponse returnCode) {
                  [helper_ alertDidEndWithResult:returnCode dialog:this];
                }];
}

ShellJavaScriptDialog::~ShellJavaScriptDialog() {
  [helper_ release];
}

void ShellJavaScriptDialog::Cancel() {
  [helper_ cancel];
}

}  // namespace content
