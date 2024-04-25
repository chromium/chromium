# Preventing OOB through Unsafe Buffers errors (aka Spanification)

Out-of-bounds (OOB) security bugs commonly happen through pointers
which have no bounds checks associated with them. We prevent such
bugs by always using containers.

Most pointers are unowned references into an array (or vector)
and the most appropriate replacement for the pointer is
[`base::span`](../base/containers/span.h).

Entire directories have been opted out of unsafe pointer usage
warnings through the
[`//build/config/unsafe_buffers_paths.txt`](../build/config/unsafe_buffers_paths.txt)
file. As we convert unsafe pointers to safe constructs like
`base::span`, `base::HeapArray` and `std::vector`, we will
remove paths from that file to enable the warnings across the
codebase.

## Controlling warnings for a single file

Warnings can be disabled for a single (C++ source or header) file by
writing `#pragma allow_unsafe_buffers` anywhere in the file. This can
be used to mark future work to drive down over time:
```
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/ABC): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
```

It's recommended to place the pragma directly copyright header. This can be used
to work through files in a directory incrementally. To do so, remove the whole
directory from opt-out in the
[`//build/config/unsafe_buffers_paths.txt`](../build/config/unsafe_buffers_paths.txt)
file, and temporarily mark any files with `#pragma allow_unsafe_buffers` that
need it.

The warnings can be enabled for a single (C++ source or header) file by writing
`#pragma check_unsafe_buffers` anywhere in the file. These also need to be
guarded by `#ifdef UNSAFE_BUFFERS_BUILD` if being checked in.

# Functions with array pointer parameters

Functions that receive a pointer into an array may read
or write out of bounds of the pointer if given a pointer that
is incorrectly sized. Such functions should be marked with the
`UNSAFE_BUFFER_USAGE` attribute macro.

The same is true for functions that accept an iterator instead
of a range type. Some examples of each are `memcpy()` and
`std::copy()`.

Calling such functions is unsafe and should generally be avoided.
Instead, replace such functions with an API built on base::span
or other range types which prevents any chance of OOB memory
access. For instance, replace `memcpy()`, `std::copy()` and
`std::ranges::copy()` with `base::span::copy_from()`. And
replace `memset()` with `std::ranges::fill()`.

# Writing unsafe data structures with pointers

TODO: Write about `UNSAFE_BUFFERS()` for rare exceptions where
the correctness of pointer bounds can be fully explained and
encapsulated, such as within a data structure or when working
with Operating System and C-like APIs.
