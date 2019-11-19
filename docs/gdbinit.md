# Usage of tools/gdb/gdbinit

Usage of Chromium's [gdbinit](../tools/gdb/gdbinit) is recommended when
debugging with gdb on any platform. This is necessary for source-level debugging
when `strip_absolute_paths_from_debug_symbols` or `use_custom_libcxx` are
enabled (they're enabled by default), and also adds extra features like
pretty-printing of custom Chromium types.

To use, add the following to your `~/.gdbinit`

```
source /path/to/chromium/src/tools/gdb/gdbinit
```
