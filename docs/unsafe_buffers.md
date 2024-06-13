# Preventing OOB through Unsafe Buffers errors (aka Spanification)

Out-of-bounds (OOB) security bugs commonly happen through pointers which
have no bounds information associated with them. We can prevent such
bugs by always using containers. Furthermore, the Clang compiler can
warn about unsafe pointer usage that should be converted to containers.
When an unsafe usage is detected, Clang prints a warning similar to
```
error: unsafe buffer access [-Werror,-Wunsafe-buffer-usage]
```
and directs developers to this file for more information.

## Suppressions

Our [compiler](../tools/clang/plugins/UnsafeBuffersPlugin.cpp) enables
the `-Wunsafe-buffer-usage` warning on all files by default. Because the
Chromium codebase is not yet compliant with these warnings, there are
mechanisms to opt out code on a directory, file, or per-occurence basis.

Entire directories are opted out of unsafe pointer usage warnings through
the [`//build/config/unsafe_buffers_paths.txt`](../build/config/unsafe_buffers_paths.txt)
file. As work progresses, directories will be removed from this list, and
non-compliant files marked on a per-file basis as below. Early results
indicate that often 85%+ of files in a directory already happen to be
compliant, so file-by-file suppression allows this code to be subject
to enforcement.

Individual files are opted out of unsafe pointer usage warnings though
the use of the following snippet, which is to be placed immediately
following the copyright header in a source file.
```
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/ABC): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif
```

