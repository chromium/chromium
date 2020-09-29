# No sources_assignment_filter

There is a [strong][0] [consensus][1] that the set_sources_assignment_filter
feature from GN is a mis-feature and should be removed. This requires that
Chromium's BUILD.gn file stop using the feature.

## Why convert

When set_sources_assignment_filter is called, it configures a list of patterns
that will be used to filter names every time a variable named "sources" is
assigned a value.

Historically, Chromium used to call this function in build/BUILDCONFIG.gn thus
causing the patterns to be applied to every BUILD.gn file in the project. This
had multiple drawbacks:

1.  the configuration of the list of patterns is located far from the point
    where they are applied and developer are usually confused when a file
    they add to a rule is not build due to those pattern

2.  the filtering is applied to every assignment to a variable named "sources"
    after interpreting the string as a relative filename, thus build breaks if
    one of the forbidden pattern is used in unexpected location (like naming
    the build directory out/linux, or having mac/ in path to SDK, ...)

3.  the filtering is applied to every assignment to a variable named "sources"
    in the whole project, thus it has significant negative impact on the
    performance of gn

Since September 2020, the filter is enabled only for the files that have not
yet been converted. Eventually, this will be removed.

## Conversion pattern

To convert a BUILD.gn file it is necessary to change the following:

```
  source_set("foo") {
    sources = [
      "foo.h",
      "foo_mac.mm",
      "foo_win.cc",
      "foo_linux.cc",
    ]
  }
```

to

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

Since the second pattern never assign a name that will be filtered out, then
it is compatible whether the set_sources_assignment_filter feature is used or
not.

Once conversion is done, remove the following lines from the top of the file
to avoid regressions:

```
import("//build/config/deprecated_default_sources_assignment_filter.gni")
sources_assignment_filter = deprecated_default_sources_assignment_filter
```


[0]: https://groups.google.com/a/chromium.org/d/topic/chromium-dev/hyLuCU6g2V4/discussion
[1]: https://groups.google.com/a/chromium.org/d/topic/gn-dev/oQcYStl_WkI/discussion
