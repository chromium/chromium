# Usage of tools/lldb/lldbinit.py

Usage of Chromium's [lldbinit.py](../tools/lldb/lldbinit.py) is recommended when
debugging with lldb. This is necessary for source-level debugging when
`strip_absolute_paths_from_debug_symbols` is enabled [this is the default].

To use, add the following to your `~/.lldbinit`

```
# So that lldbinit.py takes precedence.
script sys.path[:0] = ['<.../path/to/chromium/src/tools/lldb>']
script import lldbinit
```
