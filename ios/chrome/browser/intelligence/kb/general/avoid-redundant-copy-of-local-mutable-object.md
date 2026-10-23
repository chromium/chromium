---
name: ios/avoid-redundant-copy-of-local-mutable-object
description: When returning or assigning a locally constructed mutable Objective-C object or collection to an immutable destination, use it directly without calling [object copy]
rubric: readability
severity: nit
---

**Rule.** Do not send `[mutableObject copy]` to a locally allocated or
mutable-copied Objective-C object or collection (such as `NSMutableDictionary`,
`NSMutableArray`, `NSMutableSet`, `UNMutableNotificationContent`,
`NSMutableURLRequest`, or `NSMutableParagraphStyle`) before returning it or
assigning it to a field, property, or variable expecting its immutable base
type. When the mutable instance is created locally solely to assemble values and
is not retained or mutated after the assignment or return, pass, assign, or
return the instance directly.

**Why.** In Objective-C, mutable variants inherit from their immutable base
classes. Calling `[object copy]` on an unshared, locally created mutable
instance merely to freeze its runtime type allocates a redundant second object
on the heap and performs an unnecessary shallow copy. Using the mutable instance
directly safely satisfies immutable base class destinations without allocation
or copying overhead.

## Examples

Bad:

```cpp
// Redundant copy when returning mutable content as immutable base type:
- (UNNotificationContent*)content {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = l10n_util::GetNSString(IDS_TITLE);
  return [content copy];
}

// Redundant copy when assigning local mutable collection to struct field:
NSMutableDictionary<NSString*, NSString*>* headers =
    [latestHTTPHeaders mutableCopy];
[headers addEntriesFromDictionary:variationHeaders];
webParams.extra_headers = [headers copy];
```

Good:

```cpp
// Return local mutable instance directly:
- (UNNotificationContent*)content {
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = l10n_util::GetNSString(IDS_TITLE);
  return content;
}

// Assign local mutable collection directly:
NSMutableDictionary<NSString*, NSString*>* headers =
    [latestHTTPHeaders mutableCopy];
[headers addEntriesFromDictionary:variationHeaders];
webParams.extra_headers = headers;
```
