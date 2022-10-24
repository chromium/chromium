// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// Common (and short) key modifier flags.
extern UIKeyModifierFlags KeyModifierNone;
extern UIKeyModifierFlags KeyModifierCommand;
extern UIKeyModifierFlags KeyModifierControl;
extern UIKeyModifierFlags KeyModifierAltCommand;
extern UIKeyModifierFlags KeyModifierShiftCommand;
extern UIKeyModifierFlags KeyModifierShiftAltCommand;
extern UIKeyModifierFlags KeyModifierControlShift;

// DEPRECATED - Do not use in new code. Use action selectors and the responder
// chain.
// Protocol UIResponder subclasses can implement to intercept key commands.
// The implementer must be in the responder chain and be the first to respond to
// this method to be called.
@protocol ChromeKeyCommandHandler

// Called when a registered key command was detected and the receiver is the
// first responder implementing this method in the responder chain.
- (void)cr_handleKeyCommand:(UIKeyCommand*)keyCommand;

@end

// DEPRECATED - Do not use in new code. Use action selectors and the responder
// chain.
// UIApplication is always the last responder. By making it implement the
// ChromeKeyCommandHandler protocol, it catches by default all key commands
// and calls their cr_action.
@interface UIApplication (ChromeKeyCommandHandler) <ChromeKeyCommandHandler>
@end

// DEPRECATED - Do not use in new code. Use action selectors and the responder
// chain.
typedef void (^UIKeyCommandAction)(void);

// Defines a set of one-liner factory methods taking a key command block.
// That way, responders willing to declare and respond to key commands can do it
// in only one place:
//
// foo.mm:
//
// - (NSArray*)keyCommands {
//   __weak AccountsTableViewController* weakSelf = self;
//   return @[
//     [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
//                           modifierFlags:KeyModifierNone
//                                   title:@"Exit"
//                                  action:^{ [[Bar sharedInstance] exit]; }],
//     [UIKeyCommand cr_keyCommandWithInput:@"t"
//                            modifierFlags:KeyModifierCommand
//                                    title:@"New Tab"
//                                   action:^{
//       Foo* strongSelf = weakSelf;
//       if (!strongSelf)
//         return;
//       [strongSelf openNewTab];
//     }],
//   ];
// }
//
// Or in a UIViewController starting in iOS 9:
//
// baz_view_controller.mm:
//
// - (void)viewDidLoad {
//   …
//   [self addKeyCommand:[UIKeyCommand cr_keyCommandWithInput:input
//                                             modifierFlags:modifierFlags
//                                                     title:title
//                                                    action:action]];
//   …
// }
//
// Note: this is implemented as a category on UIKeyCommand because UIKeyCommand
// can't be subclassed as of iOS 9 beta 4. http://crbug.com/510970
@interface UIKeyCommand (Chrome)

// These commands come pre-configured with localized titles (for those that
// appear in the HUD or menu), inputs, and modifier flags. Their action is
// matching their name, where the UIKeyCommand cr_xxx triggers the action method
// keyCommand_xxx.
// Variants are provided if necessary. Variants are named cr_xxx_2, cr_xxx_3,
// etc. They don't have a title and don't appear in the HUD or menu, but trigger
// the same action method keyCommand_xxx.
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewTab_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openNewIncognitoTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reopenClosedTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_openFindInPage;
@property(class, nonatomic, readonly) UIKeyCommand* cr_findNextStringInPage;
@property(class, nonatomic, readonly) UIKeyCommand* cr_findPreviousStringInPage;
@property(class, nonatomic, readonly) UIKeyCommand* cr_focusOmnibox;
@property(class, nonatomic, readonly) UIKeyCommand* cr_closeTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showNextTab_3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showPreviousTab_3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_addToBookmarks;
@property(class, nonatomic, readonly) UIKeyCommand* cr_reload;
@property(class, nonatomic, readonly) UIKeyCommand* cr_goBack;
@property(class, nonatomic, readonly) UIKeyCommand* cr_goForward;
@property(class, nonatomic, readonly) UIKeyCommand* cr_goBack_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_goForward_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showHistory;
@property(class, nonatomic, readonly) UIKeyCommand* cr_startVoiceSearch;
@property(class, nonatomic, readonly) UIKeyCommand* cr_dismissModalDialogs;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showSettings;
@property(class, nonatomic, readonly) UIKeyCommand* cr_stop;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showHelpPage;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showDownloadsFolder;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showDownloadsFolder_2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab0;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab1;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab2;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab3;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab4;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab5;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab6;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showTab7;
@property(class, nonatomic, readonly) UIKeyCommand* cr_showLastTab;

// DEPRECATED - Do not use in new code. Use action selectors and the responder
// chain.
// Block to call when the key command is fired.
@property(nonatomic, copy, setter=cr_setAction:) UIKeyCommandAction cr_action;

// Returns a symbolic description of the key command. For example: ⇧⌘T.
@property(nonatomic, readonly) NSString* cr_symbolicDescription;

// Returns a key command to return in -[UIResponder keyCommands] or to pass to
// -[UIViewController addKeyCommand:].
+ (instancetype)cr_commandWithInput:(NSString*)input
                      modifierFlags:(UIKeyModifierFlags)modifierFlags
                             action:(SEL)action
                            titleID:(int)messageID;

// DEPRECATED - Do not use in new code. Use
// +cr_commandWithInput:modifierFlags:action:titleID: or
// +keyCommandWithInput:modifierFlags:action:.
// TODO(crbug.com/1371848): Remove all usage.
+ (instancetype)cr_keyCommandWithInput:(NSString*)input
                         modifierFlags:(UIKeyModifierFlags)modifierFlags
                                 title:(nullable NSString*)discoveryTitle
                                action:(UIKeyCommandAction)action;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_
