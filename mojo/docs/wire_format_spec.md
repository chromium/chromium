# Mojom Wire Format Specification

This document serves as mostly-rigorous specification of the wire format uses by
mojo messages. For the original designs, see the documents on the
[header](https://docs.google.com/document/d/13pv9cFh5YKuBggDBQ1-AL8VReF-IYpFOFpRfvWFrwio/edit?tab=t.0)
and [archive](https://docs.google.com/document/d/1jNcsxOdO3Al52s6lIrMOOgY7KXB7TJ8wGGWstAHiTd8/edit?tab=t.0)
formats. The document on the
[validation testing format](https://docs.google.com/document/d/1-y-2IYctyX2NPaLxJjpJfzVNWCC2SR2MJAD9MpIytHQ/edit?tab=t.0#heading=h.10jrlwscpsxt)
is also helpful.

This document is descriptive; if it deviates from the in-practice format, we
should update the doc instead of the format. This document is also incomplete;
footnotes mark locations where there may be unknown details.

## Background

Mojom messages are serialized into a series of bytes, comprised of a
[Mojom header](#header-format), the serialized data (encoded as a struct), and
optionally an array of associated interface IDs. Each section follows the
previous one at an 8-byte alignment.

The Mojom format is not self-describing; the type of the serialized data is
required to decode the message. Furthermore, the data in the body of each struct
is packed, so it may not appear in the order it is defined in the Mojom file.
Packing does not cross struct boundaries.

See the [relevant section of the docs](https://chromium.googlesource.com/chromium/src/mojo/+/HEAD/public/tools/bindings/README.md#primitive-types)
for descriptions of (most of) Mojom’s types. It is important to be familiar with
how Mojo uses ordinal values (discussed in [this section](https://chromium.googlesource.com/chromium/src/mojo/+/HEAD/public/tools/bindings/README.md#versioned-structs)),
since they sometimes determine ordering of values within the message.

IMPORTANT: Mojo (and Mojom) are designed for inter-_process_ communication. They
are not meant for communication between hosts. To be safe, you should never try
to interpret an encoded message on a different host. Instead, you should use
a network-safe protocol (e.g. protobuf) from the beginning.

## Key Concepts

This section lays out the high-level ideas that underpin the Mojom wire format.

### Leaf vs. Structured Data

The wire format draws a distinction between two types of data:
**leaf (or “simple”) values** and **structured data**. The difference is that
leaf values do not contain other values inside themselves, whereas structured
data does. There are three types of structured data: structs, arrays, and
unions.

The two types of data have different properties when serialized. Note that all
data values appear inside the body of an enclosing structured data type, except
for a single top-level struct containing all the arguments to the message.
This struct is generated from the interface definition, and is not visible to
the user.

A leaf data value:

1. Has a fixed size.  
1. Its data layout may depend on the type of structured data enclosing it.

A structured data value:

1. Has a fixed-size **body**, which is prefixed by an 8-byte **header** and
   suffixed by a variable-length **tail**.  
   1. Exception: array bodies are composed of a variable number of
      fixed-size elements.  
1. The first 4 bytes of the header store the size in bytes of the header \+ the
   body (not the tail).  
   1. Padding bytes in the body are included in the size for structs and unions,
      but not arrays.  
1. The second 4 bytes of the header store various information depending on the
   type of the structured data.  
1. When structured data is nested inside the body of an enclosing piece of
   structured data, it is represented as a [pointer](#pointers) to a position in
   the tail, where the actual structured data appears.  
   1. Exception: If a union is nested in the body of a struct or array,
      the union value itself appears in the body, instead of a pointer.  
      1. If the union contains a pointer, it points to a position in the tail
      of the _enclosing body_ instead of the union’s tail (which is empty).  
1. Any nested structured data appears in the tail, the order that their pointers
   appear in the body.  
   1. The [original specification](https://docs.google.com/document/d/1jNcsxOdO3Al52s6lIrMOOgY7KXB7TJ8wGGWstAHiTd8/edit?tab=t.0#bookmark=id.5tmvuo17xs4r) has a more detailed description of this, with a very helpful illustration.

### Pointers

Note that these are an implementation detail of the Mojom format, and do not
represent C/C++ pointers.

1. A **pointer** is a `uint64`[1] pointing at a later part of the message,
   typically the tail of a structured data value.  
   1. Note that since pointers are unsigned, cycles are impossible.  
2. The pointer’s value is the number of bytes from the beginning of the pointer
   to the beginning of the pointee.  
3. A value of 0 denotes a null pointer.

Note: as a corollary of (2), the minimum value for a non-null pointer is `8`, if
it immediately precedes the data it points to (since the pointer itself is
always 8 bytes).

Note: As a corollary of (2) and the alignment rules below, pointer values must
always be divisible by 8 (since the pointer and pointee are both 8-byte-aligned).

[1]: It's possible these are pointer-sized instead of always 64-bit.

### Alignment

Data must be aligned inside the message, with respect to the beginning of the
message. Each value or pointer is prefixed by 0 or more padding bytes in order
to ensure that it begins at an appropriate location.

1. Leaf values and pointers are aligned to multiples of their size in bytes.  
2. Structured data is 8-byte-aligned.

### Endianness

All values are host-endian[2].

[2]: There may be some parts of the bindings or related code that assume
     little-endian.

### Ordinals

The **ordinal** of a field of a struct is a number indicating where in the
struct it was declared. The first field has ordinal 0, the second has ordinal 1,
and so on.

Since fields in the body of a struct are [packed](#field-packing), the order
they appear on the wire is not necessarily ordinal order.

### Nullability

Mojom allows most types to be declared as nullable. Nullability is represented
in different ways for different types. Note that a type cannot be
multiply-nullable (i.e. `int??` Is not a valid mojom type).

Leaf Data:

1. Nullable leaf data in a struct is encoded as if the struct contained two
separate fields: a `bool` tag and the value itself. The tag is guaranteed to
appear before the value (it has a lower ordinal value).  
   1. If the value is present, the first field is `true`.  
   2. Otherwise, the first field is `false`. The value is still serialized, but
      its bytes are undefined and considered meaningless.  
   3. Note that since the value is encoded as _two_ struct fields, they are not
   guaranteed to be packed contiguously, and the boolean field might be [packed
   in with other bools](#booleans).  
2. Nullable leaf data in an array is encoded using a bitfield with the tags for
   each array element.  
   1. The bitfield appears at the beginning of the array’s body, before the
   element values.  
   2. The tag for the first element is in the 1s place of the first byte of the
   bitfield.  
   3. The tag for the eleventh element is in the 4s place of the second byte of
   the bitfield, and so on.  
   4. Padding is emitted after the bitfield to meet the element type’s alignment
   requirements.  
   5. Since the bitfield and padding are part of the body, they are included in
   the size reported in the array’s header.  
3. Nullable leaf data is not allowed in unions.  
4. Unlike other types of leaf data, a null handle is represented by the special
   value `0xffffffff` instead of using a tag bit.  
   1. Therefore, nullable handle values may appear in unions.

Structured Data:

1. Nullable union _values_ (not pointers\!) use the 16-byte value `0` to
   represent `null`.  
2. Null structured data is represented as a null pointer (`0`) in the body of
   the enclosing data. If a pointer is null, no corresponding data is emitted
   in the enclosing data’s tail.  
   1. The top-level enclosing struct is never nullable.

## Value Format

### Numbers

1. Numbers are leaf data.  
2. Integers are represented as themselves (e.g. a 32-bit integer is represented
   as4 bytes on the wire).  
3. Floats and doubles use IEEE-754 format.

Note: Java (and maybe javascript) don't support unsigned integers, so they will
interpret all integers as signed. Sending unsigned integers cross-language is
therefore unsafe.

### Booleans

1. Booleans are leaf data. Their layout is subject to special packing rules when
   inside a struct or array.  
2. A single boolean is represented as a byte with value 0 or 1\.  
3. If multiple booleans are present within a struct or array, they will be
   packed together in **bitfields** that store up to 8 booleans in a single byte.  
   1. The second boolean will be packed into the 2s bit of the byte containing
   the first boolean.  
   2. The third boolean will be packed into the 4s bit, and so on.  
   3. The ninth boolean will begin a new byte, and so on.  
   4. Note that each packed boolean retains its own ordinal; they are still
   considered individuals for the purpose of e.g. counting the number of
   elements in an array.

### Structs

1. Structs are structured data.  
2. The second 4 bytes of a struct’s header contain the version number of the
struct, as defined in the Mojom file used by the serializer.  
3. The size of a struct must be a multiple of 8 bytes[3]. Padding bits are
inserted at the end to ensure this.  
   1. Note: since these bits are part of the body, they are included in the size
   reported in the struct’s header.  
4. Fields of a struct are ordered according to the
[field packing algorithm](#field-packing).

[3]: It's possible this is platform-dependent.

### Arrays

1. Arrays are structured data.  
2. The second 4 bytes of an array’s header is the number of elements in the
array.  
3. The body of an array consists of all its elements laid out in order with no
gaps between them.  
   1. After the body, padding bytes are inserted to alignment 8\.  
   2. Unlike structs, these padding bytes are _not_ included in the byte size in
   the array’s header.  
   3. Exception: Arrays of nullables additionally have a bitfield before the
   elements; see the [Nullability](#nullability) section for details.  
4. There is no difference in wire format between a fixed-size array and a
variably-sized array with the same elements.

### Strings

1. In practice, the `string` type is an alias for `array<uint8>`, except that
the language bindings map it to a different source type (e.g. `std::string` in
C++).  
   1. Mojo imposes no requirements on the contents of the array. In particular,
   strings are not null-terminated, and null bytes in the middle of the string
   are valid.  
   2. In the future, Mojo hopes to require that strings are UTF-8 encoded, but
   [they’re not quite there yet](https://g-issues.chromium.org/issues/328278701).

### Maps

1. A `map<K, V>` is encoded precisely as if it were defined as  
   `struct Foo { array<K> keys, array<V> values }`.  
   1. The elements are associated by index; They key at `keys[i]` maps to the
   value at `values[i]`.
2. Therefore, maps are structured data.  
3. The second 4 bytes of a map header are the implicit struct’s version field,
which is always 0\.

### Enums

1. Enums are leaf data.  
2. Enums are encoded as a signed `int32` containing their underlying
discriminant value.  
   1. Note that enum values are validated during deserialization to ensure they
   have a known value.  
   2. If an enum has a default value, then unknown values are mapped to the
   default.

Note: If an enum is nested inside of a struct/interface, then the C++ bindings
generate an enum at the top level with the name `StructName_EnumName`, etc.

### Unions

1. Unions are structured data.  
2. The last 4 bytes of the union’s header contain the discriminant of the
union’s value.  
   1. Note: the discriminant is defined by the Mojom file. The first variant
   listed has discriminant 0, the second one has discriminant 1, etc.  
3. Union bodies consist of an 8 byte value.  
   1. Note: this means the union’s size will always be 16 bytes.  
4. Take note of the [exception](#leaf-vs-structured-data) listed in the
structured data section, regarding how unions in a struct or array are embedded
directly in the enclosing body instead of behind a pointer.

Note: Recursive unions (which contain themselves as a possible field) are not
allowed, but _mutually_ recursive unions are.

### Handles

Mojom handles do not appear directly in the body of the encoded message.
Instead, they are passed via a separate vector of handles alongside the message
body. They are referred to from the message body via indices into that vector.

1. Handles are leaf data (but have several exceptions to the normal rules)  
2. All handle types (untyped, message pipe, data pipe, etc) are treated
identically during encoding/decoding.  
3. Each handle is encoded as a 32-bit index into the associated handle array.  
   1. The first handle has index 0, the next index 1, etc.  
   2. The index value `0xffffffff` is reserved for nullable handle types only
   (and indicates the `None` value)  
4. Handles are numbered by depth-first traversal of the type being encoded.  
   1. i.e. Earlier struct fields have smaller indices than later ones,
   similarly for array elements.  
   2. Handles with a value of `None` are skipped when assigning indices.

Note that since handle values should be globally unique, no index should be
re-used within a message. Similarly, each handle in the separate vector should
be referenced exactly once.

### Remotes and Receivers

The `pending_receiver` type is represented on the wire as its underlying
message pipe [handle value](#handles).

The `pending_remote` type is represented as 64-bit _pair_ of the underlying handle
value and a 32-bit [version field](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/cpp/bindings/pending_associated_remote.h;l=112;drc=8abea14deda089834ba142a35e8342014812df55).

1. The first 32 bits of the `pending_remote` value are the handle value.
2. The second 32 bits contain the 32-bit version value.
3. The two values are encoded independently, but unlike
[nullable primitives](#nullability) the two halves cannot get separated during
packing.
   1. Because they are independent, a null remote is encoded as `0xffffffff` in
      the first 32 bits. The remaining 32 bits may still contain a valid version
      number.
4. Despite being 8 bytes, pending remotes are 4-byte-aligned.

### Associated Remotes and Receivers

The `pending_associated_remote` and `pending_associated_receiver` types are
encoded like the `pending_remote` and `pending_receiver` types, but
rather than indexing into an external array of handle values, they index into
a separate array of interface IDs.

If (and only if) a message contains an associated remote or receiver, the
payload will be immediately followed by an `array<uint32>`, and the
`payload_interface_ids` header field will contain a pointer to that array. The
array is encoded like [any other array](#arrays). It is not considered part of
the payload. Interface IDs are 32-bit integers.

Associated remotes and receivers are otherwise encoded identically to their
non-associated equivalents. Since they index into a different array, their
indices are independent of the ones used by remote/receivers/raw handles.

Note that interface IDs are generated and attached to associated endpoints as
part of (de)serialization. That is, encoding an associated remote or receiver
is a stateful operation and has side-effects on other parts of the system. That
process is independent of the wire format, however.

## Other Details

### Field Packing

Fields of a struct are packed in order to take up less space. For the full,
canonical algorithm, see
[the implementation in python](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/tools/mojom/mojom/generate/pack.py?q=pack.py&ss=chromium). Field packing does not cross struct boundaries.

At a high level, the algorithm operates as follows:

1. Arrange all fields in ordinal order.
1. For each field, if it fits in a space between two prior fields, move it to
the earliest such space.
   1. A field fits in a space if the space is large enough to contain it after
   considering alignment requirements.
1. If the field is a boolean, then also consider whether it fits into an
existing bitfield.
   1. If so, move it to the first available bit in earliest bitfield.
   1. Otherwise, move it to the earliest space where it fits, and start a new
   bitfield there.

For example, the following struct:

```C++
struct Foo {
   uint8 n8;
   uint64 n64;
   uint16 n16_1;
   bool b1;
   uint16 n16_2;
   uint32 n32;
   bool b2;
}
```

Would be packed as

```
n8 [bitfield: 0 0 0 0 0 0 b2 b1] n16_1 n16_2 [pad 16] n64 n32
```

Because:

1. `n8` and `n64` have nowhere earlier to move.
1. `n16_1` fits between them, but is 2-byte-aligned and so must be placed one
byte after `n8`.
1. `b1` fits between `n8` and `n16_1` and starts a new bitfield in the 1s place.
1. `n16_2` fits exactly between `n16_1` and `n64`.
1. `n32` doesn't fit in the remaining 16 bytes between `n16_2` and `n64`, so it
stays where it is.
1. `b2` fits into the existing bitfield in the 2s place.

## Header Format

Headers are serialized as if they were a Mojom struct with the following
definition:

```C++
struct MessageHeader {
   // Interface ID for identifying multiple interfaces running on the same
   // message pipe.
   uint32 interface_id;
   // Message name, which is scoped to the interface that the message belongs to.
   uint32 name;
   // A combination of zero or more of the flag constants defined within the
   // Message class.
   uint32 flags;
   // A unique (hopefully) value for a message. Used in tracing, forming the
   // lower part of the 64-bit trace id, which is used to match trace events for
   // sending and receiving a message (`name` forms the upper part).
   uint32 trace_nonce;
   // Only used if either kFlagExpectsResponse or kFlagIsResponse is set in
   // order to match responses with corresponding requests.
   [MinVersion = 1] uint64 request_id;
   // Stores a Mojom Pointer (see #Pointers above) to the payload struct which
   // appears after this header. Note: The `mojo_ptr_t` type can't be written
   // down in real mojom. It represents a pointer-sized integer.
   [MinVersion = 2] mojo_ptr_t payload;
   // Stores a Mojom Pointer (see #Pointers above) to the list of interface IDs
   // that optionally appears after the payload.
   [MinVersion = 2] mojo_ptr_t payload_interface_ids;
   // Stores the timestamp when this message was created.
   [MinVersion = 3] int64_t creation_timeticks_us;
}
```

See [this file](https://source.chromium.org/chromium/chromium/src/+/main:mojo/public/cpp/bindings/lib/message_internal.h)
for the C++ definition of the header type.
