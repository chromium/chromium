---
name: ios/compare-unvalidated-integers-against-cast-enums
description: When handling untrusted integers from IPC or web messaging, validate via `<Enum>_IsValid()` before casting to enums, or compare raw integers against integer-cast enums
rubric: safety
severity: should_fix
citations:
  - path: docs/security/compromised-renderers.md
    heading: Threat Model And Defenses Against Compromised Renderers
    quote: A compromised renderer can forge malicious IPC messages, impersonate a Chrome Extension content script, or use other techniques to trick more privileged parts of the browser.
  - path: styleguide/c++/checks.md
    heading: Failures beyond Chromium's control
    quote: 'Data across security boundaries: A compromised renderer should not be able to bring down the browser process (higher privilege). Bad IPC messages should be safely rejected by Chromium without the use of base::ImmediateCrash() or CHECK() etc. as part of normal control flow.'
---

**Rule.** Do not cast an unvalidated or arbitrary integer (such as from IPC,
dictionary parsing, JSON, renderers, or JavaScript execution) to an enumeration
type solely to check whether it matches a specific enum state; compare the
integer directly against the integer-cast enum value (using `std::to_underlying`
or `static_cast`) instead. Additionally, when converting an untrusted numeric
value across trust or process boundaries to an enum type—such as a Protocol
Buffer enum or C++ enum—validate the integer using the generated
`<EnumName>_IsValid(...)` helper or bounds checks before casting. Handle invalid
or unmapped values gracefully through normal control flow rather than casting
blindly or crashing via assertions.

**Why.** In C++, converting an arbitrary or out-of-range integer to an
enumeration type can produce an invalid enum value or trigger undefined behavior
if the value does not correspond to a valid enumerator. Direct comparisons
against integer-cast constants (`std::to_underlying` or `static_cast`) safely
verify states without instantiating invalid enums. When populating protobuf
messages or C++ data structures, validating via generated
`<EnumName>_IsValid(...)` helpers ensures untrusted inputs from compromised
renderers or external scripts cannot corrupt serialized message fields, cause
downstream type confusion, or bypass security boundaries.

## Examples

Bad:

```cpp
// 1. Casting arbitrary integer to enum to test state:
int status_int = dict->FindInt("status").value_or(-1);
auto status = static_cast<TextFragmentGenerationStatus>(status_int);
if (status == TextFragmentGenerationStatus::kSuccess) { ... }

// 2. Casting untrusted number directly to protobuf enum without validation:
if (std::optional<double> role = dict.FindDouble("role")) {
  node->mutable_content()->add_annotated_roles(
      static_cast<proto::AnnotatedRole>(static_cast<int>(*role)));
}
```

Good:

```cpp
// 1. Compare raw integer directly against integer-cast enum:
int status_int = dict->FindInt("status").value_or(-1);
if (status_int ==
    std::to_underlying(TextFragmentGenerationStatus::kSuccess)) { ... }

// 2. Validate with generated `<Enum>_IsValid()` before casting:
if (std::optional<double> role = dict.FindDouble("role")) {
  int role_int = static_cast<int>(*role);
  if (proto::AnnotatedRole_IsValid(role_int)) {
    node->mutable_content()->add_annotated_roles(
        static_cast<proto::AnnotatedRole>(role_int));
  }
}
```
