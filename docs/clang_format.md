# Using clang-format on Chromium C++ Code

[TOC]

*** note
NOTE: This page does not apply to the Chromium OS project. See [Chromium Issue
878506](https://bugs.chromium.org/p/chromium/issues/detail?id=878506#c10)
for updates.
***

## Easiest usage, from the command line

To automatically format a pending patch according to
[Chromium style](/styleguide/c++/c++.md), run: `git cl format` from the command
line. This should work on all platforms without any extra set up: the tool is
integrated with depot_tools and the Chromium checkout.

Like other `git-cl` commands, this operates on a diff relative to the upstream
branch. Only the lines that changed in a CL will be reformatted. To see what
clang-format would choose, commit any local changes and then run `git cl
format` followed by `git diff`. Alternatively, run `git cl format` and commit
the now-formatted code.

## Editor integrations

Many developers find it useful to integrate the clang-format tool with their
editor of choice. As a convenience, the scripts for this are also available in
your checkout of Chrome under
[src/third_party/clang-format/script/](https://source.chromium.org/chromium/chromium/src/+/HEAD:third_party/clang-format/script/).

If you use an editor integration, you should try to make sure that you're using
the version of clang-format that comes with your checkout. That way, you'll
automatically get updates and be running a tool that formats consistently with
other developers. The binary lives under `src/buildtools`, but it's also in your
path indirectly via a `depot_tools` launcher script:
[clang-format](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/HEAD:clang-format)
([clang-format.bat](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/HEAD:clang-format.bat) on Windows). Assuming that `depot_tools` is in your editor's `PATH`
and the editor command runs from a working directory inside the Chromium
checkout, the editor scripts (which anticipate clang-format on the path) should
work.

For further guidance on editor integration, see these specific pages:

*   [Sublime Text](https://www.chromium.org/developers/sublime-text#TOC-Format-selection-or-area-around-cursor-using-clang-format)
*   [llvm's guidelines for vim, emacs, and bbedit](http://clang.llvm.org/docs/ClangFormat.html)
*   [Visual Studio Code](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/vscode.md#useful-extensions)
*   For vim, `:so tools/vim/clang-format.vim` and then hit cmd-shift-i (mac)
    ctrl-shift-i (elsewhere) to indent the current line or current selection.

## Reporting problems

If clang-format is broken, or produces badly formatted code, please file a
[bug]. Assign it to thakis@chromium.org or dcheng@chromium.org, who will route
it upstream.

[bug]:
https://code.google.com/p/chromium/issues/entry?comment=clang-format%20produced%20code%20that%20(choose%20all%20that%20apply):%20%0A-%20Doesn%27t%20match%20Chromium%20style%0A-%20Doesn%27t%20match%20blink%20style%0A-%20Riles%20my%20finely%20honed%20stylistic%20dander%0A-%20No%20sane%20human%20would%20ever%20choose%0A%0AHere%27s%20the%20code%20before%20formatting:%0A%0A%0AHere%27s%20the%20code%20after%20formatting:%0A%0A%0AHere%27s%20how%20it%20ought%20to%20look:%0A%0A%0ACode%20review%20link%20for%20full%20files/context:&summary=clang-format%20quality%20problem&cc=thakis@chromium.org&labels=Type-Bug,Build-Tools,OS-?,clang-format

## Are robots taking over my freedom to choose where newlines go?

Mostly. At upload time, a presubmit check warns if a CL is not clang-formatted,
but this is a non-blocking warning, and the CL may still be submitted. Even so,
try to prefer clang-format's output when possible:

- While clang-format does not necessarily format code the exact same way a human
  might choose, it produces style-conformat code by design. This can allow
  development and review time to be focused on discovering functional defects,
  addressing readability/understandability concerns that can't be automatically
  fixed by tooling, et cetera.
- Continually fighting the tooling is a losing battle. Most Chromium developers
  use clang-format. Large-scale changes will simply run `git cl format` once to
  avoid having to deal with the particulars of formatting. Over time, this will
  likely undo any carefully-curated manual formatting of the affected lines.

There is one notable exception where clang-format is often disabled: large
tables of data are often surrounded by `// clang-format off` and `//
clang-format on`. Try to use this option sparingly, as widespread usage makes
tool-assisted refactoring more difficult.

Again, if clang-format produces something odd, please err on the side of
[reporting an issue](#Reporting-problems): bugs that aren't reported can't be
fixed.
