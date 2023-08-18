// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_scroll_view_proxy+internal.h"

#import <objc/runtime.h>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/auto_reset.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/web_state/ui/crw_web_view_scroll_view_delegate_proxy.h"

// *Address of* this variable is used as a marker to specify that it matches any
// context.
static int gAnyContext = 0;

// A wrapper of a key-value observer. When an instance of
// CRWKeyValueObserverForwarder receives a KVO callback, it forwards the
// callback to `wrappedObserver`, but replacing the object parameter with the
// `object` given in its initializer.
//
// This is useful when creating a proxy class of an object and forwarding KVO
// against the proxy object to the underlying object, but making the KVO
// callback still look like a callback from the proxy object.
@interface CRWKeyValueObserverForwarder : NSObject

@property(nonatomic, weak) id wrappedObserver;
@property(nonatomic, weak) id object;
@property(nonatomic) NSKeyValueObservingOptions options;
@property(nonatomic) void* context;

- (instancetype)initWithWrappedObserver:(id)wrappedObserver
                                 object:(id)object
                                options:(NSKeyValueObservingOptions)options
                                context:(void*)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation CRWKeyValueObserverForwarder

- (instancetype)initWithWrappedObserver:(id)wrappedObserver
                                 object:(id)object
                                options:(NSKeyValueObservingOptions)options
                                context:(void*)context {
  self = [super init];
  if (self) {
    _wrappedObserver = wrappedObserver;
    _object = object;
    _options = options;
    _context = context;
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [self.wrappedObserver observeValueForKeyPath:keyPath
                                      ofObject:self.object
                                        change:change
                                       context:context];
}

@end

@interface CRWWebViewScrollViewProxy ()

// A delegate object of the UIScrollView managed by this class.
@property(nonatomic, strong, readonly)
    CRWWebViewScrollViewDelegateProxy* delegateProxy;

@property(nonatomic, strong)
    CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>* observers;

@property(nonatomic, strong) UIScrollView* underlyingScrollView;

// This exists for compatibility with UIScrollView (see -asUIScrollView).
@property(nonatomic, weak) id<UIScrollViewDelegate> delegate;

// Wrappers of key-value observers against this instance, keyed by:
//   - the key path (the outer dictionary)
//   - NSValue representation of an unretained pointer to the observer (the
//     inner dictionary).
//
// This dictionary must hold an *unretained* pointer to the observer, neither a
// strong or weak pointer.
//   - An object should not retain its key-value observer. So it must not be a
//     strong pointer.
//   - The dictionary may be accessed during -dealloc of an observer. This is
//     quite possible because it is common that an observer calls
//     -removeObserver:forKeyPath: during its -dealloc, and this dictionary is
//     accessed in -removeObserver:forKeyPath:. And a weak pointer to an object
//     is not available during its -dealloc.
//
// And holding NSValue wrapping the pointer is the only way to use an unretained
// pointer as a key of a dictionary. NSMapTable supports using a weak pointer
// for its keys, but not an unretained pointer.
//
// Use of an unretained pointer here is safe because it is never dereferenced,
// and the observer must call -removeObserver:forKeyPath: before it is
// deallocated.
@property(nonatomic, strong) NSMutableDictionary<
    NSString*,
    NSMutableDictionary<NSValue*,
                        NSMutableArray<CRWKeyValueObserverForwarder*>*>*>*
    keyValueObserverForwarders;

// Returns the key paths that need to be observed for UIScrollView.
+ (NSArray*)scrollViewObserverKeyPaths;

// Adds and removes key-value observers for `scrollView` needed by `proxy`.
+ (void)startObservingScrollView:(UIScrollView*)scrollView
                           proxy:(CRWWebViewScrollViewProxy*)proxy;
+ (void)stopObservingScrollView:(UIScrollView*)scrollView
                          proxy:(CRWWebViewScrollViewProxy*)proxy;

@end

// Note: An instance of this class must be safely casted to UIScrollView. See
// -asUIScrollView. To make it happen:
//   - When this class defines a method with the same selector as in a method of
//     UIScrollView (or its ancestor classes), its API and the behavior should
//     be consistent with the UIScrollView one's.
//   - Calls to UIScrollView methods not implemented in this class are forwarded
//     to the underlying UIScrollView by -methodSignatureForSelector: and
//     -forwardInvocation:.
@implementation CRWWebViewScrollViewProxy

- (instancetype)init {
  self = [super init];
  if (self) {
    Protocol* protocol = @protocol(CRWWebViewScrollViewProxyObserver);
    _observers =
        static_cast<CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>*>(
            [CRBProtocolObservers observersWithProtocol:protocol]);
    _delegateProxy = [[CRWWebViewScrollViewDelegateProxy alloc]
        initWithScrollViewProxy:self];
    _keyValueObserverForwarders = [[NSMutableDictionary alloc] init];

    // Assign a placeholder UIScrollView until the actual underlying scroll view
    // is set. This must be a real UIScrollView, not nil, so that:
    //   - The proxy preserves the values of properties assigned before the
    //     actual scroll view is set. These properties will then be inherited to
    //     the actual scroll view in -setScrollView:.
    //   - The proxy returns the actual default value of the property before the
    //     actual scroll view is set, even when the default value is non-zero
    //     e.g., scrollsToTop.
    //   - The proxy uses the actual implementation of methods defined in
    //     third-party categories of UIScrollView.
    //
    // Note that this proxy must support all methods/properties of UIScrollView,
    // including those defined in third-party categories, because it provides
    // -asUIScrollView method.
    _underlyingScrollView = [[UIScrollView alloc] init];

    // There are a few properties where the default WKWebView.scrollView has
    // different values from a base UIScrollView. As _underlyingScrollView
    // starts out as a base UIScrollView, the property preservation code will
    // copy over these incorrect values and overwrite the default
    // WKWebView.scrollView values for those properties. Instead, set those
    // values to their WebKit defaults.
    _underlyingScrollView.alwaysBounceVertical = YES;
    _underlyingScrollView.directionalLockEnabled = YES;

    [self.class startObservingScrollView:_underlyingScrollView proxy:self];
  }
  return self;
}

- (void)dealloc {
  [self.class stopObservingScrollView:self.underlyingScrollView proxy:self];
}

- (void)addObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)setScrollView:(UIScrollView*)scrollView {
  if (self.underlyingScrollView == scrollView)
    return;

  // Use a placeholder UIScrollView instead when nil is given. See the comment
  // in -init why this is necessary.
  if (!scrollView) {
    scrollView = [[UIScrollView alloc] init];
  }

  // Clean up the delegate/observers of the old scroll view.
  [self.underlyingScrollView setDelegate:nil];
  [self.class stopObservingScrollView:self.underlyingScrollView proxy:self];

  // Set up the delegate/observers of the new scroll view.
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self.delegateProxy;
  [self.class startObservingScrollView:scrollView proxy:self];

  [self preservePropertiesFromOldScrollView:self.underlyingScrollView
                            toNewScrollView:scrollView];

  self.underlyingScrollView = scrollView;

  [_observers webViewScrollViewProxyDidSetScrollView:self];
}

