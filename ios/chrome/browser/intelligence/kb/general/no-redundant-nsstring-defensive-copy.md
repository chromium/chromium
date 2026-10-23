---
name: ios/no-redundant-nsstring-defensive-copy
description: When receiving or inspecting NSString or immutable object parameters synchronously without storing them, avoid redundant defensive copies via copy, initWithString, or stringWithString
rubric: readability
severity: nit
---

**Rule.** Do not create defensive copies of `NSString*` or immutable object
parameters (such as via `[[NSString alloc] initWithString:]`,
`[NSString stringWithString:]`, or `[... copy]`) in synchronous functions or
methods where the object is only inspected or accessed locally and neither
stored nor retained across calls or asynchronous boundaries. Inspect and operate
on the passed parameter directly. When deriving substrings or transformed values
(for example, via `substringToIndex:`), reassign a local pointer variable or
consume the parameter directly rather than allocating a defensive copy.

**Why.** Defensively copying an `NSString` or immutable object parameter for
purely synchronous local execution introduces redundant heap allocations and
buffer duplication overhead, impacting performance in synchronous utilities and
processing loops. Because the function executes synchronously on the calling
thread and the reference does not escape or cross asynchronous boundaries,
concurrent mutation is impossible—even if the caller passes an `NSMutableString`
instance.

## Examples

Bad:

```cpp
// Variant 1: Redundant copy via `initWithString:` when modifying local pointer.
NSString* ExtractCreditCardNumber(NSString* string) {
  NSString* text = [[NSString alloc] initWithString:string];
  if ([text length] > 50) {
    text = [string substringToIndex:50];
  }
  return MatchCardNumberInText(text);
}

// Variant 2: Redundant copy via `stringWithString:` when inspecting/forwarding.
NSDateComponents* ExtractExpirationDateFromText(NSString* string) {
  NSString* text = [NSString stringWithString:string];
  if ([text length] > 50) {
    text = [text substringToIndex:50];
  }
  return ParseDateComponents(text);
}
```

Good:

```cpp
// Variant 1: Reassign parameter directly without copying.
NSString* ExtractCreditCardNumber(NSString* string) {
  if ([string length] > 50) {
    string = [string substringToIndex:50];
  }
  return MatchCardNumberInText(string);
}

// Variant 2: Inspect and forward parameter directly without copying.
NSDateComponents* ExtractExpirationDateFromText(NSString* string) {
  if ([string length] > 50) {
    string = [string substringToIndex:50];
  }
  return ParseDateComponents(string);
}
```
