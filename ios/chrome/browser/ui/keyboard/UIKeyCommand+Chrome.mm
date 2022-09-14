// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#import <objc/runtime.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIKeyModifierFlags Cr_UIKeyModifierNone = 0;

@implementation UIApplication (ChromeKeyCommandHandler)

- (void)cr_handleKeyCommand:(UIKeyCommand*)keyCommand {
  [keyCommand cr_action]();
}

@end

@implementation UIKeyCommand (Chrome)

#pragma mark - Block

- (UIKeyCommandAction _Nonnull)cr_action {
  return objc_getAssociatedObject(self, @selector(cr_action));
}

- (void)cr_setAction:(UIKeyCommandAction _Nonnull)action {
  objc_setAssociatedObject(self, @selector(cr_action), action,
                           OBJC_ASSOCIATION_COPY_NONATOMIC);
}

#pragma mark - Symbolic Description

- (NSString*)cr_symbolicDescription {
  NSMutableString* description = [NSMutableString string];

  if (self.modifierFlags & UIKeyModifierNumericPad)
    [description appendString:@"Num lock "];
  if (self.modifierFlags & UIKeyModifierControl)
    [description appendString:@"⌃"];
  if (self.modifierFlags & UIKeyModifierShift)
    [description appendString:@"⇧"];
  if (self.modifierFlags & UIKeyModifierAlphaShift)
    [description appendString:@"⇪"];
  if (self.modifierFlags & UIKeyModifierAlternate)
    [description appendString:@"⌥"];
  if (self.modifierFlags & UIKeyModifierCommand)
    [description appendString:@"⌘"];

  if ([self.input isEqualToString:@"\b"])
    [description appendString:@"⌫"];
  else if ([self.input isEqualToString:@"\r"])
    [description appendString:@"↵"];
  else if ([self.input isEqualToString:@"\t"])
    [description appendString:@"⇥"];
  else if ([self.input isEqualToString:UIKeyInputUpArrow])
    [description appendString:@"↑"];
  else if ([self.input isEqualToString:UIKeyInputDownArrow])
    [description appendString:@"↓"];
  else if ([self.input isEqualToString:UIKeyInputLeftArrow])
    [description appendString:@"←"];
  else if ([self.input isEqualToString:UIKeyInputRightArrow])
    [description appendString:@"→"];
  else if ([self.input isEqualToString:UIKeyInputEscape])
    [description appendString:@"⎋"];
  else if ([self.input isEqualToString:@" "])
    [description appendString:@"␣"];
  else
    [description appendString:[self.input uppercaseString]];
  return description;
}

#pragma mark - Factory

+ (nonnull instancetype)
cr_keyCommandWithInput:(nonnull NSString*)input
         modifierFlags:(UIKeyModifierFlags)modifierFlags
                 title:(nullable NSString*)discoveryTitle
                action:(nonnull UIKeyCommandAction)action {
  UIKeyCommand* keyCommand =
      [self keyCommandWithInput:input
                  modifierFlags:modifierFlags
                         action:@selector(cr_handleKeyCommand:)];
  keyCommand.discoverabilityTitle = discoveryTitle;
  keyCommand.cr_action = action;
  return keyCommand;
}

@end