// Preserves properties of the underlying scroll view when it changes from
// `oldScrollView` to `newScrollView`.
//
// This is necessary to avoid losing properties set against the proxy when the
// underlying scroll view is reset.
- (void)preservePropertiesFromOldScrollView:(UIScrollView*)oldScrollView
                            toNewScrollView:(UIScrollView*)newScrollView {
  // This method should preserve all properties of UIScrollView and its
  // ancestor classes (not limited to properties explicitly declared in
  // CRWWebViewScrollViewProxy) which:
  //   - is a readwrite property
  //   - AND is supposed to be modified directly, considering it's a scroll
  //     view of a web view. e.g., `frame` and `subviews` do not meet this
  //     condition because they are managed by the web view.  `backgroundColor`
  //     is also managed by WKWebView to match the page's background color, and
  //     should not be set directly (see crbug.com/1078790).
  //
  // Properties not explicitly declared in CRWWebViewScrollViewProxy can still
  // be accessed via -asUIScrollView, so they should be preserved as well.

  // UIScrollView properties.
  if (base::FeatureList::IsEnabled(
          web::features::kScrollViewProxyScrollEnabledWorkaround)) {
    if (newScrollView.scrollEnabled != oldScrollView.scrollEnabled) {
      // Don't update scrollEnabled if it is the same value as it creates issues
      // with clobbering state in WebKit, since the getter and setter in WebKit
      // are not symmetric. The setter sets state about whether the WKWebView
      // embedder wants to disable scrolling, while the getter and used value
      // also account for whether the main-frame is scrollable (e.g., due to the
      // size of its content relative to the viewport). See crbug.com/1375837.
      newScrollView.scrollEnabled = oldScrollView.scrollEnabled;
    }
  } else {
    newScrollView.scrollEnabled = oldScrollView.scrollEnabled;
  }
  newScrollView.directionalLockEnabled = oldScrollView.directionalLockEnabled;
  newScrollView.pagingEnabled = oldScrollView.pagingEnabled;
  newScrollView.scrollsToTop = oldScrollView.scrollsToTop;
  newScrollView.bounces = oldScrollView.bounces;
  newScrollView.alwaysBounceVertical = oldScrollView.alwaysBounceVertical;
  newScrollView.alwaysBounceHorizontal = oldScrollView.alwaysBounceHorizontal;
  newScrollView.showsHorizontalScrollIndicator =
      oldScrollView.showsHorizontalScrollIndicator;
  newScrollView.showsVerticalScrollIndicator =
      oldScrollView.showsVerticalScrollIndicator;
  newScrollView.canCancelContentTouches = oldScrollView.canCancelContentTouches;
  newScrollView.delaysContentTouches = oldScrollView.delaysContentTouches;
  newScrollView.keyboardDismissMode = oldScrollView.keyboardDismissMode;
  newScrollView.indexDisplayMode = oldScrollView.indexDisplayMode;
  newScrollView.indicatorStyle = oldScrollView.indicatorStyle;

  // UIView properties.
  newScrollView.hidden = oldScrollView.hidden;
  newScrollView.alpha = oldScrollView.alpha;
  newScrollView.opaque = oldScrollView.opaque;
  newScrollView.tintColor = oldScrollView.tintColor;
  newScrollView.tintAdjustmentMode = oldScrollView.tintAdjustmentMode;
  newScrollView.clearsContextBeforeDrawing =
      oldScrollView.clearsContextBeforeDrawing;
  newScrollView.maskView = oldScrollView.maskView;
  newScrollView.userInteractionEnabled = oldScrollView.userInteractionEnabled;
  newScrollView.multipleTouchEnabled = oldScrollView.multipleTouchEnabled;
  newScrollView.exclusiveTouch = oldScrollView.exclusiveTouch;
  if (newScrollView.clipsToBounds != oldScrollView.clipsToBounds) {
    newScrollView.clipsToBounds = oldScrollView.clipsToBounds;
  }
  if (newScrollView.contentInsetAdjustmentBehavior !=
      oldScrollView.contentInsetAdjustmentBehavior) {
    newScrollView.contentInsetAdjustmentBehavior =
        oldScrollView.contentInsetAdjustmentBehavior;
  }
}

