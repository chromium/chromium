# Using clang-format on Chromium C++ Code

## Easiest usage, from the command line

To automatically format a pending patch according to
[Chromium style](https://www.chromium.org/developers/coding-style), from
the command line, simply run: ``` git cl format ``` This should work on all
platforms _(yes, even Windows)_ without any set up or configuration: the tool
comes with your checkout. Like other `git-cl` commands, this operates on a diff
relative to the upstream branch. Only the lines that you've already touched in
your patch will be reformatted. You can commit your changes to your git branch
and then run `git cl format`, after which `git diff` will show you what
clang-format changed. Alternatively, you can run `git cl format` with your
changes uncommitted, and then commit your now-formatted code.

## Editor integrations

Many developers find it useful to integrate the clang-format tool with their
editor of choice. As a convenience, the scripts for this are also available in
your checkout of Chrome under
[src/buildtools/clang_format/script/](https://code.google.com/p/chromium/codesearch#chromium/src/buildtools/clang_format/script/).

If you use an editor integration, you should try to make sure that you're using
the version of clang-format that comes with your checkout. That way, you'll
automatically get updates and be running a tool that formats consistently with
other developers. The binary lives under `src/buildtools`, but it's also in your
path indirectly via a `depot_tools` launcher script:
[clang-format](https://code.google.com/p/chromium/codesearch#chromium/tools/depot_tools/clang-format)
([clang-format.bat](https://code.google.com/p/chromium/codesearch#chromium/tools/depot_tools/clang-format.bat) on Windows). Assuming that `depot_tools` is in your editor's `PATH`
and the editor command runs from a working directory inside the Chromium
checkout, the editor scripts (which anticipate clang-format on the path) should
work.

For further guidance on editor integration, see these specific pages:

*   [Sublime Text](https://www.chromium.org/developers/sublime-text#TOC-Format-selection-or-area-around-cursor-using-clang-format)
*   [llvm's guidelines for vim, emacs, and bbedit](http://clang.llvm.org/docs/ClangFormat.html)
*   For vim, `:so tools/vim/clang-format.vim` and then hit cmd-shift-i (mac)
    ctrl-shift-i (elsewhere) to indent the current line or current selection.

## Are robots taking over my freedom to choose where newlines go?

No. For the project as a whole, using clang-format is just one optional way to
format your code. While it will produce style-guide conformant code, other
formattings would also satisfy the style guide, and all are okay.

Having said that, many clang-format converts have found that relying on a tool
saves both them and their reviewers time. The saved time can then be used to
discover functional defects in their patch, to address style/readability
concerns whose resolution can't be automated, or to do something else that
matters.

In directories where most contributors have already adopted clang-format, and
code is already consistent with what clang-format would produce, some teams
intend to experiment with standardizing on clang-format. When these local
standards apply, it will be enforced by a PRESUBMIT.py check.

## Reporting problems

If clang-format is broken, or produces badly formatted code, please file a
[bug]. Assign it to thakis@chromium.org who will route it upstream.

[bug]:
https://code.google.com/p/chromium/issues/entry?comment=clang-format%20produced%20code%20that%20(choose%20all%20that%20apply):%20%0A-%20Doesn%27t%20match%20Chromium%20style%0A-%20Doesn%27t%20match%20blink%20style%0A-%20Riles%20my%20finely%20honed%20stylistic%20dander%0A-%20No%20sane%20human%20would%20ever%20choose%0A%0AHere%27s%20the%20code%20before%20formatting:%0A%0A%0AHere%27s%20the%20code%20after%20formatting:%0A%0A%0AHere%27s%20how%20it%20ought%20to%20look:%0A%0A%0ACode%20review%20link%20for%20full%20files/context:&summary=clang-format%20quality%20problem&cc=thakis@chromium.org&labels=Type-Bug,Build-Tools,OS-?
