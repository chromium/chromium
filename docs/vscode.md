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
Open the file chromium/src/.vscode/settings.json and add the following settings.
Remember to replace `<full_path_to_your_home>`!

```
{
  // Default tab size of 2.
  "editor.tabSize": 2,
  // Do not figure out tab size from opening a file.
  "editor.detectIndentation": false,
  // Add a line at 80 characters.
  "editor.rulers": [80],
  // Optional: Highlight current line at the left of the editor.
  "editor.renderLineHighlight": "gutter",
  // Optional: Don't automatically add closing brackets. It gets in the way.
  "editor.autoClosingBrackets": false,
  // Optional: Enable a tiny 30k feet view of your doc.
  "editor.minimap.enabled": true,
  "editor.minimap.maxColumn": 80,
  "editor.minimap.renderCharacters": false,
  // Trim tailing whitespace on save.
  "files.trimTrailingWhitespace": true,
  // Optional: Do not open files in 'preview' mode. Opening a new file in can
  //           replace an existing one in preview mode, which can be confusing.
  "workbench.editor.enablePreview": false,
  // Optional: Same for files opened from quick open (Ctrl+P).
  "workbench.editor.enablePreviewFromQuickOpen": false,
  // Optional: Don't continuously fetch remote changes.
  "git.autofetch": false,

  "files.associations": {
    // Adds xml syntax highlighting for grd files.
    "*.grd" : "xml",
    // Optional: .gn and .gni are not JavaScript, but at least it gives some
    // approximate syntax highlighting. Ignore the linter warnings!
    "*.gni" : "javascript",
    "*.gn" : "javascript"
  },

  "files.exclude": {
    // Ignore build output folders.
    "out*/**": true
  },

  "files.watcherExclude": {
    // Don't watch out*/ and third_party/ for changes to fix an issue
    // where vscode doesn't notice that files have changed.
    // https://github.com/Microsoft/vscode/issues/3998
    // There is currently another issue that requires a leading **/ for
    // watcherExlude. Beware that this pattern might affect other out* folders
    // like src/cc/output/.
    "**/out*/**": true,
    "**/third_party/**": true
  },

  // Wider author column for annotator extension.
  "annotator.annotationColumnWidth": "24em",

  // C++ clang format settings.
  "C_Cpp.clang_format_path": "${workspaceRoot}/third_party/depot_tools/clang-format",
  "C_Cpp.clang_format_sortIncludes": true,
  "C_Cpp.clang_format_formatOnSave": true,

  // YouCompleteMe
  "ycmd.path": "<full_path_to_your_home>/.ycmd",
  "ycmd.global_extra_config": "${workspaceRoot}/tools/vim/chromium.ycm_extra_conf.py",
  "ycmd.confirm_extra_conf": false,
}
```

### Tasks
Next, we'll tell VS Code how to compile our code and how to read warnings and
errors from the build output. Copy the code below to
chromium/src/.vscode/tasks.json. This will provide 5 tasks to do basic things.
You might have to adjust the commands to your situation and needs.

```
{
  "version": "0.1.0",
  "runner": "terminal",
  "showOutput": "always",
  "echoCommand": true,
  "tasks": [
  {
    "taskName": "1-build_chrome_debug",
    "command": "ninja -C out/Debug -j 2000 chrome",
    "isShellCommand": true,
    "isTestCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  },
  {
    "taskName": "2-build_chrome_release",
    "command": "ninja -C out/Release -j 2000 chrome",
    "isShellCommand": true,
    "isBuildCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  },
  {
    "taskName": "3-build_all_debug",
    "command": "ninja -C out/Debug -j 2000",
    "isShellCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  },
  {
    "taskName": "4-build_all_release",
    "command": "ninja -C out/Release -j 2000",
    "isShellCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  },
  {
    "taskName": "5-build_test_debug",
    "command": "ninja -C out/Debug -j 2000 unit_tests components_unittests browser_tests",
    "isShellCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  },
  {
    "taskName": "6-build_current_file",
    "command": "compile_single_file --build-dir=out/Debug --file-path=${file}",
    "isShellCommand": true,
    "problemMatcher": [
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*):(\\d+):(\\d+):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
      }
    },
    {
      "owner": "cpp",
      "fileLocation": ["relative", "${workspaceRoot}"],
      "pattern": {
        "regexp": "^../../(.*?):(.*):\\s+(warning|\\w*\\s?error):\\s+(.*)$",
        "file": 1, "severity": 3, "message": 4
      }
    }]
  }]
}
```

