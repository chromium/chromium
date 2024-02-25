// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/hardware_keyboard_util.h"

#import <dlfcn.h>
#import <objc/runtime.h>

#import "base/test/ios/wait_util.h"

#pragma mark - Private API from IOKit to support CreateHIDKeyEvent

typedef UInt32 IOOptionBits;

typedef struct __IOHIDEvent* IOHIDEventRef;

extern "C" {

// This function is privately defined in IOKit framework.
IOHIDEventRef IOHIDEventCreateKeyboardEvent(CFAllocatorRef,
                                            uint64_t,
                                            uint32_t,
                                            uint32_t,
                                            boolean_t,
                                            IOOptionBits);
}

// This enum is defined in IOKit framework.
typedef enum {
  kUIKeyboardInputRepeat = 1 << 0,
  kUIKeyboardInputPopupVariant = 1 << 1,
  kUIKeyboardInputMultitap = 1 << 2,
  kUIKeyboardInputSkipCandidateSelection = 1 << 3,
  kUIKeyboardInputDeadKey = 1 << 4,
  kUIKeyboardInputModifierFlagsChanged = 1 << 5,
  kUIKeyboardInputFlick = 1 << 6,
  kUIKeyboardInputPreProcessed = 1 << 7,
} UIKeyboardInputFlags;

// This enum is defined in IOKit framework.
enum {
  kHIDUsage_KeyboardA = 0x04,
  kHIDUsage_Keyboard1 = 0x1E,
  kHIDUsage_Keyboard2 = 0x1F,
  kHIDUsage_Keyboard3 = 0x20,
  kHIDUsage_Keyboard4 = 0x21,
  kHIDUsage_Keyboard5 = 0x22,
  kHIDUsage_Keyboard6 = 0x23,
  kHIDUsage_Keyboard7 = 0x24,
  kHIDUsage_Keyboard8 = 0x25,
  kHIDUsage_Keyboard9 = 0x26,
  kHIDUsage_Keyboard0 = 0x27,
  kHIDUsage_KeyboardReturnOrEnter = 0x28,
  kHIDUsage_KeyboardEscape = 0x29,
  kHIDUsage_KeyboardDeleteOrBackspace = 0x2A,
  kHIDUsage_KeyboardTab = 0x2B,
  kHIDUsage_KeyboardSpacebar = 0x2C,
  kHIDUsage_KeyboardHyphen = 0x2D,
  kHIDUsage_KeyboardEqualSign = 0x2E,
  kHIDUsage_KeyboardOpenBracket = 0x2F,
  kHIDUsage_KeyboardCloseBracket = 0x30,
  kHIDUsage_KeyboardBackslash = 0x31,
  kHIDUsage_KeyboardSemicolon = 0x33,
  kHIDUsage_KeyboardQuote = 0x34,
  kHIDUsage_KeyboardGraveAccentAndTilde = 0x35,
  kHIDUsage_KeyboardComma = 0x36,
  kHIDUsage_KeyboardPeriod = 0x37,
  kHIDUsage_KeyboardSlash = 0x38,
  kHIDUsage_KeyboardCapsLock = 0x39,
  kHIDUsage_KeyboardF1 = 0x3A,
  kHIDUsage_KeyboardF12 = 0x45,
  kHIDUsage_KeyboardPrintScreen = 0x46,
  kHIDUsage_KeyboardInsert = 0x49,
  kHIDUsage_KeyboardHome = 0x4A,
  kHIDUsage_KeyboardPageUp = 0x4B,
  kHIDUsage_KeyboardDeleteForward = 0x4C,
  kHIDUsage_KeyboardEnd = 0x4D,
  kHIDUsage_KeyboardPageDown = 0x4E,
  kHIDUsage_KeyboardRightArrow = 0x4F,
  kHIDUsage_KeyboardLeftArrow = 0x50,
  kHIDUsage_KeyboardDownArrow = 0x51,
  kHIDUsage_KeyboardUpArrow = 0x52,
  kHIDUsage_KeypadNumLock = 0x53,
  kHIDUsage_KeyboardF13 = 0x68,
  kHIDUsage_KeyboardF24 = 0x73,
  kHIDUsage_KeyboardMenu = 0x76,
  kHIDUsage_KeypadComma = 0x85,
  kHIDUsage_KeyboardLeftControl = 0xE0,
  kHIDUsage_KeyboardLeftShift = 0xE1,
  kHIDUsage_KeyboardLeftAlt = 0xE2,
  kHIDUsage_KeyboardLeftGUI = 0xE3,
  kHIDUsage_KeyboardRightControl = 0xE4,
  kHIDUsage_KeyboardRightShift = 0xE5,
  kHIDUsage_KeyboardRightAlt = 0xE6,
  kHIDUsage_KeyboardRightGUI = 0xE7,
};

