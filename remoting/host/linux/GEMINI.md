# Chrome Remote Desktop Linux Host

This directory contains the implementation of the Chrome Remote Desktop host for
Linux.

## GVariant Library Usage

A set of C++ wrappers for GLib's `GVariant` is used extensively in this
directory for type-safe D-Bus communication and data handling. It is highly
recommended to be familiar with the
[GVariant type system](https://docs.gtk.org/glib/struct.VariantType.html) before
working with this code.

### Key Components

*   **`gvariant_type.h`**: Defines the `Type` class, which holds GVariant type
    strings. It allows for compile-time type checking and provides utilities for
    inspecting and unpacking GVariant types.
*   **`gvariant_ref.h`**: Provides `GVariantRef`, a scoped, reference-counted
    wrapper around `GVariant*`. It supports:
    *   Automatic reference counting (RAII).
    *   Type-safe conversions to/from C++ types (e.g., `std::string`,
        `std::vector`, `std::tuple`) using `From()`, `Into()`, and `TryInto()`.
    *   Compile-time type constraints when a type string is provided as a
        template argument (e.g., `GVariantRef<"s">`).
*   **`gvariant_dict_builder.h`**: A helper class (`GVariantDictBuilder`) for
    easily constructing GVariant dictionaries of type `a{sv}` (string keys,
    variant values), which are common in D-Bus APIs.

### Recommendations for Future Agents

*   **Read the headers**: For detailed usage examples and available methods,
    please read the comments in the header files listed above.
*   **Use `GVariantRef`**: Prefer using `GVariantRef` over raw `GVariant*` to
    ensure proper memory management and type safety.
*   **Type Safety**: Leverage the template parameters of `GVariantRef` whenever
    the expected D-Bus type is known at compile time.
*   **Error Handling**: Use the `TryInto()` and `TryFrom()` methods, which
    return `base::expected`, to gracefully handle type mismatches during D-Bus
    communication.