### Launch Commands
Launch commands are the equivalent of `F5` in Visual Studio: They launch some
program or a debugger. Optionally, they can run some task defined in
`tasks.json`. Launch commands can be run from the debug view (`Ctrl+Shift+D`).
Copy the code below to `//.vscode/launch.json` and adjust them to your situation
and needs.
```
{
  "version": "0.2.0",
  "configurations": [
  {
    "name": "Chrome Debug",
    "type": "cppdbg",
    "request": "launch",
    "targetArchitecture": "x64",
    "program": "${workspaceRoot}/out/Debug/chrome",
    "args": [],  // Optional command line args
    "preLaunchTask": "1-build_chrome_debug",
    "stopAtEntry": false,
    "cwd": "${workspaceRoot}/out/Debug/",
    "environment": [],
    "externalConsole": true
  },
  {
    "name": "Chrome Release",
    "type": "cppdbg",
    "request": "launch",
    "targetArchitecture": "x64",
    "program": "${workspaceRoot}/out/Release/chrome",
    "args": [],  // Optional command line args
    "preLaunchTask": "2-build_chrome_release",
    "stopAtEntry": false,
    "cwd": "${workspaceRoot}/out/Release/",
    "environment": [],
    "externalConsole": true
  },
  {
    "name": "Custom Test Debug",
    "type": "cppdbg",
    "request": "launch",
    "targetArchitecture": "x64",
    "program": "${workspaceRoot}/out/Debug/unit_tests",
    "args": ["--gtest_filter=*",
              "--single_process",
              "--ui-test-action-max-timeout=1000000",
              "--test-launcher-timeout=1000000"],
    "preLaunchTask": "5-build_test_debug",
    "stopAtEntry": false,
    "cwd": "${workspaceRoot}/out/Debug/",
    "environment": [],
    "externalConsole": true
  },
  {
    "name": "Attach Debug",
    "type": "cppdbg",
    "request": "launch",
    "targetArchitecture": "x64",
    "program": "${workspaceRoot}/out/Debug/chrome",
    "args": ["--remote-debugging-port=2224"],
    "stopAtEntry": false,
    "cwd": "${workspaceRoot}/out/Debug/",
    "environment": [],
    "externalConsole": false
  },
  {
    // Must be running before launching: out/Debug/bin/chrome_public_apk gdb --ide
    "name": "Attach Android",
    "type": "cppdbg",
    "request": "launch",
    "targetArchitecture": "arm",
    "program": "/tmp/adb-gdb-support-${env:USER}/app_process",
    "miDebuggerPath": "/tmp/adb-gdb-support-${env:USER}/gdb",
    "miDebuggerServerAddress": "ignored",
    "cwd": "${workspaceRoot}",
    "customLaunchSetupCommands": [{
      "text": "-interpreter-exec console \"source -v /tmp/adb-gdb-support-${env:USER}/gdbinit\""
    }],
    "launchCompleteCommand": "None",
  }]
}
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

Here are some key bindings that are likely to be useful for you:

```
// Place your key bindings in this file to overwrite the defaults
[
// Run the task marked as "isTestCommand": true, see tasks.json.
{ "key": "ctrl+shift+t",       "command": "workbench.action.tasks.test" },
// Jump to the previous change in the built-in diff tool.
{ "key": "ctrl+up",            "command": "workbench.action.compareEditor.previousChange" },
// Jump to the next change in the built-in diff tool.
{ "key": "ctrl+down",          "command": "workbench.action.compareEditor.nextChange" },
// Jump to previous location in the editor (useful to get back from viewing a symbol definition).
{ "key": "alt+left",           "command": "workbench.action.navigateBack" },
// Jump to next location in the editor.
{ "key": "alt+right",          "command": "workbench.action.navigateForward" },
// Get a blame view of the current file. Requires the annotator extension.
{ "key": "ctrl+alt+a",         "command": "annotator.annotate" },
// Toggle header/source with the Toggle Header/Source extension (overrides the
// key binding from the C/C++ extension as I found it to be slow).
{ "key": "alt+o",              "command": "togglehs.toggleHS" },
// Quickly run a task, see tasks.json. Since we named them 1-, 2- etc., it is
// suffucient to press the corresponding number.
{ "key": "ctrl+r",             "command": "workbench.action.tasks.runTask",
                                  "when": "!inDebugMode" },
// The following keybindings are useful on laptops with small keyboards such as
// Chromebooks that don't provide all keys.
{ "key": "shift+alt+down",     "command": "cursorColumnSelectDown",
                                  "when": "editorTextFocus" },
{ "key": "shift+alt+left",     "command": "cursorColumnSelectLeft",
                                  "when": "editorTextFocus" },
{ "key": "shift+alt+pagedown", "command": "cursorColumnSelectPageDown",
                                  "when": "editorTextFocus" },
{ "key": "shift+alt+pageup",   "command": "cursorColumnSelectPageUp",
                                  "when": "editorTextFocus" },
{ "key": "shift+alt+right",    "command": "cursorColumnSelectRight",
                                  "when": "editorTextFocus" },
{ "key": "shift+alt+up",       "command": "cursorColumnSelectUp",
                                  "when": "editorTextFocus" },
{ "key": "alt+down",           "command": "scrollPageDown",
                                  "when": "editorTextFocus" },
{ "key": "alt+up",             "command": "scrollPageUp",
                                  "when": "editorTextFocus" },
{ "key": "alt+backspace",      "command": "deleteRight",
                                  "when": "editorTextFocus && !editorReadonly" },
{ "key": "ctrl+right",         "command": "cursorEnd",
                                  "when": "editorTextFocus" },
{ "key": "ctrl+shift+right",   "command": "cursorEndSelect",
                                  "when": "editorTextFocus" },
{ "key": "ctrl+left",          "command": "cursorHome",
                                  "when": "editorTextFocus" },
{ "key": "ctrl+shift+left",    "command": "cursorHomeSelect",
                                  "when": "editorTextFocus" },
]
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
"C_Cpp.addWorkspaceRootToIncludePath": false
```

### Unable to open $File resource is not available when debugging Chromium on Linux
Chromium [recently changed](https://docs.google.com/document/d/1OX4jY_bOCeNK7PNjVRuBQE9s6BQKS8XRNWGK8FEyh-E/edit?usp=sharing)
the file path to be relative to the output dir. Check
`gn args out/$dir --list` if `strip_absolute_paths_from_debug_symbols` is true (which is the default),
set `cwd` to the output dir. otherwise, set `cwd` to `${workspaceRoot}`.

### More
More tips and tricks can be found
[here](https://github.com/Microsoft/vscode-tips-and-tricks/blob/master/README.md).
