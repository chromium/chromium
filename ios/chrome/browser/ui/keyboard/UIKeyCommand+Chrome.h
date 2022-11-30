// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

// Protocol UIResponder subclasses can implement to intercept key commands.
// The implementer must be in the responder chain and be the first to respond to
// this method to be called.
@protocol ChromeKeyCommandHandler

// Called when a registered key command was detected and the receiver is the
// first responder implementing this method in the responder chain.
- (void)cr_handleKeyCommand:(UIKeyCommand*)keyCommand;

@end

// UIApplication is always the last responder. By making it implement the
// ChromeKeyCommandHandler protocol, it catches by default all key commands
// and calls their cr_action.
@interface UIApplication (ChromeKeyCommandHandler)<ChromeKeyCommandHandler>
@end

typedef void (^UIKeyCommandAction)(void);

// Addition to the set of predefined modifier flags.
extern UIKeyModifierFlags Cr_UIKeyModifierNone;

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
//                           modifierFlags:Cr_UIKeyModifierNone
//                                   title:@"Exit"
//                                  action:^{ [[Bar sharedInstance] exit]; }],
//     [UIKeyCommand cr_keyCommandWithInput:@"t"
//                            modifierFlags:UIKeyModifierCommand
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

// Block to call when the key command is fired.
@property(nonatomic, copy, setter=cr_setAction:) UIKeyCommandAction cr_action;
// Returns a symbolic description of the key command. For example: ⇧⌘T.
@property(nonatomic, readonly) NSString* cr_symbolicDescription;

// Returns a key command to return in -[UIResponder keyCommands] or to pass to
// -[UIViewController addKeyCommand:].
+ (instancetype)cr_keyCommandWithInput:(NSString*)input
                         modifierFlags:(UIKeyModifierFlags)modifierFlags
                                 title:(nullable NSString*)discoveryTitle
                                action:(UIKeyCommandAction)action;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_UIKEYCOMMAND_CHROME_H_
