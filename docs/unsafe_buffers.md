# Preventing OOB through Unsafe Buffers errors (aka Spanification)

Out-of-bounds (OOB) security bugs commonly happen through C-style pointers which
have no bounds information associated with them. We can prevent such
bugs by always using C++ containers. Furthermore, the Clang compiler can
warn about unsafe pointer usage that should be converted to containers.
When an unsafe usage is detected, Clang prints a warning similar to
```
error: unsafe buffer access [-Werror,-Wunsafe-buffer-usage]
```
and directs developers to this file for more information.

[TOC]

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

Code introducing UNSAFE_BUFFERS() macro invocations without corresponding
`// SAFETY:` comment should be summarily rejected during code review.

To allow for incremental conversion, code can be temporarily opted out by
using the `UNSAFE_TODO()` macro. This provides the same functionality as
the `UNSAFE_BUFFERS()` macro, but allows easier searching for code in need
of revision. Add TODO() comment, along the lines of
`// TODO(crbug.com/xxxxxx): resolve safety issues`.

## Container-based ecosystem

Containers may be owning types or view types. The common owning containers that
us contiguous storage are `std::vector`, `std::string`, `base::HeapArray`,
`std::array`. Their common view types are `base::span`, `std::string_view`,
`base::cstring_view`.

Other owning containers include maps, sets, deques, etc. These are not
compatible with `base::span` as they are not contiguous and generally do not
have an associated view type at this time.

We are using `base::span` instead of `std::span` in order to provide a type that
can do more than the standard type. We also have other types and functions to
work with ranges and spans instead of unbounded pointers and iterators.

The common conversions to spans are:
- `base::span<T>` replaces `T* ptr, size_t size`.
- `base::span<T, N>` replaces `T (&ptr)[N]` (a reference to a compile-time-sized
  array).
- `base::raw_span<T>` replaces `base::span<T>` (and `T* ptr, size_t size`) for
  class fields.

### Span construction
- `base::span()` constructor can make a span, and deduce the type and size,
  from:
  - a `T[N]` array
  - `std::array<T, N>`
  - `std::vector`
  - `std::string`
  - any contiguous range with `begin()` and `end()` methods.
  - any type with `T* data()` and `size_t size()` methods.
- `base::make_span<N>()` can make a fixed-size span from any range.
- `base::as_bytes()` and `base::as_chars()` convert a span’s inner type to
   `uint8_t` or `char` respectively, making a byte-span or char-span.
- `base::span_from_ref()` and `base::byte_span_from_ref()` make a span, or
  byte-span, from a single object.
- `base::as_byte_span()` and `base::as_writable_byte_span()` to make a
   byte-span (const or mutable) from any container that can convert to a
   `base::span<T>`, such as `std::string` or `std::vector<Stuff>`.

#### Padding bytes

Note that if the type contains padding bytes that were not somehow explicitly
initialized, this can create reads of uninitialized memory. Conversion to a
byte-span is most commonly used for spans of primitive types, such as going from
`char` (such as in `std::string`) or `uint32_t` (in a `std::vector`) to
`unit8_t`.

### Dynamic read/write of a span
- `base::SpanReader` reads heterogeneous values from a (typically, byte-) span
  in a dynamic manner.
- `base::SpanWriter` writes heterogeneous values into a (typically, byte-) span
  in a dynamic manner.

### Values to/from byte spans
In [`//base/numerics/byte_conversions.h`](../base/numerics/byte_conversions.h)
we have conversions between byte-arrays and big/little endian integers or
floats. For example (and there are many other variations):
- `base::U32FromBigEndian` converts from a big-endian byte-span to an unsigned
  32-bit integer.
- `base::U32FromLittleEndian` converts from a little-endian byte-span to an
  unsigned
- `base::U32ToBigEndian` converts from an integer to a big-endian-encoded
  4-byte-array.
- `base::U32ToLittleEndian` converts from an integer to a little-endian-encoded
  4-byte-array.

### Heap-allocated arrays
- `base::HeapArray<T>` replaces `std::unique_ptr<T[]>` and places the bounds of
the array inside the `HeapArray` which makes it a bounds-safe range.

### Copying and filling arrays
- `base::span::copy_from(span)` replaces `memcpy` and `memmove`, and verifies
that the source and destination spans have the same size instead of writing
out of bounds. It lowers to the same code as `memmove` when possible.
  - Note `std::ranges::copy` is not bounds-safe (though its name sounds like
    it should be).
- `std::ranges::fill` replaces `memset` and works with a range so you don't
  need explicit bounds.

### String pointers

A common form of pointer is `const char*` which is used (sometimes) to represent
a NUL-terminated string. The standard library gives us two types to replace
`char*`, which allow us to know the bounds of the character array and work with
the string as a range:

- `std::string` owns a NUL-terminated string.
- `std::string_view` is a view of a non-NUL-terminated string.

What’s missing is a view of a string that is guaranteed to be NUL-terminated so
that you can call `.c_str()` to generate a `const char*` suitable for C APIs.

- `base::cstring_view` is a view of a NUL-terminated string. This avoids the
  need to construct a `std::string` in order to ensure a terminating NUL is
  present. Use this as a view type whenever your code bottoms out in a C API
  that needs NUL-terminated string pointer.