// This function is privately defined in IOKit framework.
static uint32_t keyCodeForFunctionKey(NSString* key) {
  // Compare the input string with the function-key names (i.e. "F1",...,"F24").
  for (int i = 1; i <= 12; ++i) {
    if ([key isEqualToString:[NSString stringWithFormat:@"F%d", i]])
      return kHIDUsage_KeyboardF1 + i - 1;
  }
  for (int i = 13; i <= 24; ++i) {
    if ([key isEqualToString:[NSString stringWithFormat:@"F%d", i]])
      return kHIDUsage_KeyboardF13 + i - 13;
  }
  return 0;
}

// This function is privately defined in IOKit framework.
static inline uint32_t hidUsageCodeForCharacter(NSString* key) {
  const int uppercaseAlphabeticOffset = 'A' - kHIDUsage_KeyboardA;
  const int lowercaseAlphabeticOffset = 'a' - kHIDUsage_KeyboardA;
  const int numericNonZeroOffset = '1' - kHIDUsage_Keyboard1;
  if (key.length == 1) {
    // Handle alphanumeric characters and basic symbols.
    int keyCode = [key characterAtIndex:0];
    if (97 <= keyCode && keyCode <= 122)  // Handle a-z.
      return keyCode - lowercaseAlphabeticOffset;

    if (65 <= keyCode && keyCode <= 90)  // Handle A-Z.
      return keyCode - uppercaseAlphabeticOffset;

    if (49 <= keyCode && keyCode <= 57)  // Handle 1-9.
      return keyCode - numericNonZeroOffset;

    // Handle all other cases.
    switch (keyCode) {
      case '`':
      case '~':
        return kHIDUsage_KeyboardGraveAccentAndTilde;
      case '!':
        return kHIDUsage_Keyboard1;
      case '@':
        return kHIDUsage_Keyboard2;
      case '#':
        return kHIDUsage_Keyboard3;
      case '$':
        return kHIDUsage_Keyboard4;
      case '%':
        return kHIDUsage_Keyboard5;
      case '^':
        return kHIDUsage_Keyboard6;
      case '&':
        return kHIDUsage_Keyboard7;
      case '*':
        return kHIDUsage_Keyboard8;
      case '(':
        return kHIDUsage_Keyboard9;
      case ')':
      case '0':
        return kHIDUsage_Keyboard0;
      case '-':
      case '_':
        return kHIDUsage_KeyboardHyphen;
      case '=':
      case '+':
        return kHIDUsage_KeyboardEqualSign;
      case '\b':
        return kHIDUsage_KeyboardDeleteOrBackspace;
      case '\t':
        return kHIDUsage_KeyboardTab;
      case '[':
      case '{':
        return kHIDUsage_KeyboardOpenBracket;
      case ']':
      case '}':
        return kHIDUsage_KeyboardCloseBracket;
      case '\\':
      case '|':
        return kHIDUsage_KeyboardBackslash;
      case ';':
      case ':':
        return kHIDUsage_KeyboardSemicolon;
      case '\'':
      case '"':
        return kHIDUsage_KeyboardQuote;
      case '\r':
      case '\n':
        return kHIDUsage_KeyboardReturnOrEnter;
      case ',':
      case '<':
        return kHIDUsage_KeyboardComma;
      case '.':
      case '>':
        return kHIDUsage_KeyboardPeriod;
      case '/':
      case '?':
        return kHIDUsage_KeyboardSlash;
      case ' ':
        return kHIDUsage_KeyboardSpacebar;
    }
  }

  uint32_t keyCode = keyCodeForFunctionKey(key);
  if (keyCode)
    return keyCode;

  if ([key isEqualToString:@"capsLock"] || [key isEqualToString:@"capsLockKey"])
    return kHIDUsage_KeyboardCapsLock;
  if ([key isEqualToString:@"pageUp"])
    return kHIDUsage_KeyboardPageUp;
  if ([key isEqualToString:@"pageDown"])
    return kHIDUsage_KeyboardPageDown;
  if ([key isEqualToString:@"home"])
    return kHIDUsage_KeyboardHome;
  if ([key isEqualToString:@"insert"])
    return kHIDUsage_KeyboardInsert;
  if ([key isEqualToString:@"end"])
    return kHIDUsage_KeyboardEnd;
  if ([key isEqualToString:@"escape"])
    return kHIDUsage_KeyboardEscape;
  if ([key isEqualToString:@"return"] || [key isEqualToString:@"enter"])
    return kHIDUsage_KeyboardReturnOrEnter;
  if ([key isEqualToString:@"leftArrow"])
    return kHIDUsage_KeyboardLeftArrow;
  if ([key isEqualToString:@"rightArrow"])
    return kHIDUsage_KeyboardRightArrow;
  if ([key isEqualToString:@"upArrow"])
    return kHIDUsage_KeyboardUpArrow;
  if ([key isEqualToString:@"downArrow"])
    return kHIDUsage_KeyboardDownArrow;
  if ([key isEqualToString:@"delete"])
    return kHIDUsage_KeyboardDeleteOrBackspace;
  if ([key isEqualToString:@"forwardDelete"])
    return kHIDUsage_KeyboardDeleteForward;
  if ([key isEqualToString:@"leftCommand"] || [key isEqualToString:@"metaKey"])
    return kHIDUsage_KeyboardLeftGUI;
  if ([key isEqualToString:@"rightCommand"])
    return kHIDUsage_KeyboardRightGUI;
  if ([key isEqualToString:@"clear"])  // Num Lock / Clear
    return kHIDUsage_KeypadNumLock;
  if ([key isEqualToString:@"leftControl"] || [key isEqualToString:@"ctrlKey"])
    return kHIDUsage_KeyboardLeftControl;
  if ([key isEqualToString:@"rightControl"])
    return kHIDUsage_KeyboardRightControl;
  if ([key isEqualToString:@"leftShift"] || [key isEqualToString:@"shiftKey"])
    return kHIDUsage_KeyboardLeftShift;
  if ([key isEqualToString:@"rightShift"])
    return kHIDUsage_KeyboardRightShift;
  if ([key isEqualToString:@"leftAlt"] || [key isEqualToString:@"altKey"])
    return kHIDUsage_KeyboardLeftAlt;
  if ([key isEqualToString:@"rightAlt"])
    return kHIDUsage_KeyboardRightAlt;
  if ([key isEqualToString:@"numpadComma"])
    return kHIDUsage_KeypadComma;

  return 0;
}

