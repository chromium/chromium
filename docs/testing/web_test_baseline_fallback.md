# Web Test Baseline Fallback


*** promo
Read [Web Test Expectations and Baselines](web_test_expectations.md) first
if you have not.
***

Baselines can vary by platforms, in which case we need to check in multiple
versions of a baseline. Meanwhile, we would like to avoid storing identical
baselines by allowing a platform to fall back to another. This document first
introduces how platform-specific baselines are structured and how we search for
a baseline (the fallback mechanism), and then goes into the details of baseline
optimization and rebaselining.

[TOC]

## Terminology

* **Root directory**:
    [`//src/third_party/blink/web_tests`](../../third_party/blink/web_tests)
    is the root directory (of all the web tests and baselines). All relative
    paths in this document start from this directory.
* **Test name**: the name of a test is its relative path from the root
    directory (e.g. `html/dom/foo/bar.html`).
* **Baseline name**: replacing the extension of a test name with
    `-expected.{txt,png,wav}` gives the corresponding baseline name.
* **Virtual tests**: tests can have virtual variants. For example,
    `virtual/gpu/html/dom/foo/bar.html` is the virtual variant of
    `html/dom/foo/bar.html` in the `gpu` suite. Only the latter file exists on
    disk, and is called the base of the virtual test. See
    [Web Tests#Testing Runtime Flags](web_tests.md#testing-runtime-flags)
    for more details.
* **Platform directory**: each directory under
    [`platform/`](../../third_party/blink/web_tests/platform) is a platform
    directory that contains baselines (no tests) for that platform. Directory
    names are in the form of `PLATFORM-VERSION` (e.g. `mac-mac10.12`), except
    for the latest version of a platform which is just `PLATFORM` (e.g. `mac`).

## Baseline fallback

Each platform has a pre-configured fallback when a baseline cannot be found in
this platform directory. A general rule is to have older versions of an OS
falling back to newer versions. Besides, Android falls back to Linux, which then
falls back to Windows. Eventually, all platforms fall back to the root directory
(i.e. the generic baselines that live alongside tests). The rules are configured
by `FALLBACK_PATHS` in each Port class in
[`//src/third_party/blink/tools/blinkpy/web_tests/port`](../../third_party/blink/tools/blinkpy/web_tests/port).

All platforms can be organized into a tree based on their fallback relations (we
are not considering virtual test suites yet). See the lower half (the
non-virtual subtree) of this
[graph](https://docs.google.com/drawings/d/13l3IUlSE99RoKjDwEWuY1O77simAhhF6Wi0fZdkSaMA/).
Walking from a platform to the root gives the **search path** of that platform.
We check each directory on the search path in order and see if "directory +
baseline name" points to a file on disk (note that baseline names are relative
paths), and stop at the first one found.

### Virtual test suites

Now we add virtual test suites to the picture, using a test named
`virtual/gpu/html/dom/foo/bar.html` as an example to demonstrate the process.
The baseline search process for a virtual test consists of two passes:

1. Treat the virtual test name as a regular test name and search for the
   corresponding baseline name using the same search path, which means we are in
   fact searching in directories like `platform/*/virtual/gpu/...`, and
   eventually `virtual/gpu/...` (a.k.a. the virtual root).
2. If no baseline can be found so far, we retry with the non-virtual (base) test
   name `html/dom/foo/bar.html` and walk the search path again.

The [graph](https://docs.google.com/drawings/d/13l3IUlSE99RoKjDwEWuY1O77simAhhF6Wi0fZdkSaMA/)
visualizes the full picture. Note that the two passes are in fact the same with
different test names, so the virtual subtree is a mirror of the non-virtual
subtree. The two trees are connected by the virtual root that has different
ancestors (fallbacks) depending on which platform we start from; this is the
result of the two-pass baseline search.

*** promo
__Note:__ there are in fact two more places to be searched before everything
else: additional directories given via command line arguments and flag-specific
baseline directories. They are maintained manually and are not discussed in this
document.
***

## Tooling implementation

This section describes the implications the fallback mechanism has on the
implementation details of tooling, namely `blink_tool.py`. If you are not
hacking `blinkpy`, you can stop here.

### Optimization

We can remove a baseline if it is the same as its fallback. An extreme example
is that if all platforms have the same result, we can just have a single generic
baseline. Here is the algorithm used by
[`blink_tool.py optimize-baselines`](../../third_party/blink/tools/blinkpy/common/checkout/baseline_optimizer.py)
to optimize the duplication away.

Notice from the previous section that the virtual and non-virtual parts are two
identically structured subtrees. Trees are easy to work with: we can simply
traverse the tree from leaves up to the root, and if there are two identical
baselines on two nodes on the path with no other nodes in between or all nodes
in between have no baselines, keep the one closer to the root (delete the
baseline on the node further from the root).

The virtual root is special because it has multiple parents. Yet if we can cut
the edges between the two subtrees (i.e. to make the virtual subtree
self-contained), we can apply the same algorithm to both of them. A subtree is
self-contained when it does not need to fallback to ancestors, which can be
guaranteed by placing a baseline on its root. If the virtual root already has a
baseline, we can simply ignore these edges without doing anything; otherwise, we
need to make sure all children of the virtual root have baselines by copying
the non-virtual fallbacks to the ones that do not (we cannot copy the generic
baseline to the virtual root because virtual platforms may have different
results).

In addition, the optimizer also removes redundant all-PASS testharness.js
results. Such baselines are redundant when there are no other fallbacks later
on the search path (including if the all-PASS baselines are at root), because
`run_web_tests.py` assumes all-PASS testharness.js results when baselines can
not be found for a platform.

### Rebaseline

The fallback mechanism also affects the rebaseline tool (`blink_tool.py
rebaseline{-cl}`). When asked to rebaseline a test on some platforms, the tool
downloads results from corresponding try bots and put them into the respective
platform directories. This is potentially problematic. Because of the fallback
mechanism, the new baselines may affect some other platforms that are not being
rebaselining but fall back to the rebaselined platforms.

The solution is to copy the current baselines from the to-be-rebaselined
platforms to the platforms that may otherwise be affected through fallback
resolution before downloading new baselines. This is handled internally by
`blink_tool.py rebaseline{,-cl}`.

Finally, `blink_tool.py rebaseline{-cl}` also does optimization in the end by
default.
