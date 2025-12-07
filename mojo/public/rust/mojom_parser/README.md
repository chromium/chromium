This crate contains an implementation of a safe, simple, and generic Mojom parser
written in Rust.

This crate is still in extremely active development! We're still figuring out
the final shape and how it will fit into the overall rust mojo bindings.

FOR_RELEASE: These comments indicate TODOs that should be addressed before we
finalize the minimum viable product for the rust bindings. For example, fill
this out more!

## Genericity

A key goal of this parser is to be agnostic to the actual Mojom files that it
parses. Unlike the bindings for other languages, which generate specialized
parsing code for each type in each Mojom file, this parser works for _all_ of
them (at the cost of performance).

This is achieved by representing the parser's inputs and outputs as miniature
ASTs: rust enums that describe the possible types and values a Mojom file could
describe. For each Mojom file, the bindings generator will be responsible for
generating a corresponding rust type, and mapping it to/from the enums, but the
generator will not have to write any parsing code itself.

## Wire Format
If you want to understand the behavior of this crate, it is highly recommended
that you first read (or at least skim) the
[Mojom Wire Format Specification](https://docs.google.com/document/d/1YyCtD2-TtBsvhV8k53N3yGrWHVMcKumFnhHxUoXyQU0/edit?usp=sharing)
document, which describes how types are laid out after serialization. All parts
of the crate assume familiarity with the basic concepts described there.

However, note that the document was written well after the wire format
was implemented in practice, so it may not perfectly reflect actual behavior
of other bindings generators (C++ in particular). If you notice something wrong,
leave a comment!

## Directory structure

Logically, this directory contains a single `mojom_parser` crate, but it's
split into several internal crates for build-system reasons.

The crates are:

`mojom_parser`:
* lib_pub.rs: The only file in this crate, which re-exports only the things that
  outside users should have access to.

`mojom_parser_core`:
* lib_core.rs: The crate root, which serves only to list the other files.
* api.rs: Defines functions which are meant to be called by external users to
  serialize/deserialize data.
* ast.rs: Defines the abstract syntax of mojom types and values.
* errors.rs: Defines error and result types for the parser and deparser functions.
* pack.rs: Translates mojom types to their wire format.
* parse_*.rs: Defines various levels of functionality for deserializing binary data:
  * parse_primitives.rs: Basic parsers, which return primitive datatypes
  * parse_values.rs: Parsers which take a single encoded value (possibly a
    recursive one like a struct) and return a mojom value.
  * parse_messages.rs: Functionality for parsing entire mojom messages.
* deparse_*:  Defines various levels of functionality for serializing binary data:
  * deparse_values.rs: Serializers which take a mojom value and return its binary
    representation.
* parsing_trait.rs: Defines traits for rust types that enable them to be
  serialized/deserialized.

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
