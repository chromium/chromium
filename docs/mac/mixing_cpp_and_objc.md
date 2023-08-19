# Mixing C++ and Objective-C

The Mac is in an unusual position of having most of its relevant UI APIs be in a
different language than the one used for its core code. Navigating boundaries
between C++ and Objective-C can be tricky.

## Use Objective-C++

Using Objective-C++ works well for Mac-only implementation files. If a file is
named with a `.mm` extension, then it will be compiled as an Objective-C++ file.
Within such a file usage of Objective-C and C++ can be intermixed.

If Objective-C++ works in the context needed, it is the preferred way to
accomplish mixing of C++ and Objective-C.

## Use the pimpl idiom

The [pimpl idiom](https://en.wikipedia.org/wiki/Opaque_pointer#C++) is a
standard way to hide the implementation of a C++ class from its users, exposing
nothing but an implementation pointer in the header file. Usually it is used for
compatibility (e.g. hiding implementation details), but it’s useful in Chromium
for hiding the Objective-C implementation details in the `.mm` implementation
file and removing them from the `.h` file which might need to be included in a
different `.cc` implementation file and which thus cannot have any Objective-C
in it, even in a `private:` block.

The standard boilerplate for doing this is named
[`ObjCStorage`](https://source.chromium.org/search?q=ObjCStorage&ss=chromium).

In the header file, a nested struct is forward-declared for use by a
`std::unique_ptr`:

```cpp
class UtilityObjectMac {
  // ...
 private:
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};
```

and in the implementation `.mm` file, have that nested class host all the Obj-C
instance variables:

```cpp
struct UtilityObjectMac::ObjCStorage {
  id appkit_token;
  NSWindow* window;
};

UtilityObjectMac::UtilityObjectMac()
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->appkit_token = [NSFramework registerTheThing: //...
  // ...
```

This moves all of the Objective-C code into an Objective-C++ file at the cost of
a secondary allocation and indirection on use.

## Use `/base/apple/owned_objc.h` types

It is, unfortunately, a common pattern in Chromium code to use macros and
typedefs to declare a platform-neutral name for a core platform UI type (e.g.
`ui/gfx/native_widget_types.h`’s `ui::NativeView`,
`ui/events/platform_event.h`’s `ui::PlatformEvent`) and then for pure C++ code
to pass those types around. For those cases, where the previous two approaches
can’t be used, the wrappers in `/base/apple/owned_objc.h` can be used.

## Double-declaration (dangerous)

If none of the previous techniques will work, a double-declaration can be used.
An example can be seen in
[native_widget_types.h](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/native_widget_types.h):

```objectivec
#ifdef __OBJC__
@class NSImage;
@class NSView;
@class NSWindow;
#else
class NSImage;
class NSView;
class NSWindow;
#endif  // __OBJC__
```

With this double-declaration, these types can be used from both C++ and
Objective-C. However, the price that is paid is that this is a (mostly) benign
violation of the [ODR](https://en.wikipedia.org/wiki/One_Definition_Rule) and
thus should be avoided if possible.

Specifically, this can get dangerous with Objective-C ARC enabled, where a
pointer to a type declared this way will be treated by C++ as a raw pointer
while it will be treated by Objective-C as a smart pointer with retain/release
semantics.

Because of Chromium’s history as a non-ARC app, the approach of using
double-declarations was found to be more acceptable of a tradeoff than it is
nowadays, so there is a lot of double-declaration. Revising code to remove
double-declaration improves the code; please do so when it makes sense. There is
a [bug](https://crbug.com/1433041) tracking this effort of eventual removal.

Do not include `<objc/objc.h>`. It has all the pitfalls of double-declaration
for the `id` type (note that even though it defines `id` as `struct
objc_object*`, the Objective-C compiler does not see them as equivalent), but
has the additional pitfall of defining away the ARC ownership annotations if not
compiling with Objective-C ARC. The inclusion of it is therefore banned, as it
causes conflicts if included in header files, and while C++ implementation files
should not be involving themselves with Objective-C types anyway, Objective-C
implementation files implicitly have it included through their inclusion of
framework headers.