typedef NS_OPTIONS(NSInteger, BKSKeyModifierFlags) {
  BKSKeyModifierShift = 1 << 17,
  BKSKeyModifierControl = 1 << 18,
  BKSKeyModifierAlternate = 1 << 19,
  BKSKeyModifierCommand = 1 << 20,
};

@interface BKSHIDEventBaseAttributes : NSObject
@end

@interface BKSHIDEventDigitizerAttributes : BKSHIDEventBaseAttributes
@property(nonatomic) BKSKeyModifierFlags activeModifiers;
@end

// These are privately defined in IOKit framework.
enum { kHIDPage_KeyboardOrKeypad = 0x07, kHIDPage_VendorDefinedStart = 0xFF00 };

enum {
  kIOHIDEventOptionNone = 0,
};

// From
// https://github.com/WebKit/WebKit/blob/32d692229b2aa815346811da6a8db51f260090fe/Source/WTF/wtf/cocoa/SoftLinking.h
#define SOFT_LINK_PRIVATE_FRAMEWORK(framework)                              \
  static void* framework##Library() {                                       \
    static void* frameworkLibrary = ^{                                      \
      void* result = dlopen("/System/Library/PrivateFrameworks/" #framework \
                            ".framework/" #framework,                       \
                            RTLD_NOW);                                      \
      return result;                                                        \
    }();                                                                    \
    return frameworkLibrary;                                                \
  }

