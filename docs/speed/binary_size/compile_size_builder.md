# `compile-size` Builder

[TOC]

## About

The `compile-size` trybot measures and prevents unexpected growth in the size of
the C++ compiler input size, which closely [correlates][Design Doc] with time
spent in the build.

## Overview

This builder does the following:
1. With the CL and without the CL applied:
   1. Builds `chrome` on Linux
   2. Queries siso for build deps and commands
   3. Generates a compile size report broken down by TU (translation unit)
2. Compares the two reports and writes a diff report with a summary and TU
   deltas
3. If a size increase threshold is exceeded and there is no override then fail
   the build

## What to do if the check fails?

1. [Inspect the report](#inspect-the-report)
2. Iterate on a fix
   1. Run [locally](#run-a-local-compile_size-check) to collect a baseline (or
   skip this to iterate remotely)
   2. If applicable, remove transitive includes by using [forward declarations](
      #forward-declarations)
   3. If stuck, ask for [help](#get-help)
      1. If still stuck, [override](#skip-the-check) the builder
3. Confirm results reproduce [remotely](#run-remotely)

### Inspect the report
1. On the `compile-size` builder's overview or infra page look for the summary
   and the link to the `compile_size_deltas.txt` report.
![alt text](compile_size_steps.png "Screenshot of the builder overview page
with boxes highlighting the link to the diff report under 'Write size results' >
'compile_size_deltas.txt' file and the total delta summary under
'Trybot Results'.")
2. The report shows translation units with the largest increases. These can
  give clues about the source of the increase. For example, when many TUs
  show a similar increase in size, it could mean that a header was added to
  be transitively included in all of those TUs.
![alt text](compile_size_diff_report.png "Screenshot of the compile size diff
report showing the same summary from the first screenshot on the top with the
list of translation units with the largest deltas below it.")

### Run a local compile-size check

If you want to check TU sizes locally to iterate faster, do the following:

```bash
# Set up a new build directory with the builder's GN args
tools/mb/mb gen -m tryserver.chromium.linux -b compile-size out/compile_size

# Build the `chrome` target
autoninja -C out/compile_size chrome

# Collect the size info
siso query commands -C out/compile_size chrome > /tmp/commands.txt
siso query deps -C out/compile_size > /tmp/deps.txt
tools/clang/scripts/compiler_inputs_size.py out/compile_size \
  /tmp/commands.txt /tmp/deps.txt > /tmp/report.txt
```

The report will contain lines with compile sizes in bytes per TU, like
```
apps/app_lifetime_monitor.cc 9,034,754
apps/app_lifetime_monitor_factory.cc 5,863,660
apps/app_restore_service.cc 9,198,130
```

As you make changes, you can re-build, collect size info, and generate reports
to see how the per-TU numbers change. Two reports can be diffed with
[compiler_inputs_size_diff.py].
```
tools/clang/scripts/compiler_inputs_size_diff.py /tmp/report-old.txt \
  /tmp/report-new.txt > /tmp/diff-report.txt
```

### Run remotely

Once done iterating on a fix locally (or if you skipped local iterations), you
can upload a new patchset or CL and trigger the `compile-size` builder to get
a new result and report.

### How to reduce your compiler input size

#### Forward Declarations

A common source of compiler input size growth is adding a header to another
widely included header. This can result in the added header getting compiled
an additional thousands or tens of thousands of times.

Chromium style prefers [forward declarations](
/styleguide/c++/c++.md#forward-declarations-vs_includes) over includes when a
full definition isn't required. For advice on when to use forward declarations
and how to minimize code in headers see [these tips](
/styleguide/c++/c++-dos-and-donts.md#minimize-code-in-headers).

Example CLs replacing header inclusion with forward declarations resulting in
large compiler input size decreases:
* [crrev.com/c/5213226](https://crrev.com/c/5213226)
* [crrev.com/c/6170321](https://crrev.com/c/6170321)
* [crrev.com/c/6073644](https://crrev.com/c/6073644)

### Get help

For help, post to build@chromium.org. They're expert build system developers.
You may also ask [#cxx](https://chromium.slack.com/archives/CGF4Y2J4W) or
[#halp](https://chromium.slack.com/archives/CGGPN5GDT) on Chromium slack.

### Skip the check

Not all checks are perfect and sometimes you want to bypass the builder (for
example, if you did your best and are unable to reduce compiler input size any
further).

Adding a `Compile-Size: $ANY_TEXT_HERE` footer to your CL description (above or
below `Bug: `)  will bypass the builder assertions.

Examples:

- `Compile-Size: Size increase is unavoidable.`
- `Compile-Size: Increase is temporary.`
- `Compile-Size: See commit description.` <-- use this if longer
than one line.

***note
**Note:** Make sure there are no blank lines between `Compile-Size:` and
other footers.
***

## Builder Implementation

The analysis is done with the following scripts:
* [compiler_inputs_size.py]: Computes growth between builds,
 breaking them down by TU.
* [compiler_inputs_size_diff.py]: Computes deltas from output of the
  compiler_inputs_size.py script.

The GN args are in the builder's [gn-args.json] file.

## References

* [Design Doc]
* [Builder recipe](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/compile_size_trybot.py)
* [recipe module](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/binary_size/api.py)

[compiler_inputs_size.py]: /tools/clang/scripts/compiler_inputs_size.py
[compiler_inputs_size_diff.py]: /tools/clang/scripts/compiler_inputs_size_diff.py
[Design Doc]: https://docs.google.com/document/d/1mb4XadoSqaqNZivcKpvMVfXZVXR41uP0wPC79GtDYr8/edit
[gn-args.json]: /infra/config/generated/builders/try/compile-size/gn-args.json