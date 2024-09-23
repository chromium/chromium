// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

#import <objc/runtime.h>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/notreached.h"

namespace {

// Constructs an NSInvocation that will be used for repeated execution of
// `selector`. `selector` must return void and take exactly one argument; it is
// an error otherwise.
NSInvocation* InvocationForBroadcasterSelector(SEL selector) {
  struct objc_method_description methodDesc = protocol_getMethodDescription(
      @protocol(ChromeBroadcastObserver), selector,
      NO /* not a required method */, YES /* an instance method */);
  DCHECK(methodDesc.types);
  NSMethodSignature* method =
      [NSMethodSignature signatureWithObjCTypes:methodDesc.types];
  DCHECK(method);
  // There should always be exactly three arguments: the two implicit arguments
  // that every Objective-C method has (self and _cmd), and the single value
  // argument for the broadcast value.
  DCHECK(method.numberOfArguments == 3);

  // Methods should always return void.
  DCHECK(strcmp(method.methodReturnType, @encode(void)) == 0);

  NSInvocation* invocation =
      [NSInvocation invocationWithMethodSignature:method];
  invocation.selector = selector;
  return invocation;
}
}

// Protocol observer subclass that explicitly implements <BroadcastObserver>.
// Mostly this is used for the non-retaining observer set; this requires
// observers to be removed before they dealloc. It would be better to track
// observer lifetime via associated objects and remove them automatically.
@interface BroadcastObservers : CRBProtocolObservers<ChromeBroadcastObserver>
+ (instancetype)observers;
@end

@implementation BroadcastObservers
+ (instancetype)observers {
  return [self observersWithProtocol:@protocol(ChromeBroadcastObserver)];
}
@end

// An object that collects the information about a single observed property.
@interface BroadcastItem : NSObject
// The observed object.
@property(nonatomic, readonly) NSObject* object;
// The observed key path.
@property(nonatomic, readonly, copy) NSString* key;
// The name associated with this observation.
@property(nonatomic, readonly, copy) NSString* name;
// The current value of `key` on `object`.
@property(nonatomic, readonly) NSValue* currentValue;

// Designated initializer.
- (instancetype)initWithObject:(NSObject*)object
                           key:(NSString*)key
                          name:(NSString*)name NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Add `observer` as a KVO of the object and key represented by the receiver.
- (void)addObserver:(NSObject*)observer;
// Remove `observer` from the KVO represented by the receiver.
- (void)removeObserver:(NSObject*)observer;
@end

@implementation BroadcastItem
@synthesize object = _object;
@synthesize key = _key;
@synthesize name = _name;

- (instancetype)initWithObject:(NSObject*)object
                           key:(NSString*)key
                          name:(NSString*)name {
  if ((self = [super init])) {
    _object = object;
    _key = [key copy];
    _name = [name copy];
  }
  return self;
}

- (NSValue*)currentValue {
  return [self.object valueForKey:self.key];
}

- (void)addObserver:(NSObject*)observer {
  // Important: because the NSKeyValueObservingOptionInitial is passed in,
  // addObserver:forKeyPath:options:context: will trigger a notification before
  // it returns, so all of the infrastructure for handling the notification must
  // be in place before the -addObserver... call.
  NSKeyValueObservingOptions options = NSKeyValueObservingOptionNew |
                                       NSKeyValueObservingOptionOld |
                                       NSKeyValueObservingOptionInitial;

  // So that the selector name to be used for this object/key pair can be
  // retrieved, it's added as an opaque 'context' object. These names are
  // constant strings used as keys in the immutable _observerInvocations
  // dictionary, which will thus live as long as this object does.
  [self.object addObserver:observer
                forKeyPath:self.key
                   options:options
                   context:(__bridge void*)self.name];
}

- (void)removeObserver:(NSObject*)observer {
  [self.object removeObserver:observer
                   forKeyPath:self.key
                      context:(__bridge void*)self.name];
}

@end

