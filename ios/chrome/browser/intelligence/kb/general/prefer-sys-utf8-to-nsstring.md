---
name: ios/prefer-sys-utf8-to-nsstring
description: When converting between NSString* and C++ string types in Objective-C++, use base::Sys* helpers instead of stringWithUTF8String:, UTF8String, format strings, or manual buffer casting
rubric: readability
severity: should_fix
---

**Rule.** When converting between NSString\* and C++ string types (std::string,
std::u16string, or std::string_view) in Objective-C++, use Chromium's standard
conversion helpers from base/strings/sys_string_conversions.h
(base::SysUTF8ToNSString, base::SysUTF16ToNSString, base::SysNSStringToUTF8,
base::SysNSStringToUTF16). Do not bridge C++ strings to NSString\* using
[NSString stringWithUTF8String:], \[[NSString alloc] initWithUTF8String:\],
[NSString stringWithCString:...], or \[NSString stringWithFormat:@"%s",
str.c_str()\]. In the reverse direction, do not convert NSString\* to C++
strings by calling [string UTF8String], using -dataUsingEncoding:, or
hand-rolling raw byte pointer casts.

**Why.** Cocoa methods like [NSString stringWithUTF8String:] and format strings
with %s require null-terminated C strings, forcing unnecessary .c_str() calls
and risking null dereferences or out-of-bounds reads. Conversely, calling
[string UTF8String] returns null if the receiver is nil, leading to undefined
behavior or crashes when passed to std::string constructors. Hand-rolling
conversions via -dataUsingEncoding: or pointer casts is error-prone, risking
encoding mismatches, byte-length miscalculations, and memory leaks. The
base::Sys\* helpers accept std::string_view directly, handle nil inputs safely
(returning empty C++ strings), and provide consistent UTF-8/UTF-16 transcoding
across Apple platforms.

## Examples

Bad:

```cpp
// Converting C++ string to NSString via Cocoa C-string methods or %s:
NSString* language =
    [NSString stringWithUTF8String:configuration.language.value().c_str()];
NSString* name = [[NSString alloc] initWithUTF8String:user_name.c_str()];
NSString* errorMsg = [NSString stringWithFormat:@"%s", message.c_str()];

// Converting NSString to C++ string via UTF8String or hand-rolled NSData casts:
std::string query = [search_text UTF8String];
NSData* data = [javaScript dataUsingEncoding:NSUTF8StringEncoding];
const char16_t* utf16 = reinterpret_cast<const char16_t*>(data.bytes);
std::u16string script(utf16, data.length / sizeof(char16_t));
```

Good:

```cpp
// Converting C++ string to NSString:
NSString* language = base::SysUTF8ToNSString(configuration.language.value());
NSString* name = base::SysUTF8ToNSString(user_name);
NSString* errorMsg = base::SysUTF8ToNSString(message);

// Converting NSString to C++ string safely:
std::string query = base::SysNSStringToUTF8(search_text);
std::u16string script = base::SysNSStringToUTF16(javaScript);
std::string utf8_name = base::SysNSStringToUTF8(name);
```
