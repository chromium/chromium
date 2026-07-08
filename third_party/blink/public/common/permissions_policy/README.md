# Adding a new enum-typed Document Policy feature

When adding a new `value_type: "Enum"` feature to `document_policy_features.json5`,
the following files must be updated:

1. **`document_policy_features.json5`** — add the feature entry with `value_type: "Enum"`
   and a numeric `default_value`. The default value must use the underlying integer
   (not a named constant), since this file is processed before C++ enums are available.

2. **`document_policy_enum_values.h`** — add cases to both:
   - `DocumentPolicyEnumTokenToValue()` — maps header token strings to int32 values
   - `DocumentPolicyEnumValueToToken()` — maps int32 values back to token strings;
     return `std::nullopt` for any sentinel/default value that has no header representation

3. **The enum header** (e.g. `js_profiling_mode.h`) — define the C++ enum with
   explicit integer values matching those used in `document_policy_features.json5`.

4. **`document_policy_feature.mojom`** — add the feature to the `DocumentPolicyFeature`
   enum.

5. **`BUILD.gn` / `sources.gni`** — add any new header files to the appropriate build
   target.

## Sentinel default values for enum features

The Document Policy framework always initialises every feature to its `default_value`,
even when the header is absent. For enum features that should fall back to another
policy when not explicitly set (rather than having a distinct "off" token), use `0` as
a sentinel default and check `value >= 1` in the consuming code. This keeps value 0
structurally unreachable from a header, making "not set" unambiguous.

See `document_policy_enum_values.h` and `profiler_group.cc` (`GetProfilingMode`) for a
worked example with `js-profiling-mode`.