@interface ChromeBroadcaster ()
// Map of selectors (as strings) to observers.
@property(nonatomic, readonly)
    NSMutableDictionary<NSString*, BroadcastObservers*>* observers;
// Map of selectors (as strings) to broadcast items.
@property(nonatomic, readonly)
    NSMutableDictionary<NSString*, BroadcastItem*>* items;
// Map of selectors (as strings) to invocations to be called on observers.
// Invocations should be fetched from this dictionary via the
// -invocationForName:value: method.
@property(nonatomic, readonly)
    NSDictionary<NSString*, NSInvocation*>* observerInvocations;
@end

@implementation ChromeBroadcaster
@synthesize observers = _observers;
@synthesize items = _items;
@synthesize observerInvocations = _observerInvocations;

- (instancetype)init {
  if ((self = [super init])) {
    _observers =
        [[NSMutableDictionary<NSString*, BroadcastObservers*> alloc] init];
    _items = [[NSMutableDictionary<NSString*, BroadcastItem*> alloc] init];

    // Pre-build the map of selector names to invocations.  The source of
    // selectors is the optional methods defined (directly) in the
    // BroadcastObserver protocol.
    NSMutableDictionary<NSString*, NSInvocation*>* observerInvocations =
        [[NSMutableDictionary<NSString*, NSInvocation*> alloc] init];

    unsigned int methodCount;
    objc_method_description* instanceMethods =
        protocol_copyMethodDescriptionList(
            @protocol(ChromeBroadcastObserver), NO /* not required methods */,
            YES /* instance methods */, &methodCount);

    for (unsigned int i = 0; i < methodCount; i++) {
      struct objc_method_description method = instanceMethods[i];
      NSString* name = NSStringFromSelector(method.name);
      observerInvocations[name] = InvocationForBroadcasterSelector(method.name);
    }
    free(instanceMethods);

    _observerInvocations = [observerInvocations copy];
  }
  return self;
}

- (void)dealloc {
  for (NSString* name in self.items.allKeys) {
    [self stopBroadcastingForSelector:NSSelectorFromString(name)];
  }
}

- (void)broadcastValue:(NSString*)valueKey
              ofObject:(NSObject*)object
              selector:(SEL)selector {
  NSString* name = NSStringFromSelector(selector);
  // Sanity check: `selector` must be one of the selectors that are mapped.
  DCHECK(self.observerInvocations[name]);
  // Sanity check: `selector` must not already be broadcast.
  DCHECK(!self.items[name]);

  // TODO(crbug.com/40519578) -- Another sanity check is needed here -- verify
  // that the value to be observed is of the type that `selector` expects.

  self.items[name] =
      [[BroadcastItem alloc] initWithObject:object key:valueKey name:name];

  [self.items[name] addObserver:self];
}

// This is usually only needed when the broadcasting object goes away, since
// it's an exception for an object with key-value observers to dealloc. This
// should just be handled by associating monitor objects with the broadcasting
// object instead.
- (void)stopBroadcastingForSelector:(SEL)selector {
  NSString* name = NSStringFromSelector(selector);
  [self.items[name] removeObserver:self];
  [self.items removeObjectForKey:name];
}

- (void)addObserver:(id<ChromeBroadcastObserver>)observer
        forSelector:(SEL)selector {
  NSString* name = NSStringFromSelector(selector);
  // Sanity check: `selector` must be one of the keys that are mapped.
  DCHECK(self.observerInvocations[name]);
  // Sanity check: `observer` must implement the selector for `selector`.
  DCHECK([observer respondsToSelector:selector]);

  if (!self.observers[name])
    self.observers[name] = [BroadcastObservers observers];

  // If the key is already being broadcast, update the observer immediately.
  if (self.items[name]) {
    NSInvocation* call =
        [self invocationForName:name value:self.items[name].currentValue];
    [call invokeWithTarget:observer];
  }

  [self.observers[name] addObserver:observer];
}