- (BOOL)clipsToBounds {
  return self.underlyingScrollView.clipsToBounds;
}

- (void)setClipsToBounds:(BOOL)clipsToBounds {
  self.underlyingScrollView.clipsToBounds = clipsToBounds;
}

- (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior {
  return [self.underlyingScrollView contentInsetAdjustmentBehavior];
}

- (void)setContentInsetAdjustmentBehavior:
    (UIScrollViewContentInsetAdjustmentBehavior)contentInsetAdjustmentBehavior {
  [self.underlyingScrollView
      setContentInsetAdjustmentBehavior:contentInsetAdjustmentBehavior];
}

- (NSArray<__kindof UIView*>*)subviews {
  return [self.underlyingScrollView subviews];
}

#pragma mark -

+ (NSArray*)scrollViewObserverKeyPaths {
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    return @[ @"frame", @"contentSize", @"contentInset" ];
  } else {
    return @[ @"contentSize" ];
  }
}

+ (void)startObservingScrollView:(UIScrollView*)scrollView
                           proxy:(CRWWebViewScrollViewProxy*)proxy {
  // Add observations by `proxy`.
  for (NSString* keyPath in [proxy.class scrollViewObserverKeyPaths]) {
    [scrollView
        addObserver:proxy
         forKeyPath:keyPath
            options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
            context:nil];
  }

  // Restore observers which were added to the past underlying scroll views.
  for (NSString* keyPath in proxy.keyValueObserverForwarders) {
    NSMutableDictionary<NSValue*,
                        NSMutableArray<CRWKeyValueObserverForwarder*>*>* map =
        proxy.keyValueObserverForwarders[keyPath];
    for (NSValue* observerValue in map) {
      for (CRWKeyValueObserverForwarder* observerForwarder in
               map[observerValue]) {
        [scrollView addObserver:observerForwarder
                     forKeyPath:keyPath
                        options:observerForwarder.options
                        context:observerForwarder.context];
      }
    }
  }
}

