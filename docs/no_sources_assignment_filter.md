# No sources_assignment_filter

There is a [strong][0] [consensus][1] that the set_sources_assignment_filter
feature from GN is a mis-feature and should be removed. This requires that
Chromium's BUILD.gn file stop using the feature.

Since October 2020, the filter is no longer used.

Chromium build does not set a default sources assignment filter, and all build
files must manage `sources` with explicit `if` statements.

## Explicit assignment

If you have a target that have platform specific implementation files, you can
use the following pattern:

```
  source_set("foo") {
    sources = [
      "foo.h",
    ]
    if (is_mac) {
      sources += [
        "foo_mac.mm",
      ]
    }
    if (is_win) {
      sources += [
        "foo_win.cc",
      ]
    }
    if (is_linux) {
      sources += [
        "foo_linux.cc",
      ]
    }
  }
```

[0]: https://groups.google.com/a/chromium.org/d/topic/chromium-dev/hyLuCU6g2V4/discussion
[1]: https://groups.google.com/a/chromium.org/d/topic/gn-dev/oQcYStl_WkI/discussion
