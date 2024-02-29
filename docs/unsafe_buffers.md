# Preventing OOB through Unsafe Buffers errors (aka Spanification)

Out-of-bounds (OOB) security bugs commonly happen through pointers
which have no bounds checks associated with them. We prevent such
bugs by always using containers.

Most pointers are unowned references into an array (or vector)
and the most appropriate replacement for the pointer is
base::span.

When a file or directory is known to pass compilation with
`-Wunsafe-buffer-usage`,it should be added to the
[`//build/config/unsafe_buffers_paths.txt`](../build/config/unsafe_buffers_paths.txt)
file to enable compiler errors if unsafe pointer usage is added to
the file later.

# Functions with array pointer parameters

Functions that receive a pointer into an array may read
or write out of bounds of the pointer if given a pointer that
is incorrectly sized. Such functions should be marked with the
UNSAFE_BUFFER_USAGE attribute macro.

The same is true for functions that accept an iterator instead
of a range type. Some examples of each are memcpy() and
std::copy().

Calling such functions is unsafe and should generally be avoided.
Instead, replace such functions with an API built on base::span
or other range types which prevents any chance of OOB memory
access. For instance, replace `memcpy()`, `std::copy()` and
`std::ranges::copy()` with `base::span::copy_from()`. And
replace `memset()` with `std::ranges::fill()`.

# Writing unsafe data structures with pointers

TODO: Write about `UNSAFE_BUFFERS()` for rare exceptions where
the correctness of pointer bounds can be fully explained and
encapsulated, such as within a data structure.
