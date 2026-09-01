# Chrome's custom UI Automation properties

Chrome exposes custom [UI Automation (UIA)](uiautomation.md) properties when
the standard UIA properties do not provide information that assistive
technologies need. See
[UI Automation Custom Properties](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-regcustompropseventpatterns)
for more info.

## Properties

| Programmatic name | GUID | UIA type |
|---|---|---|
| `IsWebContentRoot` | `C5FDC049-4EEC-4299-87BB-CA9FC718A681` | Boolean |
| `UniqueId` | `CC7EEB32-4B62-4F4C-AFF6-1C2E5752AD8E` | String |
| `MathML` | `FA170AB3-3229-4E7C-827F-DD05EE0481D9` | String |
| `AccessibleActions` | `8C787AC3-0405-4C94-AC09-7A56A173F7EF` | Element array |

### IsWebContentRoot

`IsWebContentRoot` is available on all nodes. It is true only on the
top-level `Document` element that forms the boundary between a browser tab's
web content and the browser UI. It is false on every other node, including
documents for child frames.

Assistive technologies can use this property to contain document navigation
within web content without depending on browser window class names or tree
shapes, which are implementation details and can change.

### UniqueId

`UniqueId` is a string containing the negative accessibility node ID. It
matches the unique ID exposed for the same node through IAccessible2.

### MathML

`MathML` is available on math elements when UIA MathML support is enabled. It
contains the serialized MathML markup for the element. Its GUID matches the
custom property exposed by Microsoft Word for compatibility with assistive
technologies.

### AccessibleActions

`AccessibleActions` is available on nodes with an `aria-actions` attribute. It
contains the target elements named by the attribute. The custom property uses
`UIAutomationType_ElementArray`, which is supported for custom properties
starting with Windows 11.

## Looking up a property

See
[UI Automation Custom Properties](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-regcustompropseventpatterns)
for information about looking up custom properties.
