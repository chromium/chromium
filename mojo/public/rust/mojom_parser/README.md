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

## Crate structure

* lib.rs: The crate root, which serves only to define the API of the crate
  via exports.
* ast.rs: Defines the abstract syntax of mojom types and values.
* pack.rs: Translates mojom types to their wire format.
* parse_*: Defines various levels of parsing functionality:
  * parse_primitives: Basic parsers, which return primitive datatypes
  * parse_values: Parsers which take a single encoded datatype (possibly a
    recursive one like a struct) and return a mojom value.
  * parser_messages: Parsers for entire mojom messages.
