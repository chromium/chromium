# Preventing OOB through Unsafe Buffers errors (aka Spanification)

Out-of-bounds (OOB) security bugs commonly happen through pointers
which have no bounds information associated with them. We can prevent such
bugs by always using containers.

Entire directories have been opted out of unsafe pointer usage
warnings through the
[`//build/config/unsafe_buffers_paths.txt`](../build/config/unsafe_buffers_paths.txt)
file. Our [compiler](../tools/clang/plugins/UnsafeBuffersPlugin.cpp) enables
the `-Wunsafe-buffer-usage` warning in any file that is not
opted out. As we convert unsafe pointers to safe constructs like
`base::span`, `base::HeapArray` and `std::vector`, we will
remove paths from that file to enable the warnings across the
codebase. To incrementally apply the warnings across a whole directory,
individual files [can be opted-out](#controlling-warnings-for-a-single-file)
of the warning.

## Patterns for spanification

Most pointers are unowned references into an array (or vector)
and the most appropriate replacement for the pointer is
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

Note: This pattern was UB-prone in addition to bounds-unsafe, as it can produce
a pointer with incorrect alignment. reinterpret_cast is very easy to hold wrong
in C++ and is worth avoiding.

You have:
```cc
uint8_t array[44] = {};
uint32_t v1 = *reinterpret_cast<const uint32_t*>(array);  // Front.
uint64_t v2 = *reinterpret_cast<const uint64_t*>(array + 6);  // Middle.
```

Spanified:
```cc
#include "base/numerics/byte_conversions.h"
...
uint8_t array[44] = {};
uint32_t v1 = base::U32FromLittleEndian(base::span(array).first<4u>());  // Front.
uint64_t v2 = base::U64FromLittleEndian(base::span(array).subspan<6u, 8u>());  // Middle.
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
