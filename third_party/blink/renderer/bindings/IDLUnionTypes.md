# How to use Blink IDL Union Types

Using [IDL union types](https://webidl.spec.whatwg.org/#idl-union) in
Blink is a bit tricky. Here are some tips to use union types
correctly.

## Generated classes

For each union type, the code generator creates a C++ class which is
used like an "impl" class of a normal interface type. The name of a
generated class is a
[type name](https://webidl.spec.whatwg.org/#dfn-type-name) of the
union type. For example, the code generator will create
`StringOrFloat` class for `(DOMString or float)`.

## Paths for generated classes

The code generator puts generated classes into separate files. You need
to include generated header files when you use union types in
core/modules.

The file name for a generated class is basically the same as its class
name, but we use some aliases to avoid too-long file names
(See https://crbug.com/611437 why we need to avoid long file names).

The paths for generated classes depend on the places union types are
used. If a union type is used only in IDL files under modules/, the
include path is `bindings/modules/v8/foo_or_bar.h`. Otherwise, the
include path is `bindings/core/v8/foo_or_bar.h`. For example, given
following definitions:

```webidl
// core/fileapi/file_reader.idl
readonly attribute (DOMString or ArrayBuffer)? result;

// dom/common_definitions.idl
typedef (ArrayBuffer or ArrayBufferView) BufferSource;

// modules/encoding/text_decoder.idl
DOMString decode(optional BufferSource input, optional TextDecodeOptions options);

// modules/fetch/request.idl
typedef (Request or USVString) RequestInfo;
```

The include paths will be:
- bindings/core/v8/string_or_array_buffer.h
- bindings/core/v8/array_buffer_or_array_buffer_view.h
- bindings/modules/v8/request_or_usv_string.h

Note that `array_buffer_or_array_buffer_view.h` is located under core/ even
it is used by `request.idl` which is located under modules/.

**Special NOTE**: If you are going to use a union type under core/ and
the union type is currently used only under modules/, you will need
to update the include path for the union type under modules/.

## Updating GN files
Due to the requirements of GN, we need to put generated file names
in GN files. Please update
`bindings/core/v8/generated.gni` and/or
`bindings/modules/v8/generated.gni` accordingly.
