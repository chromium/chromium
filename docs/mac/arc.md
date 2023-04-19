# Automatic Reference Counting

[TOC]

## What is ARC? {#what-is}

Objective-C memory management is based on reference counting. Historically,
managing the reference count was a task given to the developer, but starting
with Mac OS X 10.7/iOS 5, additional language support was added to Objective-C
to allow the compiler to manage the reference counts given lifetime annotations
by the developer. That feature is named “automatic reference counting” and is
abbreviated to “ARC”.

ARC is enabled via a flag passed to the compiler and thus for Chromium is
enabled at the build target level. ARC is currently enabled for most of Chrome
on iOS, and is in the [process](https://crbug.com/1280317) of being enabled for
Chrome on the Mac.

For the rest of this document, the term “Objective-C” will be used to mean both
pure Objective-C as well as Objective-C++.

## The basics of ARC {#basics}

(This is necessarily a simplified explanation; reading ARC
[documentation](#references) outside of this document is highly recommended.)

Reference counting is a technique of managing the lifetime of objects by keeping
a count of ownership references (in the conceptual sense of “references”, not
in the C++ sense of `&`s). If some code wants an object to remain alive, it can
“retain” the object by incrementing the reference count, and when it no longer
needs the object to remain alive, it can “release” the object by decrementing
the reference count. When the reference count hits zero, that indicates that no
code needs the object to remain alive, so it is deallocated.

Objective-C objects are accessed via pointer. The most straightforward way of
thinking about ARC is that, while in classic manual reference counting, those
pointers are raw pointers and the programmer is in charge of writing the
appropriate retain and release messages, with ARC, all pointers to Objective-C
objects are smart pointers:

- `__strong` (default): This pointer maintains a strong reference to the object.
  When an object pointer is assigned to it, that object is sent a retain
  message, and the object pointer that it used to contain is sent a release
  message (possibly causing it to be deallocated it if that caused the retain
  count to hit zero).
  - Chromium usage note: Even though this is the default, please always
    explicitly specify it for class members/ivars so that it is clear which
    pointers are Objective-C object pointers and which are C++ object pointers.
- `__weak`: This pointer maintains a weak reference to the object which is kept
  alive by other `__strong` references. If the last of the strong references is
  released, and the object is deallocated, this pointer will be set to `nil`.
- `__unsafe_unretained`: This is a raw pointer (as in C/C++) which maintains a
  reference to the object but has no other automatic capabilities.
  - Chromium usage note: Do not use this, as it is almost certainly the wrong
    choice. The `PRESUBMIT` will complain.

ARC knows about the standard Objective-C conventions for naming methods on
objects that return unretained vs retained objects, and will automatically treat
those functions as such. C++ functions that return retained Objective-C objects
will need to be explicitly annotated as such with `NS_RETURNS_RETAINED` if in
Objective-C code, or `__attribute__((ns_returns_retained))` if in a shared C++
header. Note, though, that for header files that interface with both pure C++
and Objective-C, serious thought will need to be given to that boundary with
regards to retain count expectations.

Because ARC handles all the reference counting, direct message sends of
`-retain`, `-release`, and `-autorelease` are no longer allowed. The compiler
automatically inserts them as needed, directed by the ownership annotations.
Incorrect annotations will cause incorrect reference counting; annotate the code
correctly to fix issues with the compiler-generated reference counting.

## ARC in Chromium {#conventions}

### When to use ARC {#conventions-when}

It is the plan to eventually enable ARC for all of Chromium’s Objective-C code.
However, because ARC is a target-scoped build configuration for Chromium, it
might be the case that, when adding a new file, you find that the `.gn` target
containing that file does not have ARC enabled. In that case, you may implement
that file without ARC support. However, if the target is already being built
with ARC, your code must use ARC as well.

If you are lucky enough to be adding the first Objective-C file to the target,
then you must write your code to build with ARC and add the [config to the `.gn`
target](#examples-gn).

### ARC compile guard {#convention-boilerplate}

Because Chromium currently comprises mixed ARC and non-ARC Objective-C code,
files that are written to build with ARC have a boilerplate compile guard after
the include block:

```objectivec
#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif
```

### Header files {#convention-headers}

Header files can be:

1. Shared between C++ and Objective-C. For these headers, avoid having any
   Objective-C object pointers in them. Use the techniques in the [Mixing C++
   and Objective-C](mixing_cpp_and_objc.md) document to accomplish this.
2. Only included in Objective-C implementation files compiled with ARC. For
   these headers, qualify the ownership of Objective-C object pointers with
   `__strong` and `__weak`, and add the [ARC
   boilerplate](#convention-boilerplate) at the top to ensure that they are only
   included by files compiled with ARC.
3. Included in Objective-C implementation files compiled with a mix of ARC and
   non-ARC. For these headers, continue using `base::scoped_nsobject<>` for
   owned object pointers, but aim to eventually remove its use and switch to
   ARC.

Be aware that distinguishing between these cases can be tricky; if a header file
is included in another header file, you must also consider which files that
header file is included in. The most expedient way to distinguish case 2 is to
add the [ARC boilerplate](#convention-boilerplate) to the header file and then
attempt to compile. If that header file is included in either a C++ file or an
Objective-C file not compiled with ARC, the compile will fail at that ARC
boilerplate. As for case 1, if the header file has Objective-C constructs (e.g.
`#import` or an `@` keyword) unguarded by `__OBJC__`, it would not compile in
C++ and therefore is not included by C++ code.

## Examples of conversion from non-ARC to ARC {#examples}

### Objective-C Classes {#examples-objc-classes}

Before:

```objectivec
@interface KittyNoARC : NSObject
@property(nonatomic, assign) id<KittyDelegate> delegate;
@property(nonatomic, copy) NSArray* childCats;
@property(nonatomic, retain) NSURL* vetURL;
- (Meow*)meowForBellyRub:(BellyRub*)rub;
@end

@implementation KittyNoARC {
  id<CatFactory> _catFactory;  // weak
  base::scoped_nsobject<NSURL> _lastVisitedCatURL;
  base::scoped_nsobject<NSArray> _childCats;
  base::scoped_nsobject<NSURL> _vetURL;
}

- (void)setChildCats:(NSArray*)childCats {
  _childCats.reset([childCats copy]);
}

- (NSArray*)childCats {
  return _childCats.get();
}

- (void)setVetURL:(NSURL*)vetURL {
  _vetURL.reset([vetURL retain]);
}

- (NSURL*)vetURL {
  return _vetURL.get();
}

- (Meow*)meowForBellyRub:(BellyRub*)rub {
  return [[[MeowImpl alloc] initWithBellyRub:rub] autorelease];
}
@end
```

After:

```objectivec
@interface KittyARC : NSObject
@property(nonatomic, weak) id<KittyDelegate> delegate;
@property(nonatomic, copy) NSArray* childCats;
@property(nonatomic, strong) NSURL* vetURL;
- (Meow*)meowForBellyRub:(BellyRub*)rub;
@end

@implementation KittyARC {
  id<CatFactory> __weak _catFactory;
  NSURL* _lastVisitedCatURL;
}

@synthesize delegate = _delegate;
@synthesize childCats = _childCats;
@synthesize vetURL = _vetURL;

- (Meow*)meowForBellyRub:(BellyRub*)rub {
  return [[MeowImpl alloc] initWithBellyRub:rub];
}
@end
```

### C++ classes in Objective-C++ implementation files {#examples-cpp-classes-impls}

Before:

```objectivec
class Banana : public Fruit {
  base::scoped_nsobject<Animal> pet_;
  base::WeakNSObject<Phone> nexus_;
  Vehicle* car_;
}
```

After:

```objectivec
class Banana : public Fruit {
  Animal* __strong pet_;
  Phone* __weak nexus_;
  Vehicle* __weak car_;  // Do not use __unsafe_unretained.
}
```

### C++ classes in header files {#examples-cpp-classes-headers}

Before:

```objectivec
class Banana : public Fruit {
  base::scoped_nsobject<Animal> pet_;
  base::WeakNSObject<Phone> nexus_;
  Vehicle* car_;
}
```

After:

```objectivec
class Banana : public Fruit {
  // If this class is only included in ARC-enabled code, then include an ARC
  // compile guard and do:
  Animal* __strong pet_;

  // Otherwise, do this and move to ARC once all including files have moved to
  // ARC:
  base::scoped_nsobject<Animal> pet_;

  Phone* __weak nexus_;
  Vehicle* __weak car_;  // Do not use __unsafe_unretained.
}
```

### Blocks {#examples-blocks}

Note: Blocks retain all objects referenced in them. This example is of a block
used in an Objective-C method that uses the “weak `self`” idiom to avoid a
retain cycle. For blocks used in C++ functions, a retain cycle is not a concern,
though the use of a `base::WeakPtr<>` might be needed to avoid stale pointers.

Before:

```objectivec
base::WeakNSObject<AuthenticationFlow> weakSelf(self);
[performer_ showAuthenticationError:error
                     withCompletion:^{
                       base::scoped_nsobject<AuthenticationFlow> strongSelf(
                           [weakSelf retain]);
                       if (!strongSelf) {
                         return;
                       }
                       [strongSelf setHandlingError:NO];
                       [strongSelf continueSignin];
                     }];
```

After:

```objectivec
typeof(self) __weak weakSelf = self;
[performer_ showAuthenticationError:error
                     withCompletion:^{
                       typeof(self) strongSelf = weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       [strongSelf setHandlingError:NO];
                       [strongSelf continueSignin];
                     }];
```

### GN file changes {#examples-gn}

A `.gn` target will compile as ARC with the config:

```gn
    configs += [ "//build/config/compiler:enable_arc" ]
```

Small targets can be converted to ARC all at once. For larger targets, a more
gradual transition may be needed, for example:

```gn
source_set("fruit_arc") {
  sources = [
    "pear.h",
    "pear.mm",
  ]
  deps = [
    "//base",
  ]
  public_deps = [
      ":fruit_support",
  ]
  configs += [ "//build/config/compiler:enable_arc" ]
  allow_circular_includes_from = [ ":fruit" ]
}

source_set("fruit") {
  sources = [
    "apple.h",
    "apple.mm",
    "banana.h",
    "banana.mm",
    // "pear.h" and "pear.mm" were here before being converted to ARC.
  ]
  deps = [
    "//base",
  ]
  public_deps = [
      ":fruit_support",
  ]
  allow_circular_includes_from = [ ":fruit_arc" ]
}
```

## Things that are going away {#changes}

Chromium’s migration to ARC means not only the opportunity to re-think the code
and do cleanup, but the removal of utilities whose functionality will no longer
be needed and will eventually end up being removed. Here are some, in no
particular order:

- `base::scoped_nsobject<>`: Only continue its use in header files shared
  between ARC and non-ARC Objective-C code, and only temporarily while those
  files are included by non-ARC files. Remove all other use.
- `ScopedNSAutoreleasePool`: Use `@autoreleasepool` instead, and remove any use
  of `ScopedNSAutoreleasePool` that you encounter, if possible.
- `base::mac::ScopedBlock`: Not needed; block pointers get the same ARC
  ownership management as Objective-C object pointers.
- `CFToNSCast` and `NSToCFCast`: These do not handle ARC ownership; switch to
  `CFToNSPtrCast`, `CFToNSOwnershipCast`, `NSToCFPtrCast`, and
  `NSToCFOwnershipCast` from `base/mac/bridging.h`.

## Further reading {#references}

- Apple documentation about memory management and how both manual reference
  counting and ARC work: [About Memory
  Management](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/MemoryMgmt/Articles/MemoryMgmt.html)
- Apple documentation from when ARC was new; it’s marked as “outdated” but is
  still a good read: [Transitioning to ARC Release
  Notes](https://developer.apple.com/library/archive/releasenotes/ObjectiveC/RN-TransitioningToARC/Introduction/Introduction.html)
- Wikipedia overview: [Automatic Reference
  Counting](https://en.wikipedia.org/wiki/Automatic_Reference_Counting)
- Clang documentation (very technical): [Objective-C Automatic Reference
  Counting (ARC)](https://clang.llvm.org/docs/AutomaticReferenceCounting.html)
  - There’s a specialized tool named
    [`objc_precise_lifetime`](https://clang.llvm.org/docs/AutomaticReferenceCounting.html#precise-lifetime-semantics)
    that might be useful in specific situations where the compiler cannot fully
    deduce what lifetime is needed for a local variable. It’s not usually
    needed, but if you have gotten to this point in this document, you should
    know it exists in case you find yourself in just that situation.

## Documents from Chromium iOS’s ARC transition {#old-docs}

Several years ago, Chromium for iOS transitioned to ARC. In the process of doing
so, they produced the documents (Google-internal):

- [ARC transition](https://goto.google.com/chrome-arc)
- [[ARC] How to convert a target](https://goto.google.com/arc-howto)
- [ARC FAQ](https://goto.google.com/faq-arc)

They described the ARC process as they intended to do it, not necessarily as it
ended up being done. All relevant information from them has been moved to this
document. However, they can be useful reading for historical context.