#ifdef __cplusplus
#define WTF_EXTERN_C_BEGIN extern "C" {
#define WTF_EXTERN_C_END }
#else
#define WTF_EXTERN_C_BEGIN
#define WTF_EXTERN_C_END
#endif

#define SOFT_LINK(framework, functionName, resultType, parameterDeclarations, \
                  parameterNames)                                             \
  WTF_EXTERN_C_BEGIN                                                          \
  resultType functionName parameterDeclarations;                              \
  WTF_EXTERN_C_END                                                            \
  static resultType init##functionName parameterDeclarations;                 \
  static resultType(*softLink##functionName) parameterDeclarations =          \
      init##functionName;                                                     \
                                                                              \
  static resultType init##functionName parameterDeclarations {                \
    softLink##functionName = (resultType(*) parameterDeclarations)dlsym(      \
        framework##Library(), #functionName);                                 \
    return softLink##functionName parameterNames;                             \
  }                                                                           \
                                                                              \
  inline __attribute__((__always_inline__))                                   \
  resultType functionName parameterDeclarations {                             \
    return softLink##functionName parameterNames;                             \
  }

// From
// https://github.com/WebKit/WebKit/blob/32d692229b2aa815346811da6a8db51f260090fe/Tools/WebKitTestRunner/ios/HIDEventGenerator.mm
SOFT_LINK_PRIVATE_FRAMEWORK(BackBoardServices)
SOFT_LINK(BackBoardServices,
          BKSHIDEventSetDigitizerInfo,
          void,
          (IOHIDEventRef digitizerEvent,
           uint32_t contextID,
           uint8_t systemGestureisPossible,
           uint8_t isSystemGestureStateChangeEvent,
           CFStringRef displayUUID,
           CFTimeInterval initialTouchTimestamp,
           float maxForce),
          (digitizerEvent,
           contextID,
           systemGestureisPossible,
           isSystemGestureStateChangeEvent,
           displayUUID,
           initialTouchTimestamp,
           maxForce))
SOFT_LINK(BackBoardServices,
          BKSHIDEventGetDigitizerAttributes,
          BKSHIDEventDigitizerAttributes*,
          (IOHIDEventRef event),
          (event))

#pragma mark - Private API to fake keyboard events.

// Convenience wrapper for IOHIDEventCreateKeyboardEvent.
IOHIDEventRef CreateHIDKeyEvent(NSString* character,
                                uint64_t timestamp,
                                bool isKeyDown) {
  return IOHIDEventCreateKeyboardEvent(
      kCFAllocatorDefault, timestamp, kHIDPage_KeyboardOrKeypad,
      hidUsageCodeForCharacter(character), isKeyDown, kIOHIDEventOptionNone);
}