+ (void)stopObservingScrollView:(UIScrollView*)scrollView
                          proxy:(CRWWebViewScrollViewProxy*)proxy {
  // Remove observations by `self`.
  for (NSString* keyPath in [proxy.class scrollViewObserverKeyPaths]) {
    [scrollView removeObserver:proxy forKeyPath:keyPath];
  }

  // Remove observations added externally.
  for (NSString* keyPath in proxy.keyValueObserverForwarders) {
    NSMutableDictionary<NSValue*,
                        NSMutableArray<CRWKeyValueObserverForwarder*>*>* map =
        proxy.keyValueObserverForwarders[keyPath];
    for (NSValue* observerValue in map) {
      for (CRWKeyValueObserverForwarder* observerForwarder in
               map[observerValue]) {
        [scrollView removeObserver:observerForwarder
                        forKeyPath:keyPath
                           context:observerForwarder.context];
      }
    }
  }
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK_EQ(object, self.underlyingScrollView);
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    if ([keyPath isEqualToString:@"frame"]) {
      [_observers webViewScrollViewFrameDidChange:self];
    }
    if ([keyPath isEqualToString:@"contentInset"]) {
      [_observers webViewScrollViewDidResetContentInset:self];
    }
  }
  if ([keyPath isEqualToString:@"contentSize"]) {
    if (!base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
      NSValue* oldValue =
          base::apple::ObjCCast<NSValue>(change[NSKeyValueChangeOldKey]);
      NSValue* newValue =
          base::apple::ObjCCast<NSValue>(change[NSKeyValueChangeNewKey]);
      // If the value is unchanged -- if the old and new values are equal --
      // then return without notifying observers.
      if (oldValue && newValue && [newValue isEqualToValue:oldValue]) {
        return;
      }
    }
    [_observers webViewScrollViewDidResetContentSize:self];
  }
}

- (UIScrollView*)asUIScrollView {
  // See the comment of @implementation of this class for why this should be
  // safe.
  return (UIScrollView*)self;
}

#pragma mark - Forwards unimplemented UIScrollView methods

- (NSMethodSignature*)methodSignatureForSelector:(SEL)sel {
  // Called when this proxy is accessed through -asUIScrollView and the method
  // is not implemented in this class.
  return [self.underlyingScrollView methodSignatureForSelector:sel];
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  // Called when this proxy is accessed through -asUIScrollView and the method
  // is not implemented in this class. Forwards the invocation to the undelrying
  // scroll view.
  [invocation invokeWithTarget:self.underlyingScrollView];
}

#pragma mark - NSObject

- (BOOL)isKindOfClass:(Class)aClass {
  // Pretend self to be a kind of UIScrollView.
  return
      [UIScrollView isSubclassOfClass:aClass] || [super isKindOfClass:aClass];
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  // Respond to both of UIScrollView methods and its own methods.
  return [UIScrollView instancesRespondToSelector:aSelector] ||
         [super respondsToSelector:aSelector];
}

