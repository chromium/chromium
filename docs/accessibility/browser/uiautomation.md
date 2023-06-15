# UI Automation

[UI Automation (UIA)](https://docs.microsoft.com/en-us/windows/win32/winauto/entry-uiauto-win32)
is the modern accessibility API on Windows. The Chromium UIA provider is
currently under development. It can be enabled via the
`--enable-features=UiaProvider` browser command line switch.

## Key Features

### Clients and Providers

UI Automation exposes two different sets of interfaces. One is intended for
clients such as assistive technologies and automation frameworks. The other is
intended for providers such as UI widget frameworks and applications that render
their own content. Chromium implements the UI Automation provider APIs.

Clients and providers do not talk directly to one another. Instead, the
operating system gathers data from providers to present a unified tree view
across all open applications to the client.

Further reading:

* [UI Automation Provider Programmer's Guide](https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-providerportal)
* [UI Automation Client Programmer's Guide](https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-clientportal)

### Views of the UI Automation tree

Clients have the ability to filter the UI Automation tree to various subsets of
nodes. Tools such as Windows Narrator take advantage of this capability to skip
over structural details that might be interesting to an automation framework but
aren't relevant to a screen reader.

Providers can set two properties on a node to determine which views it appears
in: IsControlElement and IsContentElement. Getting the value of these properties
right is critical to ensuring assistive technologies can get the information
they need. Despite the names, there are many cases where nodes that might not be
considered a "control" should appear in the Control view of the tree. A good
litmus test is: if there's a reason a screen reader might be interested in a
node, it should appear in the control view.

Further reading:

* [UI Automation Tree Overview](https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-treeoverview)

### TextPattern

In addition to the tree view, UI Automation exposes a linear reading view
through an API known as TextPattern. This API allows a screen reader to navigate
text in more natural ways - characters, words, sentences, paragraphs, pages -
without worrying about the underlying tree structure. Windows Narrator relies
heavily on TextPattern for reading.

Further reading:

* [Text and TextRange Control Patterns](https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingtextandtextrange)