### Use of std::array<T>.

The clang plugin is very particular about indexing a C-style array (e.g.
`int arr[100]`) with a variable index. Often these issues can be resolved
by replacing this with `std::array<int, 100> arr`, which provides safe
indexed operations. In particular, new code should prefer to use the
`std::array<T, N>` mechanism.

For arrays where the size is determined by the compiler (e.g.
`int arr[] = { 1, 3, 5 };`), the `std::to_array<T>()` helper function
should be used along with the `auto` keyword:
`auto arr = std::to_array<int>({1, 3, 5});`

## Avoid reinterpret_cast

### Writing to a byte span

A common idiom in older code is to write into a byte array by casting
the array into a pointer to a larger type (such as `uint32_t` or `float`)
and then writing through that pointer. This can result in Undefined Behaviour
and violates the rules of the C++ abstract machine.

Instead, keep the byte array as a `base::span<uint8_t>`, and write to it
directly by chunking it up into pieces of the size you want to write.

Using `first()`:
```cc
void write_floats(base::span<uint8_t> out, float f1, float f2) {
  out.first<4>().copy_from(base::byte_span_from_ref(f1));
  out = out.subspan(4u);  // Advance the span past what we wrote.
  out.first<4>().copy_from(base::byte_span_from_ref(f2));
}
```

Using `split_at()`:
```cc
void write_floats(base::span<uint8_t> out, float f1, float f2) {
  auto [write_f1, rem] = out.split_at<4>();
  auto [write_f2, rem2] = rem.split_at<4>();
  write_f1.copy_from(base::byte_span_from_ref(f1));
  write_f2.copy_from(base::byte_span_from_ref(f2));
}
```

Using `SpanWriter` and endian-aware `FloatToLittleEndian()`:
```cc
void write_floats(base::span<uint8_t> out, float f1, float f2) {
  auto writer = base::SpanWriter(out);
  CHECK(writer.Write(base::FloatToLittleEndian(f1)));
  CHECK(writer.Write(base::FloatToLittleEndian(f2)));
}
```

Writing big-endian, with `SpanWriter` and endian-aware `U32ToBigEndian()`:
```cc
void write_values(base::span<uint8_t> out, uint32_t i1, uint32_t i2) {
  auto writer = base::SpanWriter(out);
  CHECK(writer.Write(base::U32ToBigEndian(i1)));
  // SpanWriter has a built-in shortcut to do the same thing.
  CHECK(writer.WriteU32BigEndian(i2));
  // Verify we wrote to the whole output. We can put a size parameter in the
  // `out` span to push this check to compile-time when it's a constant.
  CHECK_EQ(writer.remaining(), 0u);
}
```

Writing an array to a byte span with `copy_from()`:
```cc
void write_floats(base::span<uint8_t> out, std::vector<const float> floats) {
  base::span<const uint8_t> byte_floats = base::as_byte_span(floats);
  // Or skip the first() if you want to CHECK at runtime that all of `out` has
  // been written to.
  out.first(byte_floats.size()).copy_from(byte_floats);
}
```

### Reading from a byte span

Instead of turning a `span<const uint8_t>` into a pointer of a larger type,
which can cause Undefined Behaviour, read values out of the byte span and
convert each one as a value (not as a pointer).

Using `subspan()` and endian-aware conversion `FloatFromLittleEndian`:
```cc
void read_floats(base::span<const uint8_t> in, float& f1, float& f2) {
  f1 = base::FloatFromLittleEndian(in.subspan<0, 4>());
  f2 = base::FloatFromLittleEndian(in.subspan<4, 4>());
}
```

Using `SpanReader` and endian-aware `U32FromBigEndian()`:
```cc
void read_values(base::span<const uint8_t> in, int& i1, int& i2, int& i3) {
  auto reader = base::SpanReader(in);
  i1 = base::U32FromBigEndian(*reader.Read<4>());
  i2 = base::U32FromBigEndian(*reader.Read<4>());
  // SpanReader has a built-in shortcut to do the same thing.
  CHECK(reader.ReadU32BigEndian(i3));
  // Verify we read the whole input. We can put a size parameter in the `in`
  // span to push this check to compile-time when it's a constant.
  CHECK_EQ(reader.remaining(), 0u);
}
```

## Patterns for spanification

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

### Avoid std::next() for silencing warnings, use ranges

When we convert `pointer + index` to `std::next(pointer, index)` we silence the
`Wunsafe-buffer-usage` warning by pushing the unsafe pointer arithmetic into
the `std::next()` function in a system header, but we have the same unsafety.
`std::next()` does no additional bounds checking.

Instead of using `std::next()`, rewrite away from using pointers (or iterators)
entirely by using ranges. `span()` allows us to take a subset of a contiguous
range without having to use iterators that we move with arithmetic or
`std::next()`.

Instead of using pointer/iterator arithmetic:
```cc
// Unsafe buffers warning on the unchecked arithmetic.
auto it = std::find(vec.begin() + offset, vec.end(), 20);
// No warning... But has the same security risk!
auto it = std::find(std::next(vec.begin(), offset), vec.end(), 20);
```

Use a range, with `span()` providing a view of a subset of the range:
```cc
auto it = std::ranges::find(base::span(vec).subspan(offset), 20);
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