- (void)removeObserver:(id<ChromeBroadcastObserver>)observer
           forSelector:(SEL)selector {
  NSString* name = NSStringFromSelector(selector);
  // Sanity check: `selector` must be one of the selectors that are mapped.
  DCHECK(self.observerInvocations[name]);

  [self.observers[name] removeObserver:observer];
  if (self.observers[name].empty)
    [self.observers removeObjectForKey:name];
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  // Bridge cast the context back to a selector name.
  NSString* name = (__bridge NSString*)context;
  // Sanity check: `name` must be one of the selectors that are mapped.
  DCHECK(self.observerInvocations[name]);
  // Sanity check: `object` should be the object currently being observed for
  // `name`.
  DCHECK(self.items[name].object == object);

  BroadcastObservers* observers = self.observers[name];
  if (!observers)
    return;

  // Sanity check: this isn't a change to a collection -- where the observed
  // property is a collection object and this change is (for example) the
  // addition of a new object to the collection. That kind of KVO isn't
  // supported by ChromeBroadcaster.
  DCHECK([change[NSKeyValueChangeKindKey]
      isEqualToValue:@(NSKeyValueChangeSetting)]);

  // If strings or other non-value types are being broadcast, then this will
  // need to change. Either value will be nil if they aren't actually NSValues.
  NSValue* newValue =
      base::apple::ObjCCast<NSValue>(change[NSKeyValueChangeNewKey]);
  NSValue* oldValue =
      base::apple::ObjCCast<NSValue>(change[NSKeyValueChangeOldKey]);

  // If the value is unchanged -- if the old and new values are equal -- then
  // return without notifying observers.
  // -isEqualToValue doesn't deal with nil arguments well, so nil check oldValue
  // here.
  if (oldValue && [newValue isEqualToValue:oldValue])
    return;

  NSInvocation* call = [self invocationForName:name value:newValue];

  [call invokeWithTarget:observers];
}

#pragma mark - internal

// Returns the invocation for the selector named `name`, populated with
// `value` as the argument.
// This method mutates the invocations stored in `self.observerInvocations`, so
// any code that gets an invocation from that dictionary to be invoked should
// do so through this method.
- (NSInvocation*)invocationForName:(NSString*)name value:(NSValue*)value {
  NSInvocation* invocation = self.observerInvocations[name];
  // Attempt to cast `value` into an NSNumber; ObjCCast will instead return
  // nil if this isn't possible.
  NSNumber* valueAsNumber = base::apple::ObjCCast<NSNumber>(value);
  std::string type([invocation.methodSignature getArgumentTypeAtIndex:2]);

  if (type == @encode(BOOL)) {
    DCHECK(valueAsNumber);
    BOOL boolValue = valueAsNumber.boolValue;
    [invocation setArgument:&boolValue atIndex:2];
  } else if (type == @encode(CGFloat)) {
    DCHECK(valueAsNumber);
// CGFloat is a float on 32-bit devices, but a double on 64-bit devices.
#if CGFLOAT_IS_DOUBLE
    CGFloat cgfloatValue = valueAsNumber.doubleValue;
#else
    CGFloat cgfloatValue = valueAsNumber.floatValue;
#endif
    [invocation setArgument:&cgfloatValue atIndex:2];
  } else if (type == @encode(CGRect)) {
    CGRect rectValue = value.CGRectValue;
    [invocation setArgument:&rectValue atIndex:2];
  } else if (type == @encode(CGSize)) {
    CGSize sizeValue = value.CGSizeValue;
    [invocation setArgument:&sizeValue atIndex:2];
  } else if (type == @encode(UIEdgeInsets)) {
    UIEdgeInsets insetValue = value.UIEdgeInsetsValue;
    [invocation setArgument:&insetValue atIndex:2];
  } else if (type == @encode(int)) {
    DCHECK(valueAsNumber);
    int intValue = valueAsNumber.intValue;
    [invocation setArgument:&intValue atIndex:2];
  } else {
    // Add more clauses as needed.
    NOTREACHED_IN_MIGRATION() << "Unknown argument type: " << type;
    return nil;
  }

  return invocation;
}

@end
