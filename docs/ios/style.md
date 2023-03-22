## All files are Objective-C++

Chrome back-end code is all C++ and we want to leverage many C++ features, such
as stack-based classes and namespaces. As a result, all front-end Bling files
should be .mm files, as we expect eventually they will contain C++ code or
language features.

## Use scoped_nsobject<T> and WeakNSObject<T> where ARC is not available.

While there are no smart pointers in Objective-C, Chrome has
`scoped_nsobject<T>` and `WeakNSObject<T>` to automatically manage (and
document) object ownership.

Under ARC,` scoped_nsobject<T>` and `WeakNSObject<T>` should only be used for
interfacing with existing APIs that take these, or for declaring a C++ member
variable in a header. Otherwise use `__weak` variables and `strong`/`weak`
properties. **Note that scoped_nsobject and WeakNSObject provide the same API
under ARC**, i.e. `scoped_nsobject<T> foo([[Bar alloc] init]);` is correct both
under ARC and non-ARC.

`scoped_nsobject<T>` should be used for all owned member variables in C++
classes (except the private classes that only exist in implementation files) and
Objective-C classes built without ARC, even if that means writing dedicated
getters and setters to implement `@property` declarations. Same goes for
WeakNSObject - always use it to express weak ownership of an Objective-C object,
unless you are writing ARC code. We'd rather have a little more boilerplate code
than a leak.

## Use ObjCCast<T> and ObjcCCastStrict<T>

As the C++ style guide tells you, we never use C casts and prefer
`static_cast<T>` and `dynamic_cast<T>`. However, for Objective-C casts we have
two specific casts: `base::mac::ObjCCast<T>arg` is similar to `dynamic_cast<T>`,
and `ObjcCCastStrict` `DCHECKs` against that class.

## Blocks

We follow Google style for blocks, except that historically we have used 2-space
indentation for blocks that are parameters, rather than 4. You may continue to
use this style when it is consistent with the surrounding code.

## NOTIMPLEMENTED and NOTREACHED logging macros

`NOTREACHED`: This function should not be called. If it is, we have a problem
somewhere else.
`NOTIMPLEMENTED`: This isn't implemented because we don't use it yet. If it's
called, then we need to figure out what it should do.

When something is called but doesn't need an implementation, just add a comment
indicating this instead of using a logging macro.

## TODO comments

Sometimes we include TODO comments in code. Generally we follow
[C++ style](https://google.github.io/styleguide/cppguide.html#TODO_Comments),
but here are some more specific practices we've agreed upon as a team:

* **Every TODO must have a bug**
* Bug should be labeled with **Hotlist-TODO-iOS**
* Always list bug in parentheses following "TODO"
    * `// TODO(crbug.com/######): Something that needs doing.`
    * Do NOT include http://
* Optionally include a username for reference
* Optionally include expiration date (make sure it's documented in the bug!)
