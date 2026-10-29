---
name: blink/content_extraction/sanitize-dom-strings-for-mojom
description: >-
  Sanitize UTF-16 DOM strings for unpaired surrogates via
  ReplaceUnpairedSurrogates() or its helpers before assigning them to Mojom
  output fields.
rubric: correctness
severity: blocking
---

**Rule.** Sanitize every UTF-16 DOM string for unpaired surrogates before
assigning it to a Mojom output field. Whenever assigning a Blink `String` or
`AtomicString` sourced from the DOM or layout tree (such as `FastGetAttribute`,
`GetAttribute`, `textContent`, `GetInnerTextWithoutUpdate`, `TransformedText`,
`Value`, `title`, `AltText`, or `StringBuilder::ToString`) to a
`mojom::blink::AIPageContent*` field, pass it through
`ReplaceUnpairedSurrogates()` (or a helper that calls it, such as
`ConvertNodeTextToUtf8()` or `NormalizeLabelCandidate()`).

**Why.** Blink DOM strings are UTF-16 and may contain lone surrogates
(`0xD800`–`0xDFFF`) from arbitrary web page script or markup. When the browser
process converts Mojom strings into Protobuf `string` fields
(`page_content_proto_util.cc`), any unpaired surrogate produces ill-formed UTF-8
that causes Protobuf serialization/parsing to fail for the entire page payload.

## Examples

Bad:

```cpp
// BUG: Assigns raw UTF-16 DOM strings directly to Mojom fields; an unpaired
// surrogate in the attribute or text will break Protobuf UTF-8 serialization!
attributes.label = element.FastGetAttribute(html_names::kAriaLabelAttr);
form_control_data->placeholder = text_control.GetPlaceholderValue();
```

Good:

```cpp
// Sanitize all DOM strings before storing them on Mojom structs:
attributes.label = NormalizeLabelCandidate(
    element.FastGetAttribute(html_names::kAriaLabelAttr));
form_control_data->placeholder =
    ReplaceUnpairedSurrogates(text_control.GetPlaceholderValue());
```