#pragma mark - KVO

- (void)addObserver:(NSObject*)observer
         forKeyPath:(NSString*)keyPath
            options:(NSKeyValueObservingOptions)options
            context:(nullable void*)context {
  // KVO against CRWWebViewScrollViewProxy works as KVO against the underlying
  // scroll view, except that `object` parameter of the notification points to
  // CRWWebViewScrollViewProxy, not the undelying scroll view. This is achieved
  // by CRWKeyValueObserverForwarder.
  NSMutableDictionary<NSValue*, NSMutableArray<CRWKeyValueObserverForwarder*>*>*
      map = _keyValueObserverForwarders[keyPath];
  if (!map) {
    map = [[NSMutableDictionary alloc] init];
    _keyValueObserverForwarders[keyPath] = map;
  }

  // See the comment of the definition of _keyValueObserverForwarders for why
  // NSValue with an unretained pointer is used here.
  NSValue* observerValue = [NSValue valueWithNonretainedObject:observer];
  NSMutableArray<CRWKeyValueObserverForwarder*>* observerForwarders =
      map[observerValue];
  if (!observerForwarders) {
    observerForwarders = [[NSMutableArray alloc] init];
    map[observerValue] = observerForwarders;
  }

  CRWKeyValueObserverForwarder* observerForwarder =
      [[CRWKeyValueObserverForwarder alloc] initWithWrappedObserver:observer
                                                             object:self
                                                            options:options
                                                            context:context];
  [observerForwarders addObject:observerForwarder];

  [self.underlyingScrollView addObserver:observerForwarder
                              forKeyPath:keyPath
                                 options:options
                                 context:context];
}

- (void)removeObserver:(NSObject*)observer forKeyPath:(NSString*)keyPath {
  [self removeObserver:observer forKeyPath:keyPath context:&gAnyContext];
}

- (void)removeObserver:(NSObject*)observer
            forKeyPath:(NSString*)keyPath
               context:(void*)context {
  NSMutableDictionary<NSValue*, NSMutableArray<CRWKeyValueObserverForwarder*>*>*
      map = _keyValueObserverForwarders[keyPath];

  // See the comment of the definition of _keyValueObserverForwarders for why
  // NSValue with an unretained pointer is used here.
  NSValue* observerValue = [NSValue valueWithNonretainedObject:observer];
  NSMutableArray<CRWKeyValueObserverForwarder*>* observerForwarders =
      map[observerValue];

  // It is technically allowed to call -addObserver:forKeypath:options:context:
  // multiple times with the same `observer` and same `keyPath`. And
  // -removeObserver:forKeyPath:context: (and -removeObserver:forKeyPath:)
  // removes the *last* observation matching the condition. This matches the
  // (undocumented) behavior of the built-in KVO.
  NSInteger i = static_cast<NSInteger>(observerForwarders.count) - 1;
  for (; i >= 0; --i) {
    if (context == &gAnyContext || observerForwarders[i].context == context) {
      break;
    }
  }

  // DCHECK on an attempt to remove an observer which is not registered. This
  // behavior is inconsistent with the behavior of this method in NSObject
  // (which throws an exception in this case). But Chromium code is not allowed
  // to throw exceptions.
  DCHECK_GE(i, 0) << base::SysNSStringToUTF8(
      context == &gAnyContext
          ? [NSString
                stringWithFormat:
                    @"Cannot remove an observer %@ for the key path \"%@\" "
                    @"from %@ because it is not registered as an observer.",
                    observer, keyPath, self]
          : [NSString
                stringWithFormat:
                    @"Cannot remove an observer %@ for the key path \"%@\" "
                    @"with context %p from %@ because it is not registered as "
                    @"an observer.",
                    observer, keyPath, context, self]);

  [self.underlyingScrollView removeObserver:observerForwarders[i]
                                 forKeyPath:keyPath];
  [observerForwarders removeObjectAtIndex:i];
}

@end
