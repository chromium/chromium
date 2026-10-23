---
name: ios/avoid-runtime-string-splitting-for-static-lists
description: When initializing containers or collections from known string constants, use a constexpr array of std::string_view instead of splitting delimited strings at runtime with base::SplitString
rubric: readability
severity: nit
---

**Rule.** Do not encode a static collection of string constants as a delimited
string (such as comma-separated or space-separated) and parse it at runtime with
`base::SplitString` or `base::SplitStringPiece`. Instead, declare a compile-time
sequence of string constants using a `constexpr` array (such as
`constexpr auto kItems = std::to_array<std::string_view>({ ... })`), a
`base::MakeFixedFlatSet`, or a container of `std::string_view`. Initialize
destination containers directly from the compile-time collection without runtime
tokenization.

**Why.** Parsing a delimited string at runtime via `base::SplitString` incurs
unnecessary parsing overhead, string scanning, and heap allocations for
temporary strings and vectors. Defining elements known at compile time as a
`constexpr` array of `std::string_view` allows direct, zero-allocation container
initialization and compile-time validation of static lists.

## Examples

Bad:

```cpp
namespace {
const char kSupportedAuthSchemes[] = "basic,digest,ntlm";
}

void IOSIOThread::CreateDefaultAuthPreferences() {
  std::vector<std::string> supported_schemes =
      base::SplitString(kSupportedAuthSchemes, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences =
      std::make_unique<net::HttpAuthPreferences>();
  globals_->http_auth_preferences->SetAllowedSchemes(
      base::flat_set<std::string>(supported_schemes.begin(),
                                  supported_schemes.end()));
}
```

Good:

```cpp
namespace {
constexpr auto kSupportedAuthSchemes =
    std::to_array<std::string_view>({
        net::kBasicAuthScheme,
        net::kDigestAuthScheme,
        net::kNtlmAuthScheme,
    });
}

void IOSIOThread::CreateDefaultAuthPreferences() {
  globals_->http_auth_preferences =
      std::make_unique<net::HttpAuthPreferences>();
  globals_->http_auth_preferences->SetAllowedSchemes(
      base::flat_set<std::string>(kSupportedAuthSchemes.begin(),
                                  kSupportedAuthSchemes.end()));
}
```
