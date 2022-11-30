# Chromium Desktop UI Debugging Tools and Tips

To help develop and debug Chromium desktop UI, this doc shares a set of
developer tools and their usages.


## UI Debugging Shortcuts

After enabling `ui-debug-tools` flag from `chrome://flags`, developers will be
able to use the following keyboard shortcuts:

| Actions             | Shortcuts |
|---------------------|:--------- |
| Ctrl+Alt+Shift+T    | Toggle between non-Tablet mode and Tablet mode |
| Ctrl+Alt+Shift+V    | Print out the current Views tree hierarchy     |
| Ctrl+Alt+Shift+M    | Print out all the views on the Views tree with their properties in details |


## UI DevTools

UI DevTools is a set of graphical inspection and debugging tools for Chromium
native UI. They largely resemble the Web DevTools used by Web developers.
There are two ways to enable the tools:
* Enable `ui-debug-tools` flag from `chrome://flags`
* Execute chromium/chrome with `--enable-ui-devtools` command line flag

Once being enabled, UI DevTools can be launched through a button on
`chrome://inspect#native-ui` page. Thus, UI developers are able to
examine the UI structure, individual view's layout and properties etc.

Detailed usage information and feature tutorials can be found in this [doc](https://chromium.googlesource.com/chromium/src/+/main/docs/ui/ui_devtools/index.md).



## Debugger Extensions/Scripts

For developers who prefer to use debuggers, some debugger extensions or
scripts can be helpful. Once being loaded into the debugger, they would provide
custom commands such as printing out a view's properties or the view hierarchy
information.


| Target Debugger       | Extension/Script Usage | Code Location|
|-----------------------|:-----------------------|--------------|
| Windbg                | [README][]             | [Source][]   |
| LLDB                  | TBA                    | TBA          |

[README]: https://chromium.googlesource.com/chromium/src/+/main/tools/win/chromeexts/README.md
[Source]: https://source.chromium.org/chromium/chromium/src/+/main:tools/win/chromeexts/


## Views examples

Ever wonder what common controls the Views framework provides? And how to
customize them?

`views_examples` and `views_examples_with_content` are two programs that list
those controls and show examples on how to use them. Just build and run the
following commands:

```shell
$ autoninja -C out/Default views_examples

$ out/Default/views_examples [--enable-examples=<example1,[example2...]>]
```

The list of all available examples can be found in
[README](https://chromium.googlesource.com/chromium/src/+/main/ui/views/examples/README.md).


## Note

### Adding Metadata and Properties

To be able to inspect the properties of a view through those tools, the
corresponding view needs to have metadata and properties built in it. This
requires adding some standard macros. A simple example can be found from this
[comment](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/view.h?q=%22Property%20metadata%22&ss=chromium%2Fchromium%2Fsrc).

More advanced usages of the macros and special property handlings are elaborated
in this [doc](https://chromium.googlesource.com/chromium/src/+/main/docs/ui/views/metadata_properties.md).

