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
controlled as a
[config](https://source.chromium.org/search?q=config\\(%22enable_arc%22\\)&ss=chromium)
for build targets. ARC is enabled by default for Chromium’s Objective-C code,
with the exception of a handful of targets that opt-out.

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

ARC is enabled by default for Objective-C code in Chromium, and all new
Objective-C code in Chromium must be written to use ARC. If there is a good
technical reason to not use ARC, you may disable it for a target, but this is
expected to be an exceedingly rare situation and you should have a discussion
with the relevant platform experts before doing so.

### Header files {#convention-headers}

Header files can be:

1. _Shared between C++ and Objective-C._ For these headers, avoid having any
   Objective-C object pointers in them. Use the techniques in the [Mixing C++
   and Objective-C](mixing_cpp_and_objc.md) document to accomplish this.
2. _Only included in Objective-C implementation files compiled with ARC._
   Because ARC is the default compilation mode, this will be common. For these
   headers, qualify the ownership of Objective-C object pointers with `__strong`
   and `__weak`.
3. _Included in Objective-C implementation files compiled with a mix of ARC and
   non-ARC._ Because ARC is the default compilation mode, this situation should
   be rare. For this situation, treat the non-ARC compiled files as if they were
   C++ and use the techniques from point 1.

## Warning! Dangers! {#dangers}

There are some bits of AppKit that are incompatible with ARC. Apple has not
updated the documentation to call this out, so a heads-up:

When creating an `NSWindow`, you _must_ set the `.releasedWhenClosed` property
to `NO`. It’s recommended that you do so immediately after creating it with
`alloc`/`init`. If you fail to do so, then closing the window will cause it to
release itself, and then when the owning pointer releases it, it will be a
double-release.

## Examples of ARC code {#examples}

### Objective-C Classes {#examples-objc-classes}

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

### C++ classes in Objective-C++ files {#examples-cpp-classes}

```objectivec
class Banana : public Fruit {
  Animal* __strong pet_;
  Phone* __weak nexus_;
  Vehicle* __weak car_;  // Do not use __unsafe_unretained.
}
```

### Blocks {#examples-blocks}

Note: Blocks retain all objects referenced in them. This example is of a block
used in an Objective-C method that uses the “weak `self`” idiom to avoid a
retain cycle. For blocks used in C++ functions, a retain cycle is not a concern,
though the use of a `base::WeakPtr<>` might be needed to avoid stale pointers.

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

All targets compile with ARC as default. For targets that must not compile with
ARC, ARC can be disabled as follows:

```gn
  # Do not compile with ARC because AncientDeps code is not compatible with
  # being compiled with ARC.
  configs -= [ "//build/config/compiler:enable_arc" ]
```

Again, ARC must be used for all new code unless there is a good technical reason
that the new code cannot use ARC. Please consult with platform experts if you
believe that you are in this situation.

## Things that should not be used from ARC code {#changes}

There are utility functions and classes that were introduced when Chromium did
not compile with ARC, but that are no longer needed with ARC code. Because there
are still parts of Chromium that cannot be compiled with ARC, these utilities
remain, however they should not (or sometimes cannot) be used from ARC:

- `scoped_nsobject<>`/`scoped_nsprotocol<>`: These only exists to handle scoping
  of Objective-C objects in non-ARC code. They cannot be used in ARC code; use
  `__strong` instead.
- `ScopedNSAutoreleasePool`: Use `@autoreleasepool` instead, and remove any use
  of `ScopedNSAutoreleasePool` that you encounter, if possible.
  `ScopedNSAutoreleasePool` was rewritten to be able to work in ARC code, but
  the C++ class-based nature of `ScopedNSAutoreleasePool` is fundamentally
  incompatible with the stack-based nature of autorelease pools, and thus it is
  in the process of [being removed](https://crbug.com/772489).

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
