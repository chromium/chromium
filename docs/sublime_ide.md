# Using Sublime Text as your IDE

Sublime Text is a fast, powerful and easily extensible code editor. Check out
some [visual demos](http://www.sublimetext.com) for a quick demonstration.

You can download and install Sublime Text 3 from the [Sublime Text
Website](http://www.sublimetext.com/3). Assuming you have access to the right
repositories, you can also install Sublime via apt-get on Linux. Help and
general documentation is available in the [Sublime Text 3
Docs](http://www.sublimetext.com/docs/3/).

Sublime can be used on Linux, Windows and Mac as an IDE for developing Chromium.
Here's what works:

*   Editing code works well (especially if you're used to it and get used to the
    shortcuts).
*   Navigating around the code works well. There are multiple ways to do this (a
    full list of keyboard shortcuts is available for [Windows/Linux](http://docs.sublimetext.info/en/latest/reference/keyboard_shortcuts_win.html)
    and [Mac](http://docs.sublimetext.info/en/latest/reference/keyboard_shortcuts_osx.html)).
*   Building works fairly well and it does a decent job of parsing errors so
    that you can click and jump to the problem spot.

[TOC]

## Setup

### Configuring Sublime

All global configuration for Sublime (including installed packages) is stored in
`~/.config/sublime-text-3` (or `%APPDATA\Sublime Text 3` on Windows, or
`~/Library/Application Support/Sublime Text 3` on Mac). We will reference the
Linux folder for the rest of this tutorial, but replace with your own path if
using a different OS. If you ever want a clean install, just remove this folder.

**Warning**: If you have installed a license key for a paid version Sublime
Text, removing this folder will delete the license key, too.

Most of the packages you will install will be placed in `~/.config/sublime-
text-3/Packages/User`, where Sublime Text can detect them. You can also get to
this folder by selecting `Preferences > Browse Packages...` (or `Sublime Text >
Preferences > Browse Packages...` on Mac).

### A short word about paths

Certain packages require executables to be on your `PATH`, but Sublime gets the
`$PATH` variable from a login shell, not an interactive session (i.e. your path
needs to be set in `~/.bash_profile`, `~/.zprofile`, etc, not `~/.bashrc`,
`~/.zshrc`, etc). For more info, see
[Debugging Path Problems](http://sublimelinter.readthedocs.io/en/latest/troubleshooting.html#debugging-path-problems).

### Editing Preferences

Sublime configuration (including project files, key bindings, etc) is done via
JSON files. All configurations have a Default config (usually provided with the
program or package to document the available commands) and a User config
(overrides the default; this is where your overrides go). For example, select
`Preferences > Settings - Default` to see all the available settings for
Sublime. You can override any of these in `Preferences > Settings - User`.

Here are some settings that help match the Chromium style guide:
```
{
  // Basic Chromium style preferences
  "rulers": [80],
  "tab_size": 2,
  "trim_trailing_white_space_on_save": true,
  "ensure_newline_at_eof_on_save": true,
  "translate_tabs_to_spaces" : true,

  // Optional, but also useful, preferences
  "always_show_minimap_viewport": true,
  "bold_folder_labels": true,
  "draw_white_space": "all",
  "enable_tab_scrolling": false,
  "highlight_line": true,

  // Mainly for Windows, but harmless on Mac/Linux
  "default_line_ending": "unix",
}
```

The settings will take effect as soon as you save the file.

#### Tips
*   `View > Side Bar > Show Open Files` will add a list of open files to the top
    of the sidebar
*   ``Ctrl+` `` will show the console; it shows errors and debugging output, and
    you can run Python
*   `View > Enter Distraction Free Mode` goes into fullscreen and removes
    Sublime's header and footer
*   `View > Layout > ...` changes the configuration of files you can open side-
    by-side
*   `Ctrl + P` (`Cmd + P` on Mac) quickly opens a search box to find a file or
    definition
*   `Alt + O` (`Alt + Cmd + Up` on Mac) switches between the source/header file
*   `Alt + PageUp`/`Alt + PageDown` (`Alt + Cmd + Left`/`Alt + Cmd + Right` on
    Mac) moves between tabs
*   `F12` (`Alt + Cmd + Down` on Mac) goes to the symbol's definition
*   With text selected, `Ctrl + D` will multi-select the next occurrence (so
    typing in one types in all of them), and `Ctrl+U` deselects
*   Similarly, after finding something with `Ctrl + F`, `Alt + Enter` will
    select all occurrences of the search query, which can be multi-edited
*   `Ctrl + X` without anything selected cuts the current line, then move to a
    different line and `Ctrl + V` pastes it below the current line

### Setting Sublime as the default Terminal editor

Add `export EDITOR="subl -w"` to your `~/.bashrc` file (or similar) to open git
commit messages, gn args, etc with Sublime Text. Since you may want to only open
sublime when using a non-SSH session, you can wrap it in the following:

```
if [ "$SSH_CONNECTION" ]; then
  export EDITOR='vim'
else
  export EDITOR='subl -w'
fi
```

### Installing the Package Manager

The Sublime Package Manager is the way most Sublime packages are installed and
configured. You can install the package manager by following in the
[installation instructions](https://packagecontrol.io/installation) on their
website. Once the package manager is installed, restart Sublime.

To install a package, press `Ctrl + Shift + P` and select `Package Manager:
Install Package` (the string match is fairly lenient; you can just type
`"instp"` and it should find it). Then type or select the package you want to
install.

#### Mac Paths Fix

There is a known bug on Mac where Sublime doesn't detect the current path
correctly. If you're using Mac, install the package `SublimeFixMacPath` to find
the path from your `~/.bashrc` file or similar.

## Making a New Project

Once you have a copy of the Chromium checkout, we'll make a new Sublime project
with the src directory as the root.

To do this, create a new file `chromium.sublime-project` (or whatever name you'd
like) in the folder above your `src/` directory, with the following contents
(the exclude patterns are needed - Sublime can't handle indexing all of Chrome's
files):

```json
{
  "folders": [
    {
      "name": "chromium",
      "path": "src",
      "file_exclude_patterns":
      [
        "*.vcproj",
        "*.vcxproj",
        "*.sln",
        "*.gitignore",
        "*.gitmodules",
        "*.vcxproj.*",
      ],
      "folder_exclude_patterns":
      [
        "build",
        "out",
        "third_party",
        ".git",
      ],
    },
    {
      "name": "Generated Files",
      "path": "src/out/Debug/gen",
    },
  ],
}
```

If you are working on Blink, or any other third-party subproject, you can add it
as a separate entry in the `folders` array:

```json
{
  "name": "blink",
  "path": "src/third_party/blink",
}
```

Once you've saved the file, select `Project > Switch Project` and navigate to
the `chromium.sublime-project` file.

### Code Linting with CPPLint (Chromium only)

**Note:** CPPLint enforces the Google/Chromium style guide, and hence is not
useful on third_party projects that use another style.

1.  Install the SublimeLinter package (`Ctrl + Shift + P > Install Package >
    SublimeLinter`).
1.  `cpplint` should be somewhere on your path so that SublimeLinter finds it.
    depot_tools includes `cpplint.py`, but it needs to be named `cpplint`, so on
    Linux and Mac you have to make a symlink to it:

    ```shell
    cd /path/to/depot_tools
    ln -s cpplint.py cpplint
    chmod a+x cpplint
    ```

1.  Install the SublimeLinter-cpplint package (`Ctrl + Shift + P > Install
    Package > SublimeLinter-cpplint`).

Now when you save a C++ file, red dots should appear next to lines that
invalidate the style. You can change this behavior with Choose Lint Mode
(`Ctrl + Shift + P > "lint mode"`).

You can also see and navigate all the linter errors with Show All Errors
(`Ctrl + Shift + P > "show all"`). You can also use Next Error/Previous Error
(and their associated shortcuts) to navigate the errors. The gutter at the
bottom of the screen shows the message for the error on the current line.

You can also change the style of dot next to the line with Choose Gutter Theme
(`Ctrl + Shift + P > "gutter"`)

For a list of all preferences, see `Preferences > Package Settings >
SublimeLinter > Settings - Default` (or `Settings - User` to edit your
preferences).

### Format Selection with Clang-Format (Chromium only)

**Note:** Like CPPLint, Clang-format enforces the Google/Chromium style guide,
and hence is not useful on third_party projects that use another style.

1.  Inside `src/`, run:

    ```shell
    cd /path/to/chromium/src
    cp third_party/clang-format/script/clang-format-sublime.py ~/.config/sublime-text-3/Packages/User/
    ```

1. This installs a plugin that defines the command "clang\_format". You can add
   the "clang\_format" command to `Preferences > Key Bindings - User`, e.g.:

    ```json
    [
        { "keys": ["ctrl+shift+c"], "command": "clang_format" },
    ]
    ```

2. Select some text and press `Ctrl + Shift + C` to format, or select no text to
   format the entire file

## CodeSearch Integration with Chromium X-Refs

With [Chromium X-Refs](https://github.com/karlinjf/ChromiumXRefs/) you can
perform [https://cs.chromium.org](https://cs.chromium.org) cross-reference
searches in your editor. This gives you the call graph, overrides, references,
declaration, and definition of most of the code. The results are as fresh as
the search engine's index so uncommitted changes won't be reflected.

More information on Chromium X-Ref's functionality (including keyboard and
mouse shortcuts) can be found on the [Chromium X-Refs
page](https://github.com/karlinjf/ChromiumXRefs/).


## Code Completion, Error Highlighting, Go-to-Definition, and Find References with LSP (clangd)

Gives Sublime Text 3 rich editing features for languages with Language Server
Protocol support. It searches the current compilation unit for definitions and
references and provides super fast code completion.

In this case, we're going to add C/C++ support.

1. Refer to [clangd.md](clangd.md) to install clangd and build a compilation
   database.

1. Install the [LSP Package](https://github.com/tomv564/LSP) and enable clangd
   support by following the [link](https://clang.llvm.org/extra/clangd/Installation.html#editor-plugins)
   and following the instructions for Sublime Text.

To remove sublime text's auto completion and only show LSPs (recommended), set
the following LSP preference:

```json
"only_show_lsp_completions": true
```

## Code Completion with SublimeClang (Linux Only) [Deprecated, see LSP above]

SublimeClang is a powerful autocompletion plugin for Sublime that uses the Clang
static analyzer to provide real-time type and function completion and
compilation errors on save. It works with Chromium with a script that finds and
parses the appropriate \*.ninja files to find the necessary include paths for a
given file.

**Note**: Currently, only the Linux setup of SublimeClang is working. However,
there are instructions below for Windows/Mac which you are welcome to try -- if
you can get them to work, please update these instructions ^\_^

More information on SublimeClang's functionality (including keyboard shortcuts)
can be found on the [SublimeClang GitHub
page](https://github.com/quarnster/SublimeClang).


### Linux

**Note** that there are recent (as of August 2017) changes to support C++14.
Namely, you must use a more recent clang (3.9 is known to work), and use its
resource directory instead of that supplied by SublimeClang.

1. Install a recent libclang-dev to get a copy of libclang.so. 3.4 isn't
   recent enough, but 3.9 works. If you use something different, change the
   names and paths accordingly:

    ```shell
    sudo apt-get install libclang-3.9-dev
    ```

1.  Build libclang.so and SublimeClang in your packages directory:

    ```shell
    cd ~/.config/sublime-text-3/Packages
    git clone --recursive https://github.com/quarnster/SublimeClang SublimeClang
    cd SublimeClang
    # Copy libclang.so to the internals dir
    cp /usr/lib/llvm-3.9/lib/libclang.so.1 internals/libclang.so
    # Fix src/main.cpp (shared_ptr -> std::shared_ptr)
    sed -i -- 's/shared_ptr/std::shared_ptr/g' src/main.cpp
    # Make the project - should be really quick, since libclang.so is already built
    cd src && mkdir build && cd build
    cmake ..
    make
    ```

1.  Edit your project file `Project > Edit Project` to call the script above
    (replace `/path/to/depot_tools` with your depot_tools directory):

    ```json
    {
      "folders":
      [
        ...
      ],
      "settings":
      {
        "sublimeclang_options":
        [
          "-Wno-attributes",
          "-resource-dir=/usr/lib/llvm-3.9/lib/clang/3.9.1",
        ],
        "sublimeclang_options_script": "python ${project_path}/src/tools/sublime/ninja_options_script.py -d '/path/to/depot_tools'",
      }
    }
    ```
1. Edit your SublimeClang settings and set `dont_prepend_clang_includes` to
   true. This way you use the resource directory we set instead of the ancient
   ones included in the repository. Without this you won't have C++14 support.

1. (Optional) To remove errors that sometimes show up from importing out of
   third_party, edit your SublimeClang settings and set:

   ```json
   "diagnostic_ignore_dirs":
   [
     "${project_path}/src/third_party/"
   ],
   ```

1.  Restart Sublime. Now when you save a file, you should see a "Reparsing…"
    message in the footer and errors will show up in the output panel. Also,
    variables and function definitions should auto-complete as you type.

**Note:** If you're having issues, adding `"sublimeclang_debug_options": true` to
your settings file will print more to the console (accessed with ``Ctrl + ` ``)
which can be helpful when debugging.

**Debugging:** If things don't seem to be working, the console ``Ctrl + ` `` is
your friend. Here are some basic errors which have workarounds:

1. Bad Libclang args
    - *problem:* ```tu is None...``` is showing up repeatedly in the console:
    - *solution:* ninja_options_script.py is generating arguments that libclang
    can't parse properly. To fix this, make sure to
    ```export CHROMIUM_OUT_DIR="{Default Out Directory}"```
    This is because the ninja_options_script.py file will use the most recently
    modified build directory unless specified to do otherwise. If the chosen
    build directory has unusual args (say for thread sanitization), libclang may
    fail.


### Mac (not working)

1.  Install cmake if you don't already have it
1.  Install XCode
1.  Copy libclang.dylib from XCode to the SublimeClang/internals folder:

    ```shell
    cd ~/Library/Application\ Support/Sublime\ Text\ 3/Packages
    git clone --recursive https://github.com/quarnster/SublimeClang SublimeClang
    cd SublimeClang
    cp /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib internals/libclang.dylib
    # Remove i386 from the build file since XCode's libclang.dylib is only a 64-bit version
    sed -ie 's/CMAKE_OSX_ARCHITECTURES i386 x86_64/CMAKE_OSX_ARCHITECTURES x86_64/' src/CMakeLists.txt
    # Copy libclang.dylib to the internals dir
    # Make the project - should be really quick, since libclang.dylib is already built
    cd src && mkdir build && cd build
    cmake ..
    make
    ```

1.  The rest of the instructions are the same, but when adding your project
    settings, add these extra arguments to `sublimeclang_options`:

    ```json
    "sublimeclang_options":
    [
      ...
      // MAC-ONLY: Include these options, replacing the paths with the correct installed SDK
      "-isystem", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/usr/include/",
      "-isystem", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/usr/include/c++/4.2.1",
      "-F/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/System/Library/Frameworks/",
      "isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk",
      "-mmacosx-version-min=10.7",
      "-stdlib=libc++",
      "-isystem", "/usr/include",
      "-isystem", "/usr/include/c++/*",
    ]
    ```

### Windows (not working)

You'll need cl.exe which can be installed with [the Visual C++ Build Tools
2015](https://blogs.msdn.microsoft.com/vcblog/2016/03/31/announcing-the-official-release-of-the-visual-c-build-tools-2015/).
You should have cl.exe on your `$PATH`, which you can get by running `C:\Program
Files (x86)\Microsoft Visual C++ Build Tools\Visual C++ 2015 x64 Native Build
Tools Command Prompt`.

Then you'll need a copy of libclang.so, which can be found on the [LLVM
website](http://llvm.org/releases/download.html). The instructions should be the
same as Linux from there.

## Alternative: Code Completion with Ctags

For a fast way to look up symbols, we recommend installing the CTags plugin.

1.  Install Exuberant Ctags and make sure that ctags is in your path:
    http://ctags.sourceforge.net/ (on linux you should be able to just do `sudo
    apt-get install ctags`)
1.  Install the Ctags plugin: `Ctrl + Shift + P > Install Package > Ctags`

Once installed, you'll get an entry in the context menu when you right click the
top level folder(s) in your project that allow you to build the Ctags database.
If you're working in a Chrome project however, do not do that at this point,
since it will index much more than you actually want. Instead, do one of:

1.  Create a batch file (e.g. ctags_builder.bat) that you can run either
    manually or automatically after you do a gclient sync:

    ```
    ctags --languages=C++ --exclude=third_party --exclude=.git --exclude=build --exclude=out -R -f .tmp_tags & ctags --languages=C++ -a -R -f .tmp_tags third_party\platformsdk_win7 & move /Y .tmp_tags .tags
    ```

    This takes a couple of minutes to run, but you can work while it is indexing.
1.  Edit the `CTags.sublime-settings` file for the ctags plugin so that it runs
    ctags with the above parameters.  Note: the above is a batch file - don't
    simply copy all of it verbatim and paste it into the CTags settings file)

Once installed, you can quickly look up symbols with `Ctrl+t, Ctrl+t` etc.  More
information on keyboard shortcuts can be found on the [CTags GitHub
page](https://github.com/SublimeText/CTags).

One more hint - Edit your `.gitignore` file (under `%USERPROFILE%` or `~/`) so
that git ignores the `.tags` file.  You don't want to commit it. :)

If you don't have a `.gitignore` in your profile directory, you can tell git
about it with this command: Windows: `git config --global core.excludesfile
%USERPROFILE%\.gitignore` Mac, Linux: `git config --global core.excludesfile
~/.gitignore`

## Building inside Sublime

To build inside Sublime Text, we first have to create a new build system.

You can add the build system to your project file (`Project > Edit Project`),
replacing `out/Debug` with your output directory (on Windows, replace /'s with
\s in `cmd` and `working_dir`):

```json
{
  "folders": [
  ...
  ],
  "build_systems":
  [
    {
      "name": "Build Chrome",
      "cmd": ["ninja", "-C", "out/Debug", "chrome"],
      "working_dir": "${project_path}/src",
      "file_regex": "^[.\\\\/]*([a-z]?:?[\\w.\\\\/]+)[(:]([0-9]+)[,:]?([0-9]+)?[)]?:(.*)$",
      "variants": [],
    },
  ],
}
```

The file regex will allow you to click on errors to go to the error line.

If you're using reclient, add the -j parameter (replace out/Debug with your out directory):
```
    "cmd": ["ninja", "-j", "1000", "-C", "out/Debug", "chrome"],
```

**Regex explanation:** Aims to capture these error formats while respecting
[Sublime's perl-like group matching](http://docs.sublimetext.info/en/latest/reference/build_systems/configuration.html#build-capture-error-output):

1.  `d:\src\chrome\src\base\threading\sequenced_worker_pool.cc(670): error
    C2653: 'Foo': is not a class or namespace name`
1.  `../../base/threading/sequenced_worker_pool.cc(670,26) error: use of
    undeclared identifier 'Foo'`
1.  `../../base/threading/sequenced_worker_pool.cc:670:26: error: use of
    undeclared identifier 'Foo'`

```
"file_regex": "^[.\\\\/]*([a-z]?:?[\\w.\\\\/]+)[(:]([0-9]+)[,:]?([0-9]+)?[)]?:(.*)$"
                (   0   ) (   1  )(    2     ) (3 )(  4   )( 5 )(   6   )(7 ) (8 )

(0) Cut relative paths (which typically are relative to the out dir and targeting src/ which is already the "working_dir")
(1) Match a drive letter if any
(2) Match the rest of the file
(1)+(2) Capture the "filename group"
(3) File name is followed by open bracket or colon before line number
(4) Capture "line number group"
(5) If (6) is non-empty there will be a comma or colon preceding it (but can't put it inside brackets as the "column number group" only wants digits).
(6) Capture "column number group" if any
(7) Closing bracket of either "(line)" or "(line,column)" if bracket syntax is in effect
(8) Everything else until EOL is the error message.
```

### Building other targets

You can add build variants to the `variants` array to have quick access to other
build targets with `Ctrl + Shift + B`:

```json
"variants":
  [
    {
      "name": "Unit Tests",
      "cmd": ["ninja", "-j", "1000", "-C", "out/Debug", "unit_tests"],
    },
    {
      "name": "Browser Tests",
      "cmd": ["ninja", "-j", "1000", "-C", "out/Debug", "browser_tests"],
    },
    {
      "name": "Current file",
      "cmd": ["compile_single_file", "--build-dir", "out/Debug", "--file-path", "$file"],
    },
  ]
```

You can also add a variant for running chrome, meaning you can assign a keyboard
shortcut to run it after building:

```json
"variants":
  [
    ...
    {
      "cmd": ["out/Debug/chrome"],
      "name": "run_chrome",
      "shell": true,
      "env": {
        "CHROME_DEVEL_SANDBOX": "/usr/local/sbin/chrome-devel-sandbox",
      },
    },
  ]
```

### More detailed stack traces

Chrome's default stack traces don't have full file paths so Sublime can't
parse them. You can enable more detailed stack traces and use F4 to step right
to the crashing line of code.

First, add `print_unsymbolized_stack_traces = true` to your gn args, and make
sure you have debug symbols enabled too (`symbol_level = 2`). Then, pipe
Chrome's stderr through the asan_symbolize.py script. Here's a suitable build
variant for Linux (with tweaked file_regex):

```json
{
  "name": "Build and run with asan_symbolize",
  "cmd": "ninja -j 1000 -C out/Debug chrome && out/Debug/chrome 2>&1 | ./tools/valgrind/asan/asan_symbolize.py",
  "shell": true,
  "file_regex": "(?:^|[)] )[.\\\\/]*([a-z]?:?[\\w.\\\\/]+)[(:]([0-9]+)[,:]?([0-9]+)?[)]?:?(.*)$"
}
```

You can test it by visiting chrome://crash. You should be able to step through
each line in the resulting stacktrace with F4. You can also get a stack trace
without crashing like so:

```c++
#include "base/debug/stack_trace.h"
[...]
base::debug::StackTrace().Print();
```

### Assigning builds to keyboard shortcuts

To assign a build to a keyboard shortcut, select `Preferences > Key Bindings -
User` (or `Key Bindings - Default` to see the current key bindings). You can add
the build variants above with the `"build"` command, like so:

```json
[
  ...
  { "keys": ["ctrl+shift+u"], "command": "build", "args": {"variant": "unit_tests"} },
  { "keys": ["ctrl+shift+b"], "command": "build", "args": {"variant": "browser_tests"} },
  { "keys": ["ctrl+shift+x"], "command": "build", "args": {"variant": "run_chrome"} },
]
```

For more info on custom key bindings, see the
[Sublime Text Key Bindings Documentation](http://docs.sublimetext.info/en/latest/customization/key_bindings.html).

## Other useful packages

Some other useful packages that improve sublime can be installed from `Ctrl+Shift+P > Install Package`:

*   Git enhancements
    *   [Git](https://packagecontrol.io/packages/Git)
    *   [GitCommitMsg](https://packagecontrol.io/packages/GitCommitMsg) -
        Performs a 'git blame' for one or more lines of code with `Alt + Shift +
        M` (`Command + Shift + M` on Mac)
    *   [GitDiffHelper](https://packagecontrol.io/packages/GitDiffHelper) -
        `Ctrl + Alt + G` to open all files modified since the last commit
    *   [GitOpenChangedFiles](https://packagecontrol.io/packages/GitOpenChangedFiles) -
        `Ctrl + Shift + O` (`Command + Shift + O` on Mac) to open all files
        modified on the current branch
    *   [Git Conflict
        Resolver](https://packagecontrol.io/packages/Git%20Conflict%20Resolver)
        - A GUI for resolving git conflicts
    *   [GitGutter](https://packagecontrol.io/packages/GitGutter) - Shows an
        icon next to lines that have been inserted, modified or deleted since
        the last commit.
*   Visual enhancements
    *   [SyncedSideBar](https://packagecontrol.io/packages/SyncedSideBar) -
        Syncs the currently open file with the expanded tree in the sidebar
    *   [SideBarEnhancements](https://packagecontrol.io/packages/SideBarEnhancements) -
        Adds more file management options to the sidebar context menu.
    *   [SyncedSidebarBg](https://packagecontrol.io/packages/SyncedSidebarBg) -
        A purely aesthetic improvement that syncs the sidebar background with
        the background color for the current theme.
    *   [Theme - Soda](http://buymeasoda.github.io/soda-theme/) - A global theme
        for Sublime that matches the default color scheme. Needs `"theme": "Soda
        Light 3.sublime-theme"` in your Preferences > Settings - User` file.
*   Code navigation tools
    *   [AutoFileName](https://packagecontrol.io/packages/AutoFileName) - Auto-
        completes filenames in #includes
    *   [Open-Include](https://packagecontrol.io/packagenavigations/Open-Include)
        - Opens the file path under the cursor with `Alt + D`
*   Text tools
    *   [Case Conversion](https://packagecontrol.io/packages/Case%20Conversion) -
        automatically changes the case of selected text, e.g. `kConstantName` to
        `CONSTANT_NAME`
    *   [Text Pastry](https://packagecontrol.io/packages/Text%20Pastry) -
        Inserts incremental number sequences with multi-select
    *   [Wrap Plus](https://packagecontrol.io/packages/Wrap%20Plus) - Auto-wraps
        a comment block to 80 columns with `Alt + Q` (was used to write this
        document! ;-)
    *   [Diffy](https://packagecontrol.io/packages/Diffy) - With two files
        opened side-by-side, `Ctrl + k Ctrl + d` will show the differences