// A fake class that mirrors UIPhysicalKeyboardEvent private class' fields.
// This class is never used, but it allows to call UIPhysicalKeyboardEvent's API
// by casting an instance of UIPhysicalKeyboardEvent to PhysicalKeyboardEvent.
@interface PhysicalKeyboardEvent : UIEvent
+ (id)_eventWithInput:(id)arg1 inputFlags:(int)arg2;
- (void)_setHIDEvent:(IOHIDEventRef)event keyboard:(void*)gsKeyboard;
// >=iOS17 only.
- (void)_setModifierFlags:(UIKeyModifierFlags)flags;
// <=iOS16 only.
@property(nonatomic) UIKeyModifierFlags _modifierFlags;
@end

// Private API in UIKit.
@interface UIApplication ()
- (void)_enqueueHIDEvent:(IOHIDEventRef)event;
- (void)handleKeyUIEvent:(id)event;
- (void)handleKeyHIDEvent:(id)event;
@end

@interface UIWindow ()
- (uint32_t)_contextId;
@end

#pragma mark - Implementation

using base::test::ios::kWaitForUIElementTimeout;

namespace {

// Delay between simulated keyboard press events.
const double kKeyPressDelay = 0.02;

// Utility to describe modifier flags. Useful in debugging.
[[maybe_unused]] NSString* DescribeFlags(UIKeyModifierFlags flags);
NSString* DescribeFlags(UIKeyModifierFlags flags) {
  NSMutableString* s = [[NSMutableString alloc] init];
  if (flags & UIKeyModifierAlphaShift) {
    [s appendString:@"CapsLock+"];
  }
  if (flags & UIKeyModifierShift) {
    [s appendString:@"Shift+"];
  }
  if (flags & UIKeyModifierControl) {
    [s appendString:@"Ctrl+"];
  }
  if (flags & UIKeyModifierAlternate) {
    [s appendString:@"Alt+"];
  }
  if (flags & UIKeyModifierCommand) {
    [s appendString:@"Command+"];
  }
  if (flags & UIKeyModifierNumericPad) {
    [s appendString:@"NumPad+"];
  }

  return s;
}

uint32_t GetContextId() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow) {
        return window._contextId;
      }
    }
  }
  return 0;
}

// Sends an individual keyboard press event.
void SendKBEventWithModifiers(UIKeyModifierFlags flags, NSString* input) {
  // Fake up an event.
  PhysicalKeyboardEvent* keyboardEvent =
      [NSClassFromString(@"UIPhysicalKeyboardEvent") _eventWithInput:input
                                                          inputFlags:0];
  if (@available(iOS 17, *)) {
    [keyboardEvent _setModifierFlags:flags];
  } else {
    keyboardEvent._modifierFlags = flags;
  }
  IOHIDEventRef hidEvent =
      CreateHIDKeyEvent(input, keyboardEvent.timestamp, true);
  [keyboardEvent _setHIDEvent:hidEvent keyboard:0];
  [[UIApplication sharedApplication] handleKeyUIEvent:keyboardEvent];
}

// Simulate pressing a hardware keyboard key.
void PressKey(NSString* key) {
  IOHIDEventRef event = CreateHIDKeyEvent(key, mach_absolute_time(), true);
  uint32_t contextId = GetContextId();
  BKSHIDEventSetDigitizerInfo(event, contextId, false, false, NULL, 0, 0);
  [[UIApplication sharedApplication] _enqueueHIDEvent:event];
}

// Simulate releasing a hardware keyboard key.
void ReleaseKey(NSString* key) {
  IOHIDEventRef event = CreateHIDKeyEvent(key, mach_absolute_time(), false);
  uint32_t contextId = GetContextId();
  BKSHIDEventSetDigitizerInfo(event, contextId, false, false, NULL, 0, 0);
  [[UIApplication sharedApplication] _enqueueHIDEvent:event];
}

