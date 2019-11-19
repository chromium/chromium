# Visual Studio Code Dev

Visual Studio Code is a free, lightweight and powerful code editor for Windows,
Mac and Linux, based on Electron/Chromium. It has built-in support for
JavaScript, TypeScript and Node.js and a rich extension ecosystem that adds
intellisense, debugging, syntax highlighting etc. for many languages (C++,
Python, Go). It works without too much setup. Get started
[here](https://code.visualstudio.com/docs).

It is NOT a full-fledged IDE like Visual Studio. The two are completely
separate products. The only commonality with Visual Studio is that both are
from Microsoft.

Here's what works well:

*   Editing code works well especially when you get used to the [keyboard
    shortcuts](https://code.visualstudio.com/docs/customization/keybindings).
    VS Code is very responsive and can handle even big code bases like Chromium.
*   Git integration is a blast. Built-in side-by-side view, local commit and
    even extensions for
    [history](https://marketplace.visualstudio.com/items?itemName=donjayamanne.githistory)
    and
    [blame view](https://marketplace.visualstudio.com/items?itemName=ryu1kn.annotator).
*   [Debugging](https://code.visualstudio.com/Docs/editor/debugging) works
    well, even though startup times can be fairly high (~40 seconds with
    gdb on Linux, much lower on Windows). You can step through code, inspect
    variables, view call stacks for multiple threads etc.
*   Opening files and searching solution-wide works well now after having
    problems in earlier versions.
*   Building works well. Build tools are easy to integrate. Warnings and errors
    are displayed on a separate page and you can click to jump to the
    corresponding line of code.

[TOC]

## Updating This Page

Please keep this doc up-to-date. VS Code is still in active development and
subject to changes. This doc is checked into the Chromium git repo, so if you
make changes, read the [documentation
guidelines](https://chromium.googlesource.com/chromium/src/+/master/docs/documentation_guidelines.md)
and [submit a change list](https://www.chromium.org/developers/contributing-code).

All file paths and commands have been tested on Linux. Windows and Mac might
require a slightly different setup (e.g. `Ctrl` -> `Cmd`). Please update this
page accordingly.

## Setup

### Installation

Follow the steps on https://code.visualstudio.com/docs/setup/setup-overview. To
run it on Linux, just navigate to `chromium/src` folder and type `code .` in a
terminal. The argument to `code` is the base directory of the workspace. VS
Code does not require project or solution files. However, it does store
workspace settings in a `.vscode` folder in your base directory.

### Fixes for Known Issues

#### Git on Windows

If you only have the `depot_tools` Git installed on your machine, even though it
is in your PATH, VS Code will ignore it as it seems to be looking for `git.exe`.
You will have to add the following to your settings in order for the Git
integration to work:

```json
{
  "git.path": "C:\\src\\depot_tools\\git.bat"
}
```

#### Rendering of underscore on Linux

As mentioned in [#35901](https://github.com/Microsoft/vscode/issues/35901), VS
Code will not show underscore (`_`) properly on Linux by default. You can work
around this issue by forcing another font such as the default `monospace` or
changing the font size in your settings:

```json
{
  // If you want to use the default "monospace" font:
  //"terminal.integrated.fontFamily": "monospace"
  // If you would rather just increase the size of the font:
  //"terminal.integrated.fontSize": 15
  // If you would rather decrease the size of the font:
  //"terminal.integrated.fontSize": 13
}
```

### Useful Extensions

Up to now, you have a basic version of VS Code without much language support.
Next, we will install some useful extensions. Jump to the extensions window
(`Ctrl+Shift+X`) and install these extensions, you will most likely use them
every day:

*   ***C/C++*** -
    Code formatting, debugging, Intellisense.
*   ***Python*** -
    Linting, intellisense, code formatting, refactoring, debugging, snippets.
*   ***Toggle Header/Source*** -
    Toggles between .cc and .h with `F4`. The C/C++ extension supports this as
    well through `Alt+O` but sometimes chooses the wrong file when there are
    multiple files in the workspace that have the same name.
*   ***Protobuf support*** -
    Syntax highlighting for .proto files.
*   ***you-complete-me*** -
    YouCompleteMe code completion for VS Code. It works fairly well in Chromium.
*   ***Rewrap*** -
    Wrap lines at 80 characters with `Alt+Q`.

To install You-Complete-Me, enter these commands in a terminal:

```
$ git clone https://github.com/Valloric/ycmd.git ~/.ycmd
$ cd ~/.ycmd
$ git submodule update --init --recursive
$ ./build.py --clang-completer
```
If it fails with "Your C++ compiler does NOT fully support C++11." but you know
you have a good compiler, hack cpp/CMakeLists.txt to set CPP11_AVAILABLE true.

On Mac, replace the last command above with the following.

```
$ ./build.py --clang-completer --system-libclang
```

The following extensions might be useful for you as well:

*   ***Annotator*** -
    Git blame view.
*   ***Git History (git log)*** -
    Git history view.
*   ***chromium-codesearch*** -
    Code search (CS) integration, see [Chromium Code
    Search](https://cs.chromium.org/), in particular *open current line in CS*,
    *show references* and *go to definition*. Very useful for existing code. By
    design, won't work for code not checked in yet. Overrides default C/C++
    functionality. Had some issues last time I tried (extensions stopped
    working), so use with care.
*   ***change-case*** -
    Quickly change the case of the current selection or current word.
*   ***Instant Markdown*** -
    Instant markdown (.md) preview in your browser as you type. This document
    was written with this extension!
*   ***Clang-Format*** -
    Format your code using clang-format. The C/C++ extension already supports
    format-on-save (see `C_Cpp.clang_format_formatOnSave` setting). This
    extension adds the ability to format a document or the current selection on
    demand.
*   ***vscode-clangd*** -
    If you do not plan to use VSCode for debugging, vscode-clangd is a great
    alternative to C/C++ IntelliSense. It knows about how to compile Chromium,
    enabling it to provide smarter autocomplete than C/C++ IntelliSense as well
    as allowing you to jump from functions to their definitions. See
    [clangd.md](clangd.md) for details.

    If you need to debug, disable the vscode-clangd extension, enable C/C++
    Intellisense, and restart VSCode.


Also be sure to take a look at the
[VS Code marketplace](https://marketplace.visualstudio.com/VSCode) to check out other
useful extensions.

### Color Scheme
Press `Ctrl+Shift+P, color, Enter` to pick a color scheme for the editor. There
are also tons of [color schemes available for download on the
marketplace](https://marketplace.visualstudio.com/search?target=VSCode&category=Themes&sortBy=Downloads).

### Usage Tips
*   `Ctrl+P` opens a search box to find and open a file.
*   `F1` or `Ctrl+Shift+P` opens a search box to find a command (e.g. Tasks: Run
    Task).
*   `Ctrl+K, Ctrl+S` opens the key bindings editor.
*   ``Ctrl+` `` toggles the built-in terminal.
*   `Ctrl+Shift+M` toggles the problems view (linter warnings, compile errors
    and warnings). You'll swicth a lot between terminal and problem view during
    compilation.
*   `Alt+O` switches between the source/header file.
*   `Ctrl+G` jumps to a line.
*   `F12` jumps to the definition of the symbol at the cursor (also available on
    right-click context menu).
*   `Shift+F12` or `F1, CodeSearchReferences, Return` shows all references of
    the symbol at the cursor.
*   `F1, CodeSearchOpen, Return` opens the current file in Code Search.
*   `Ctrl+D` selects the word at the cursor. Pressing it multiple times
    multi-selects the next occurrences, so typing in one types in all of them,
    and `Ctrl+U` deselects the last occurrence.
*   `Ctrl+K, Z` enters Zen Mode, a fullscreen editing mode with nothing but the
    current editor visible.
*   `Ctrl+X` without anything selected cuts the current line. `Ctrl+V` pastes
    the line.

## Setup For Chromium

VS Code is configured via JSON files. This paragraph contains JSON configuration
files that are useful for Chromium development, in particular. See [VS Code
documentation](https://code.visualstudio.com/docs/customization/overview) for an
introduction to VS Code customization.

### Workspace Settings
Open the file [//tools/vscode/settings.json5](/tools/vscode/settings.json5),
and check out the default settings there. Feel free to commit added or removed
settings to enable better team development, or change settings locally to suit
personal preference. Remember to replace `<full_path_to_your_home>`! To use
these settings wholesale, enter the following commands into your terminal while
at the src directory:
```
$ mkdir .vscode/
$ cp tools/vscode/settings.json5 .vscode/settings.json
```

### Tasks
Next, we'll tell VS Code how to compile our code and how to read warnings and
errors from the build output. Open the file
[//tools/vscode/tasks.json5](/tools/vscode/tasks.json5). This will provide 5
tasks to do basic things. You might have to adjust the commands to your
situation and needs. To use these settings wholesale, enter the following
command into your terminal:
```
$ cp tools/vscode/tasks.json5 .vscode/tasks.json
```

### Launch Commands
Launch commands are the equivalent of `F5` in Visual Studio: They launch some
program or a debugger. Optionally, they can run some task defined in
`tasks.json`. Launch commands can be run from the debug view (`Ctrl+Shift+D`).
Open the file at [//tools/vscode/launch.json5](/tools/vscode/launch.json5) and
adjust the example launch commands to your situation and needs. To use these
settings wholesale, enter the following command into your terminal:
```
$ cp tools/vscode/launch.json5 .vscode/launch.json
```

### Key Bindings
To edit key bindings, press `Ctrl+K, Ctrl+S`. You'll see the defaults on the
left and your overrides on the right stored in the file `keybindings.json`. To
change a key binding, copy the corresponding key binding to the right. It's
fairly self-explanatory.

You can bind any command to a key, even commands specified by extensions like
`CodeSearchOpen`. For instance, to bind `CodeSearchOpen` to `F2` to , simply add
`{ "key": "F2", "command": "cs.open" },`.
Note that the command title `CodeSearchOpen` won't work. You have to get the
actual command name from the [package.json
file](https://github.com/chaopeng/vscode-chromium-codesearch/blob/master/package.json)
of the extension.

If you are used to other editors, you can also install your favorite keymap.
For instance, to install eclipse keymaps, install the
`vscode-eclipse-keybindings` extension. More keymaps can be found
[in the marketplace](https://marketplace.visualstudio.com/search?target=vscode&category=Keymaps).

Some key bindings that are likely to be useful for you are available at
[//tools/vscode/keybindings.json5](/tools/vscode/keybindings.json5). Please
take a look and adjust them to your situation and needs. To use these settings
wholesale, enter the following command into your terminal:
```
$ cp tools/vscode/keybindings.json5 .vscode/keybindings.json
```

### Snippets
There are some useful snippets provided in
[//tools/vscode/cpp.json5](/tools/vscode/cpp.json5).

You can either install them in your user profile (path may vary depending on the
platform):
```
$ cp tools/vscode/cpp.json5 ~/.config/Code/User/snippets/cpp.json
```

Or install them as project snippets after installing the [Project
Snippets](https://marketplace.visualstudio.com/items?itemName=rebornix.project-snippets)
extension:
```
$ cp tools/vscode/cpp.json5 .vscode/snippets/cpp.json
```

### Tips

#### The `out` folder
Automatically generated code is put into a subfolder of out/, which means that
these files are ignored by VS Code (see files.exclude above) and cannot be
opened e.g. from quick-open (`Ctrl+P`).
As of version 1.21, VS Code does not support negated glob commands, but you can
define a set of exclude pattern to include only out/Debug/gen:
```
"files.exclude": {
  // Ignore build output folders. Except out/Debug/gen/
  "out/[^D]*/": true,
  "out/Debug/[^g]*": true,
  "out/Debug/g[^e]*": true,
  "out_*/**": true,
},
```

Once it does, you can use
```
"!out/Debug/gen/**": true
```
in files.exclude instead of the symlink.

#### Using VS Code as git editor
Add `[core] editor = "code --wait"` to your `~/.gitconfig` file in order to use
VS Code as editor for git commit messages etc. Note that the editor starts up
significantly slower than nano or vim. To use VS Code as merge tool, add
`[merge] tool = code`.

#### Task Names
Note that we named the tasks `1-build_chrome_debug`, `2-build_chrome_release`
etc. This allows you to quickly execute tasks by pressing their number:
Press `Ctrl+P` and enter `task <n>`, where `<n>` is the number of the task. You
can also create a keyboard shortcut for running a task. `File > Preferences >
Keyboard Shortcuts` and add `{ "key": "ctrl+r", "command":
"workbench.action.tasks.runTask", "when": "!inDebugMode" }`. Then it's
sufficient to press `Ctrl+R` and enter `<n>`.

#### Working on Laptop
Because autocomplete is provided by the You-Complete-Me extension, consider
disabling C/C++ autocomplete and indexing to save battery. In addition, you
might want to disable git status autorefresh as well.

```
"git.autorefresh": false,
"C_Cpp.autocomplete": "Disabled",
```

### Unable to open $File resource is not available when debugging Chromium on Linux
Chromium [recently changed](https://docs.google.com/document/d/1OX4jY_bOCeNK7PNjVRuBQE9s6BQKS8XRNWGK8FEyh-E/edit?usp=sharing)
the file path to be relative to the output dir. Check
`gn args out/$dir --list` if `strip_absolute_paths_from_debug_symbols` is true (which is the default),
set `cwd` to the output dir. otherwise, set `cwd` to `${workspaceRoot}`.

### More
More tips and tricks can be found
[here](https://github.com/Microsoft/vscode-tips-and-tricks/blob/master/README.md).
