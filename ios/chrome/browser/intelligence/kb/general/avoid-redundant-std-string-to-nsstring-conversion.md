---
name: ios/avoid-redundant-std-string-to-nsstring-conversion
description: 'Avoid redundant conversions between C++ strings and NSString: operate in C++, accept C++ string parameters, and convert tested strings once before branching'
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Language Choice
    quote: In `//ios/`, "Objective-C" always means Objective-C++ (`.mm`), which allows freely mixing Objective-C and C++. Plain Objective-C (`.m`) is not used outside `//ios/third_party/`.
  - path: ui/base/README.md
    heading: //ui/base
    quote: '[l10n](l10n): localization APIs used throughout all of chromium, especially the widely-used `l10n_util::GetString*` functions'
---

**Rule.** In Objective-C++ code, do not convert std::string or std::u16string to
NSString\* solely to check emptiness or perform formatting; check .empty()
directly, keep formatting in C++ (base::StrCat, std::format), and retrieve
localized strings directly via l10n_util::GetStringUTF8() (or GetStringUTF16())
rather than converting l10n_util::GetNSString() via base::SysNSStringToUTF8().
In internal helpers forwarding to C++ APIs (such as UMA metrics or
l10n_util::GetNSStringF), accept C++ string types (const std::string&,
std::string_view, const std::u16string&) rather than NSString\*, and pass C++
string literals ("" or "...") directly instead of converting NSString literals
via base::SysNSStringToUTF8(@"..."). Additionally, when comparing an NSString\*
against multiple C++ string constants across conditional branches (if/else if),
do not repeatedly convert constants via base::SysUTF8ToNSString in each branch;
convert the tested NSString\* to std::string once upfront before branching (or
define matching string constants) and compare in C++.

**Why.** Converting between C++ strings and NSString\* allocates heap objects,
creates autoreleased instances, and performs unnecessary character transcoding
passes (UTF-8/UTF-16). Operating directly on C++ strings, accepting C++ string
parameters in internal helpers, passing C++ literals directly, and converting a
tested NSString\* once upfront before conditional branches eliminates
boilerplate conversions, avoids repeated allocation overhead, and avoids
transcoding on non-matching branches.

## Examples

Bad:

```cpp
// Emptiness check, GetNSString conversion, or literals:
if ([base::SysUTF8ToNSString(location) length] == 0) return @"";
std::string text = base::SysNSStringToUTF8(l10n_util::GetNSString(IDS_CONFIRM));
auto guid = autofill::Suggestion::Guid(base::SysNSStringToUTF8(@""));

// Accepting NSString* only to convert it back for C++ APIs:
+ (void)logUma:(NSString*)name {
  base::UmaHistogramMemoryLargeMB(base::SysNSStringToUTF8(name), mb);
}

// Repeated conversions across conditional branches:
if ([action isEqualToString:base::SysUTF8ToNSString(spotlight::kActionOne)]) {
  HandleOne();
} else if ([action isEqualToString:base::SysUTF8ToNSString(spotlight::kActionTwo)]) {
  HandleTwo();
}
```

Good:

```cpp
// Check .empty() directly, use GetStringUTF8, or pass literals:
if (location.empty()) return @"";
std::string text = l10n_util::GetStringUTF8(IDS_CONFIRM);
auto guid = autofill::Suggestion::Guid("");

// Accept C++ string types directly in Objective-C++ helpers:
+ (void)logUma:(const std::string&)name {
  base::UmaHistogramMemoryLargeMB(name, mb);
}

// Convert once upfront and compare directly in C++:
const std::string action_str = base::SysNSStringToUTF8(action);
if (action_str == spotlight::kActionOne) {
  HandleOne();
} else if (action_str == spotlight::kActionTwo) {
  HandleTwo();
}
```