// Lifts the keypresses one by one.
// Once all keypresses are reversed, executes |completion| on the main thread.
void UnwindFakeKeyboardPressWithFlags(UIKeyModifierFlags flags,
                                      NSString* input,
                                      void (^completion)()) {
  if (flags == 0 && input.length == 0) {
    if (completion) {
      completion();
    }
    return;
  }

  auto block = ^(NSTimer* timer) {
    // First release all the non-modifier
    // keys.
    if (input.length > 0) {
      NSString* remainingInput = [input substringFromIndex:1];

      SendKBEventWithModifiers(flags, remainingInput);
      UnwindFakeKeyboardPressWithFlags(flags, remainingInput, completion);
      return;
    }

    // Unwind the modifier keys.
    for (int i = 16; i < 22; i++) {
      UIKeyModifierFlags flag = 1 << i;
      if (flags & flag) {
        SendKBEventWithModifiers(flags & ~flag, input);
        UnwindFakeKeyboardPressWithFlags(flags & ~flag, input, completion);
      }
    }
  };
  [NSTimer scheduledTimerWithTimeInterval:kKeyPressDelay
                                  repeats:false
                                    block:block];
}

// Programmatically simulates pressing the keys one by one, starting with
// modifier keys and moving to input string through recursion, then calls
// unwindFakeKeyboardPressWithFlags to release the pressed keys in reverse
// order.
// Once all key downs and key ups are simulated, executes |completion| on the
// main thread.
void SimulatePhysicalKeyboardEventInternal(UIKeyModifierFlags flags,
                                           NSString* input,
                                           UIKeyModifierFlags previousFlags,
                                           NSString* previousInput,
                                           void (^completion)()) {
  auto block = ^(NSTimer* timer) {
    // First dial in all the modifier keys.
    for (int i = 15; i < 25; i++) {
      UIKeyModifierFlags flag = 1 << i;
      if (flags & flag) {
        SendKBEventWithModifiers(previousFlags ^ flag, previousInput);
        SimulatePhysicalKeyboardEventInternal(flags & ~flag, input,
                                              previousFlags ^ flag,
                                              previousInput, completion);
        return;
      }
    }

    // Now add the next input char.
    if (input.length > 0) {
      NSString* pressedKey = [input substringToIndex:1];
      NSString* remainingInput = [input substringFromIndex:1];
      NSString* alreadyPressedString =
          [previousInput stringByAppendingString:pressedKey];

      SendKBEventWithModifiers(previousFlags, alreadyPressedString);
      SimulatePhysicalKeyboardEventInternal(flags, remainingInput,
                                            previousFlags, alreadyPressedString,
                                            completion);
    } else {
      // Time to unwind the presses.
      UnwindFakeKeyboardPressWithFlags(previousFlags, previousInput,
                                       completion);
    }
  };
  [NSTimer scheduledTimerWithTimeInterval:kKeyPressDelay
                                  repeats:false
                                    block:block];
}

}  // namespace

#pragma mark - Public

namespace chrome_test_util {

void SimulatePhysicalKeyboardEvent(UIKeyModifierFlags flags, NSString* input) {
  // No modifier keys pressed.
  if (flags == 0) {
    if (hidUsageCodeForCharacter(input)) {
      // Input is a keyboard key.
      PressKey(input);
      ReleaseKey(input);
      return;
    }

    // If there are no modifier keys and the input is not a keyboard key. Our
    // intention is then to type the input.
    for (NSUInteger i = 0; i < [input length]; i++) {
      NSRange range = NSMakeRange(i, 1);
      NSString* key = [input substringWithRange:range];
      PressKey(key);
      ReleaseKey(key);
    }
    return;
  }

  // Input with modifier keys.
  __block BOOL keyPressesFinished = NO;

  SimulatePhysicalKeyboardEventInternal(flags, input, 0, @"", ^{
    keyPressesFinished = YES;
  });

  BOOL __unused result =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^{
        return keyPressesFinished;
      });
}

}  // namespace chrome_test_util
