## All files are Objective-C++

Chrome back-end code is all C++ and we want to leverage many C++ features, such
as stack-based classes and namespaces. As a result, all front-end Bling files
should be .mm files, as we expect eventually they will contain C++ code or
language features.

## Use ObjCCast<T> and ObjcCCastStrict<T>

As the C++ style guide tells you, we never use C casts and prefer
`static_cast<T>` and `dynamic_cast<T>`. However, for Objective-C casts we have
two specific casts: `base::apple::ObjCCast<T>arg` is similar to `dynamic_cast<T>`,
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
