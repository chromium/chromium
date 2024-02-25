// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/protocol_fake.h"

#import <objc/runtime.h>

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"

namespace {
// Opaque value to use as an associated object key.
char kAssociatedProtocolNameKey;
}  // namespace

@interface NSInvocation (Description)
// Returns a string description of the invocation consisting of the selector
// name interspersed with argument values.
- (NSString*)crsc_description;
@end

@interface ProtocolFake () {
  NSSet<Protocol*>* _protocols;
  // Selectors for which no logging should be done.
  NSMutableSet<NSValue*>* _ignoredSelectors;
  // Count of selector calls.
  NSMutableDictionary<NSValue*, NSNumber*>* _callCounts;
}
@end

@implementation ProtocolFake

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithProtocols:(NSArray<Protocol*>*)protocols {
  if (!protocols) {
    return nil;
  }
  // NSProxy isn't a subclass of NSObject, and has no superclass, so
  // there's no [super init] to call.
  _protocols = [[NSSet<Protocol*> alloc] initWithArray:protocols];
  _ignoredSelectors = [NSMutableSet set];
  _callCounts = [NSMutableDictionary dictionary];
  // Log by default.
  _logs = YES;
  return self;
}

- (NSInteger)callCountForSelector:(SEL)sel {
  return _callCounts[[NSValue valueWithPointer:sel]].integerValue;
}

- (void)ignoreSelector:(SEL)sel {
  [_ignoredSelectors addObject:[NSValue valueWithPointer:sel]];
}

#pragma mark - NSProxy

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  for (Protocol* protocol in _protocols) {
    const BOOL kYesAndNoArray[] = {YES, NO};
    for (BOOL required : kYesAndNoArray) {
      struct objc_method_description method = protocol_getMethodDescription(
          protocol, sel, required, YES /* an instance method */);
      if (method.name != NULL) {
        NSMethodSignature* signature =
            [NSMethodSignature signatureWithObjCTypes:method.types];
        // Tag the method signature with the protocol name.
        objc_setAssociatedObject(signature, &kAssociatedProtocolNameKey,
                                 NSStringFromProtocol(protocol),
                                 OBJC_ASSOCIATION_COPY_NONATOMIC);
        return signature;
      }
    }
  }
  return nil;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  NSValue* selectorValue = [NSValue valueWithPointer:invocation.selector];
  if ([_ignoredSelectors containsObject:selectorValue]) {
    return;
  }

  NSNumber* count = _callCounts[selectorValue];
  if (count) {
    _callCounts[selectorValue] = @(count.integerValue + 1);
  } else {
    _callCounts[selectorValue] = @1;
  }

  // Instead of actually doing anything the protocol method would normally
  // do, instead just generate a title and description and display an alert or
  // log a message.
  NSString* protocolName = objc_getAssociatedObject(
      [self methodSignatureForSelector:invocation.selector],
      &kAssociatedProtocolNameKey);
  NSString* description = [invocation crsc_description];

  if (self.alerts && self.baseViewController) {
    [self showAlertWithTitle:protocolName message:description];
  }

  if (self.logs) {
    VLOG(0) << "Alerter -- protocol:" << base::SysNSStringToUTF8(protocolName);
    VLOG(0) << "Alerter -- invocation:" << base::SysNSStringToUTF8(description);
  }
}

#pragma mark - NSObject

- (BOOL)conformsToProtocol:(Protocol*)aProtocol {
  for (Protocol* protocol in _protocols) {
    // Handle protocols that conform to other protocols.
    if (protocol_conformsToProtocol(protocol, aProtocol)) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  return [self methodSignatureForSelector:aSelector] != nil;
}

#pragma mark - Private

// Helper to show a simple alert.
- (void)showAlertWithTitle:(NSString*)title message:(NSString*)message {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [action setAccessibilityLabel:@"protocol_alerter_done"];
  [alertController addAction:action];
  [self.baseViewController presentViewController:alertController
                                        animated:YES
                                      completion:nil];
}

@end

@implementation NSInvocation (Description)

- (NSString*)crsc_description {
  NSInteger arguments = self.methodSignature.numberOfArguments;
  NSString* selector = NSStringFromSelector(self.selector);

  // NSInvocation's first two arguments are `self` and `_cmd`; if they are the
  // only ones, then the invocation has no actual arguments.
  if (arguments == 2) {
    return selector;
  }

  // Get the parts of the selector name by splitting on /:/, and dropping the
  // last (empty) part.
  NSArray* keywords = [[selector componentsSeparatedByString:@":"]
      subarrayWithRange:NSMakeRange(0, arguments - 2)];
  NSMutableString* description = [[NSMutableString alloc] init];
  NSInteger argumentIndex = 2;
  for (NSString* keyword in keywords) {
    // Insert a space before each keyword after the first one.
    if (description.length) {
      [description appendString:@" "];
    }
    [description appendString:keyword];
    [description appendString:@":"];
    [description
        appendString:[self crsc_argumentDescriptionAtIndex:argumentIndex]];
    argumentIndex++;
  }

  return description;
}

// Return a string describing the argument value at `index`.
// (`index` is in NSInvocation's argument array).
- (NSString*)crsc_argumentDescriptionAtIndex:(NSInteger)index {
  const char* type = [self.methodSignature getArgumentTypeAtIndex:index];

  switch (*type) {
    case '@':
      return [self crsc_objectDescriptionAtIndex:index];
    case 'q':
      return [self crsc_int64DescriptionAtIndex:index];
    case 'Q':
      return [self crsc_unsignedInt64DescriptionAtIndex:index];
    // Add cases as needed here.
    default:
      return [NSString stringWithFormat:@"<Unknown Type:%s>", type];
  }
}

// Return a string describing an argument at `index` that's known to be an
// objective-C object.
- (NSString*)crsc_objectDescriptionAtIndex:(NSInteger)index {
  // This is one of the few cases where __unsafe_unretained is correct; this
  // allocates memory for an empty generic object that `getArgument:` will
  // write in to.
  __unsafe_unretained id object;

  [self getArgument:&object atIndex:index];
  if (!object) {
    return @"nil";
  }

  NSString* description = [object description];
  NSString* className = NSStringFromClass([object class]);
  if (!description) {
    return
        [NSString stringWithFormat:@"<%@ object, no description>", className];
  }

  // Wrap strings in @" ... ".
  if ([object isKindOfClass:[NSString class]]) {
    return [NSString stringWithFormat:@"@\"%@\"", description];
  }

  // Remove the address of objects from their descriptions, so (for example):
  //   <NSObject: 0xc00lf0ccac1a>
  // becomes just:
  //   <NSObject>
  NSRange range = NSMakeRange(0, description.length);
  NSString* classPlusAddress =
      [className stringByAppendingString:@": 0x[0-9a-c]+"];
  return [description
      stringByReplacingOccurrencesOfString:classPlusAddress
                                withString:className
                                   options:NSRegularExpressionSearch
                                     range:range];
}

// Returns a string describing an argument at `index` that is known to be a
// 64-bit integer (a "long long").
- (NSString*)crsc_int64DescriptionAtIndex:(NSInteger)index {
  int64_t value;

  [self getArgument:&value atIndex:index];
  return [NSString stringWithFormat:@"%lld", value];
}

// Returns a string describing an argument at `index` that is known to be an
// unsigned 64-bit integer (an "unsigned long long").
- (NSString*)crsc_unsignedInt64DescriptionAtIndex:(NSInteger)index {
  uint64_t value;

  [self getArgument:&value atIndex:index];
  return [NSString stringWithFormat:@"%llu", value];
}

@end