Individual expressions or blocks of code are opted out by using the
`UNSAFE_BUFFERS()` macro as defined in [`//base/compiler_specific.h`[(../base/compiler_specific.h)
file. These should be rare once a project is fully converted, except
perhaps when working with C-style external APIs. These must
always be accompanied by a `// SAFETY:` comment explaining in detail
how the code has been evaluated to be safe for all possible input.

To allow for incremental conversion, the use of a safety comment with
a TODO() is permitted, along the lines of
`// SAFETY: TODO(crbug.com/xxxxxx): resolve safety issues`.

Code introducing UNSAFE_BUFFER() macro invocations without corresponding
`// SAFETY:` comment should be summarily rejected during code review.

## Use of std::array<T>.

The clang plugin is very particular about indexing a C-style array (e.g.
`int arr[100]`) with a variable index. Often these issues can be resolved
by replacing this with `std::array<int, 100> arr`, which provides safe
indexed operations. In particular, new code should prefer to use the
`std::array<T, N>` mechanism.

For arrays where the size is determined by the compiler (e.g.
`int arr[] = { 1, 3, 5 };`), the `std::to_array<T>()` helper function
should be used along with the `auto` keyword:
`auto arr = std::to_array<int>({1, 3, 5});`

## Patterns for spanification.

Most pointer issues ought to be resolved by converting to containers. In
particular, one common conversion is to replace `T*` pointers with
`base::span<T>` in a process known as spanification, since most pointers
are unowned references into an array (or vector). The appropriate
replacement for the pointer is
[`base::span`](../base/containers/span.h).

### Copying arrays (`memcpy`)

You have:
```cc
uint8_t array1[12];
uint8_t array2[16];
uint64_t array3[2];
memcpy(array1, array2 + 8, 4);
memcpy(array1 + 4, array3, 8);
```

Spanified:
```cc
uint8_t array1[12];
uint8_t array2[16];
uint64_t array3[2];
base::span(array1).first(4u).copy_from(base::span(array2).subspan(8u, 4u));
base::span(array1).subspan(4u).copy_from(base::as_byte_span(array3).first(8u));

// Use `split_at()` to ensure `array1` is fully written.
auto [from2, from3] = base::span(array1).split_at(4u);
from2.copy_from(base::span(array2).subspan(8u, 4u));
from3.copy_from(base::as_byte_span(array3).first(8u));

// This can even be ensured at compile time (if sizes and offsets are all
// constants).
auto [from2, from3] = base::span(array1).split_at<4u>();
from2.copy_from(base::span(array2).subspan<8u, 4u>());
from3.copy_from(base::as_byte_span(array3).first<8u>());
```

### Zeroing arrays (`memset`)

`std::ranges::fill` works on any range/container and won't write out of
bounds. Converting arbitrary types into a byte array (through
`base::as_writable_byte_span`) is only valid when the type holds trivial
types such as primitives. Unlike `memset`, a constructed object can be
given as the value to use as in `std::ranges_fill(container, Object())`.

You have:
```cc
uint8_t array1[12];
uint64_t array2[2];
Object array3[4];
memset(array1, 0, 12);
memset(array2, 0, 2 * sizeof(uint64_t));
memset(array3, 0, 4 * sizeof(Object));
```

Spanified:
```cc
uint8_t array1[12];
uint64_t array2[2];
Object array3[4];
std::ranges::fill(array1, 0u);
std::ranges::fill(array2, 0u);
std::ranges::fill(base::as_writable_byte_span(array3), 0u);
```

### Comparing arrays (`memcmp`)

You have:
```cc
uint8_t array1[12] = {};
uint8_t array2[12] = {};
bool eq = memcmp(array1, array2, sizeof(array1)) == 0;
bool less = memcmp(array1, array2, sizeof(array1)) < 0;

// In tests.
for (size_t i = 0; i < sizeof(array1); ++i) {
  SCOPED_TRACE(i);
  EXPECT_EQ(array1[i], array2[i]);
}
```

Spanified:
```cc
uint8_t array1[12] = {};
uint8_t array2[12] = {};
// If one side is a span, the other will convert to span too.
bool eq = base::span(array1) == array2;
bool less = base::span(array1) < array2;

// In tests.
EXPECT_EQ(base::span(array1), array2);
```

### Copying array into an integer

You have:
```cc
uint8_t array[44] = {};
uint32_t v1;
memcpy(&v1, array, sizeof(v1));  // Front.
uint64_t v2;
memcpy(&v2, array + 6, sizeof(v2));  // Middle.
```

Spanified:
```cc
#include "base/numerics/byte_conversions.h"
...
uint8_t array[44] = {};
uint32_t v1 = base::U32FromLittleEndian(base::span(array).first<4u>());  // Front.
uint64_t v2 = base::U64FromLittleEndian(base::span(array).subspan<6u, 8u>());  // Middle.
```

### Copy an array into an integer via cast

Note: This pattern is prone to more UB than out-of-bounds access. It is UB to
cast pointers if the result is not aligned, so these cases are often buggy or
were only correct due to subtle assumptions on buffer alignment. The spanified
version avoids this pitfalls. It has no alignment requirement.

You have:
```cc
uint8_t array[44] = {};
uint32_t v1 = *reinterpret_cast<const uint32_t*>(array);  // Front.
uint64_t v2 = *reinterpret_cast<const uint64_t*>(array + 16);  // Middle.
```

Spanified:
```cc
#include "base/numerics/byte_conversions.h"
...
uint8_t array[44] = {};
uint32_t v1 = base::U32FromLittleEndian(base::span(array).first<4u>());  // Front.
uint64_t v2 = base::U64FromLittleEndian(base::span(array).subspan<16u, 8u>());  // Middle.
```

### Making a byte array (`span<uint8_t>`) from a string (or any array/range)

You have:
```cc
std::string str = "hello world";
func_with_const_ptr_size(reinterpret_cast<const uint8_t*>(str.data()), str.size());
func_with_mut_ptr_size(reinterpret_cast<uint8_t*>(str.data()), str.size());
```

Spanified:
```cc
std::string str = "hello world";
base::span<const uint8_t> bytes = base::as_byte_span(str);
func_with_const_ptr_size(bytes.data(), bytes.size());
base::span<uint8_t> mut_bytes = base::as_writable_byte_span(str);
func_with_mut_ptr_size(mut_bytes.data(), mut_bytes.size());

// Replace pointer and size with a span, though.
func_with_const_span(base::as_byte_span(str));
func_with_mut_span(base::as_writable_byte_span(str));
```

### Making a byte array (`span<uint8_t>`) from an object

You have:
```cc
uint8_t array[8];
uint64_t val;
two_byte_arrays(array, reinterpret_cast<const uint8_t*>(&val));
```

Spanified:
```cc
uint8_t array[8];
uint64_t val;
base::span<uint8_t> val_span = base::byte_span_from_ref(val);
two_byte_arrays(array, val_span.data());

// Replace an unbounded pointer a span, though.
two_byte_spans(base::span(array), base::byte_span_from_ref(val));
```

# Functions with array pointer parameters

Functions that receive a pointer argument into an array may read
or write out of bounds of that array if subsequent manual size
calculations are incorrect. Such functions should be avoided if
possible, or marked with the `UNSAFE_BUFFER_USAGE` attribute macro
otherwise.  This macro propagates to their callers that they must
be called from inside an `UNSAFE_BUFFERS()` region (along with
a corresponding safety comment explaining how the caller knows
the call will be safe).

The same is true for functions that accept an iterator instead
of a range type. Some examples of each are `memcpy()` and
`std::copy()`.

Again, calling such functions is unsafe and should be avoided.
Replace such functions with an API built on base::span
or other range types which prevents any chance of OOB memory
access. For instance, replace `memcpy()`, `std::copy()` and
`std::ranges::copy()` with `base::span::copy_from()`. And
replace `memset()` with `std::ranges::fill()`.
