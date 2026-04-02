# The Mojom Value Parser

This crate contains an implementation of a safe, simple, and generic Mojom
parser and deparser written in Rust. Its job is to translate between Rust types
and binary data that was encoded using the [Mojom Wire Format](../../docs/wire_format_spec.md).
It is strongly recommended to understand, or at least skim, the wire format
before working with the parsing code.

Note: Since the parser targets binary encoded data, parsing and deparsing can
also be referred to as deserialization and serialization, respectively. The term
"parsing" is used throughout this crate mostly as a historical artifact.
Everything in this documentation that refers to the parser also applies to the
deparser.

The primary goals for this parser are:

1. Maintainability: It is (hopefully!) easy to understand, extend, and modify.
2. Abstraction: It works for all Mojom types without needing to be specialized.

Note that performance is explicitly a _non_-goal; we'd certainly like it to be
fast, but having comprehensible code is paramount, and highly optimized parsers
are frequently much harder to understand. In the future, we may implement a new
parser with an emphasis on speed, using this one as a reference.

Note that since this parser is abstract, it has significantly lower binary size
impact than one which generated highly-specialized code for each individual type.

## Overview

### Mojom Values

The parser centers around the concept of a **Mojom Value**, which is any value
that can be described in a Mojom file (e.g. uint32, a user-defined struct, etc).
The parser captures this using a `MojomValue` type, which is capable of
representing any such value:

```Rust
// ast.rs:
pub enum MojomValue {
    Bool(bool),
    Int8(i8),
    UInt8(u8),
    Struct(Vec<MojomValue>),
    ...
}
```

For example, the value `Foo { x: 1u8, y: true }` corresponds to
`Struct(vec![UInt8(1), Bool(true)])`.

The `MojomValue` type serves as an intermediary between concrete Rust types
(like `Foo`), and encoded binary data (`[u8]`). In order to parse a value,
we first create a `MojomValue` from the data, and then convert that into a
specified Rust type. Deparsing is the same, but in the opposite direction:

```text
Foo <---> MojomValue <---> [u8]
```

### Mojom Types

Because the Mojom wire format is not self-describing, a given binary message
could correspond to many different possible `MojomValue`s. Therefore, when
parsing, we need to know the expected type of the data in the message, so we
know how to interpret the bits. To accommodate this, we define a `MojomType`
type which mirrors `MojomValue`:

```Rust
// ast.rs:
pub enum MojomType {
    Bool,
    Int8,
    UInt8,
    Struct(Vec<MojomType>),
    ...
}
```

The `Foo` value above would map to the `MojomType` `Struct(vec![UInt8, Bool])`.

### Wire Types

Unfortunately, the wire format is not a straightforward serialization; there are
various optimizations to the encoded format to e.g. save space. Therefore, we
additionally define a `MojomWireType` type, which specifies the on-the-wire
representation of a given `MojomType`. This type is used as an input to the
parsing and deparsing functions.

The packing algorithm in `pack.rs` is responsible for converting a `MojomType`
into the corresponding `MojomWireType`.

### Rust <---> Mojom conversions

The process of converting Rust values to `MojomValue`s is managed by the
`MojomParse` trait (`parsing_trait.rs`). The trait provides functions for

1. Getting the `MojomType` that corresponds to the Rust type.
2. Converting value of the Rust type to a `MojomValue` (infallible).
3. Converting a `MojomValue` to the value of the Rust type.

Step (3) may fail if the `MojomValue` has the wrong type; for example, if we
tried to convert `MojomValue::Int8(3)` to a `Foo` (which is a `Struct`).

The `MojomParse` trait is automatically implemented for most types (primitives
and important builtins like vectors). For structs and enums, it can be derived
using a proc macro (`parsing_attribute.rs`).

### Mojom <--> binary conversions

This is the actual parsing/deparsing step (`parse_values.rs`, `deparse_values.rs`).
Both take the wire type as one of the inputs; deparsing additionally takes the
`MojomValue` to be serialized, while parsing instead _produces_ a `MojomValue`.

Both parsing and deparsing proceed by inspecting the wire type to determine what
the next thing on the wire should be. Deparsing looks up that part of the value
and writes it to the message, whereas parsing reads the next N bytes and
interprets them according to the wire type.

For more information about parsing and deparsing, see the relevant files, and
make sure to read over the [Mojom Wire Format](../../docs/wire_format_spec.md)
beforehand.

## Directory structure

Logically, this directory contains a single `mojom_parser` crate, but it's
split into several internal crates for build-system reasons.

The crates are:

`mojom_parser` (publicly visible):

* lib_pub.rs: The only file in this crate, which re-exports only the things that
  outside users should have access to.

`mojom_parser_core` (internal):

* lib_core.rs: The crate root, which serves only to list the other files.
* api.rs: Defines functions which are meant to be called by external users to
  serialize/deserialize data.
* ast.rs: Defines the abstract syntax of mojom types and values.
* errors.rs: Defines error and result types for the parsing functions.
* pack.rs: Translates mojom types to their wire format.
* parse_*.rs: Defines various levels of functionality for deserializing binary data:
  * parse_primitives.rs: Basic parsers, which return primitive datatypes
  * parse_values.rs: Parsers which take a single encoded value (possibly a
    recursive one like a struct) and return a mojom value.
* deparse_*:  Defines various levels of functionality for serializing binary data:
  * deparse_values.rs: Serializers which take a mojom value and return its binary
    representation.
* parsing_trait.rs: Defines traits for rust types that enable them to be
  serialized/deserialized.
* message_header.rs: Defines a type representing the header of a mojom message.
  This is mostly a hack and should be ignored if working on the crate as a whole.

`parsing_attribute`:

* parsing_attribute.rs: Defines proc macros for deriving the traits defined in
  `parsing_trait.rs`.

`mojom_parser_unittests`:

* test/
  * lib.rs: The crate root, which serves only to list the other files
  * test_mojomparse.rs: Tests for the parsing trait and its derivation.
  * test_parser.rs: Tests that ensure the parser and deparser produce the
    expected output.
* test_util/
  * cxx.rs: FFI bindings for calling a C++ function needed by `test_mojomparse.rs`
  * parser_unittests.test-mojom: Contains the mojom definitions used by the
    testing functions.
