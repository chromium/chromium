# Visual Studio Code Dev

**Get started [here](#setup)**.

[Visual Studio Code (VS Code)](https://code.visualstudio.com) is a free, open
source, lightweight and powerful code editor for Windows, macOS and Linux, based
on [Electron](https://www.electronjs.org/)/Chromium. It has built-in support for
JavaScript, TypeScript and Node.js and a rich extension ecosystem that adds
intellisense, debugging, syntax highlighting etc. For many languages like C++,
Python, Go, Java, it works without too much setup.

It is NOT a full-fledged IDE like Visual Studio. The two are completely
separate products. The only commonality with Visual Studio is that both are
from Microsoft.

Here's what works well:

*   **Editing code** works well especially when you get used to the [keyboard
    shortcuts](#Keyboard-Shortcuts). VS Code is very responsive and can handle
    even big code bases like Chromium.
*   **Git integration** is a blast. Built-in side-by-side view, local commit and
    even extensions for
    [history](https://marketplace.visualstudio.com/items?itemName=donjayamanne.githistory)
    and
    [blame view](https://marketplace.visualstudio.com/items?itemName=ryu1kn.annotator).
*   [**Debugging**](https://code.visualstudio.com/Docs/editor/debugging) works
    well, even though startup times can be fairly high (~40 seconds with
    gdb on Linux, much lower on Windows). You can step through code, inspect
    variables, view call stacks for multiple threads etc.
    *   For more information on debugging Python code, see [here](vscode_python.md).
*   **Command Palette** makes opening files and searching solution really easy.
*   **Building** works well. Build tools are easy to integrate. Warnings and errors
    are displayed on a separate page and you can click to jump to the
    corresponding line of code.
*   **VS Code Remote**, which allows you to edit remotely-hosted code, and even
    run computationally expensive plugins like vscode-clangd on the remote
    server. Great for working from home. See the [Remote section](#Remote) for
    more details.

[TOC]


## Updating This Page

Please keep this doc up-to-date. VS Code is still in active development and
subject to changes. This doc is checked into the Chromium git repo, so if you
make changes, read the [documentation
guidelines](documentation_guidelines.md) and
[submit a change list](contributing.md).

All file paths and commands have been tested on Linux and macOS. Windows might
require a slightly different setup. Please update this page accordingly.


## Setup

### Installation

*** promo
Googlers: See [go/vscode/install](http://go/vscode/install) instead.
***

Follow the steps on [Setting up Visual Studio Code][setup] to install a proper
version for you development platform.

[setup]: https://code.visualstudio.com/docs/setup/setup-overview

### Usage

To run it on Linux or on macOS:

```bash
cd /path/to/chromium/src
code .
```

If you installed Code Insiders, the binary name is `code-insiders` instead.

Note that VS Code does not require project or solution files. However, it does
store workspace settings in a `.vscode` folder in your base directory (i.e. your
project root folder). See the [Chromium Workspace Settings](#setup-for-chromium)
section for details.

### Useful Extensions

Up to now, you have a basic version of VS Code without much language support.
Next, we will install some useful extensions.

#### Recommended Extensions

You will most likely use the following extensions every day:

There are 2 ways to install them:

*   Follow the instructions from
    [Install Recommended Extensions](#install-recommended-extensions).
*   Manual installation. Jump to the extensions window (`Ctrl+Shift+X`, or
    `Cmd+Shift+X` in macOS) and search the names of the following extensions.

*** aside
Note: All the extension settings mentioned below are already set in
`tools/vscode/settings.json`. You don't have do anything if you have followed
[the instructions](#setup-for-chromium) to copy that file into your workspace.
***

*   [**ChromiumIDE**](https://marketplace.visualstudio.com/items?itemName=Google.cros-ide) -
    The critical extension to make Chromium/ChromiumOS development easier and
    faster by anchoring core tools in one place.
*   [**Chromium Context**](https://marketplace.visualstudio.com/items?itemName=solomonkinard.chromium-context) -
    Provides Chromium-specific context, e.g. code owners, release version,
    author blame list, in a single tab for an opened file.
*   [**C/C++**](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) -
    Code formatting, debugging, Intellisense. Enables the use of clang-format
    (via the `C_Cpp.clang_format_path` setting) and format-on-save (via the
    `editor.formatOnSave` setting).
*   [**vscode-clangd**](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) -
    Enables VS Code to compile Chromium, provide Chromium XRefs to support
    functions like jumping to definition, and provide a clangd
    [language server][lang-server] powering smarter autocompletion than
    **C/C++** extension's IntelliSense, but they also conflicts with each
    other. To resolve the conflict, add the following to `settings.json`:
    `"C_Cpp.intelliSenseEngine": "disabled"`.  See [clangd.md](clangd.md) for
    setup instructions.
*   [**Toggle Header/Source**](https://marketplace.visualstudio.com/items?itemName=bbenoist.togglehs) -
    Toggles between .cc and .h with `F4`. The C/C++ extension supports this as
    well through `Alt+O` but sometimes chooses the wrong file when there are
    multiple files in the workspace that have the same name.
*   [**vscode-proto3**](https://marketplace.visualstudio.com/items?itemName=zxh404.vscode-proto3) -
    Syntax highlighting for .proto files.
*   [**Mojom IDL support**](https://marketplace.visualstudio.com/items?itemName=Google.vscode-mojom) -
    Syntax highlighting and a [language server][lang-server] for .mojom files.
*   [**GN**](https://marketplace.visualstudio.com/items?itemName=msedge-dev.gnls) -
    Code IntelliSense for the GN build system.
*   [**Rewrap**](https://marketplace.visualstudio.com/items?itemName=stkb.rewrap) -
    Wrap lines at 80 characters with `Alt+Q`.
*   [**Remote**](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-ssh) -
    Remotely connect to your workstation through SSH using your laptop. See the
    [Remote](#Remote) section for more information about how to set this up.
*   [**GitLens**](https://marketplace.visualstudio.com/items?itemName=eamodio.gitlens) -
    Git supercharged. A Powerful, feature rich, and highly customizable git
    extension.
*   [**Python**](https://marketplace.visualstudio.com/items?itemName=ms-python.python) -
    Linting, intellisense, code formatting, refactoring, debugging, snippets.
    * If you want type checking, add: `"python.analysis.typeCheckingMode": "basic",`
      to your `settings.json` file (you can also find it in the settings UI).

[lang-server]: https://microsoft.github.io/language-server-protocol/

#### Optional Extensions

The following extensions are not included in
[//tools/vscode/settings.json](/tools/vscode/settings.json), but they might be
useful for you as well:

```bash
$ echo "ryu1kn.annotator wmaurer.change-case shd101wyy.markdown-preview-enhanced Gruntfuggly.todo-tree alefragnani.Bookmarks spmeesseman.vscode-taskexplorer streetsidesoftware.code-spell-checker george-alisson.html-preview-vscode anseki.vscode-color" | xargs -n 1 code --force --install-extension
```

*   [**Annotator**](https://marketplace.visualstudio.com/items?itemName=ryu1kn.annotator) -
    Display git blame info along with your code. Can open the diff of a particular commit from there.
*   [**change-case**](https://marketplace.visualstudio.com/items?itemName=wmaurer.change-case) -
    Quickly change the case of the current selection or current word.
*   [**Markdown Preview Enhanced**](https://marketplace.visualstudio.com/items?itemName=shd101wyy.markdown-preview-enhanced) -
    Preview markdown side-by-side with automatic scroll sync and many other
    features with `Ctrl+k v`. This document was written with this extension!
*   [**Todo Tree**](https://marketplace.visualstudio.com/items?itemName=Gruntfuggly.todo-tree) -
    Displays comment tags like TODO/FIXME in a tree view in a dedicated sidebar.
*   [**Bookmarks**](https://marketplace.visualstudio.com/items?itemName=alefragnani.Bookmarks) -
    Supports easy mark/unmark positions in the codebase and displays them in a
    dedicated sidebar. Very useful for a large codebase like Chromium.
*   [**Task Explorer**](https://marketplace.visualstudio.com/items?itemName=spmeesseman.vscode-taskexplorer) -
    Displays supported tasks, e.g. vscode tasks, shell scripts and others,
    organized into a treeview in sidebar.
*   [**Code Spell Checker**](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker) -
    A basic spell checker that works well with camelCase code. It helps catch
    common spelling errors.
*   [**HTML Preview**](https://marketplace.visualstudio.com/items?itemName=george-alisson.html-preview-vscode) -
    Previews HTML files while editing with `Ctrl+k v`.
*   [**Color Picker**](https://marketplace.visualstudio.com/items?itemName=anseki.vscode-color) -
    Visualizes color codes inline and provides color picker GUI to generates new
    color codes.
*   [**Bazel**](https://marketplace.visualstudio.com/items?itemName=BazelBuild.vscode-bazel) -
    This is very useful for editing `*.star` starlark files. If you want "Go
    to definition" to work in our `infra/config` directory, see the
    [//tools/vscode/bazel_lsp/README.md][lsp_patches_readme]

[lsp_patches_readme]: ../tools/vscode/bazel_lsp/README.md


Also be sure to take a look at the
[VS Code marketplace](https://marketplace.visualstudio.com/VSCode) to check out
other useful extensions.

### Color Scheme

Press `Ctrl+Shift+P (Cmd+Shift+P `in macOS`), color, Enter`  to pick a color
scheme for the editor. There are also tons of [color schemes available for
download on the
marketplace](https://marketplace.visualstudio.com/search?target=VSCode&category=Themes&sortBy=Downloads).

### Keyboard Shortcuts

#### CheatSheet

*   [Windows](https://code.visualstudio.com/shortcuts/keyboard-shortcuts-windows.pdf)
*   [macOS](https://code.visualstudio.com/shortcuts/keyboard-shortcuts-macos.pdf)

#### Useful Shortcuts (Linux)

*   `Ctrl+P` opens a search box to find and open a file.
*   `F1` or `Ctrl+Shift+P` opens a search box to find a command (e.g. Tasks: Run
    Task). Note: if you want to run one of the [Predefined tasks in
    tasks.json](#Tasks), it is faster to just use `Ctrl+P` &gt; "task <n>".
*   `Ctrl+K, Ctrl+S` opens the key bindings editor.
*   ``Ctrl+` `` toggles the built-in terminal.
*   `Ctrl+Shift+M` toggles the problems view (linter warnings, compile errors
    and warnings). You'll switch a lot between terminal and problem view during
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

*** aside
Note: See also [Key Bindings for Visual Studio Code
](https://code.visualstudio.com/docs/getstarted/keybindings).
***

### Java/Android Support

Follow these steps to get full IDE support (outline, autocompletion, jump to
definition including automatic decompilation of prebuilts, real-time reporting
of compile errors/warnings, Javadocs, etc.) when editing `.java` files in
Chromium:

1. **Add the following to your VS Code workspace `settings.json`:**

   ```
   "java.import.gradle.enabled": false,
   "java.import.maven.enabled": false
   ```

   This will prevent the language server from attempting to build *all* Gradle
   and Maven projects that can be found anywhere in the Chromium source tree,
   which typically results in hilarity.

   ```
   "java.jdt.ls.java.home": "<< ABSOLUTE PATH TO YOUR WORKING COPY OF CHROMIUM >>/src/third_party/jdk/current"
   ```

   This one is optional but reduces the likelihood of problems by making sure
   the language server uses the same JDK as the Chromium build system (as
   opposed to some random JDK from your host system).

2. **Install the
   [*Language Support for Java™ by Red Hat*](https://marketplace.visualstudio.com/items?itemName=redhat.java)
   extension.**
   You do not need any other extension.
3. **Build your code** in the usual way (i.e. using gn and ninja commands).
   This will produce build config files that are necessary for the next step. It
   will also make autogenerated code visible to the language server.
4. **Generate the Eclipse JDT project** by running
   `build/android/generate_vscode_project.py` from the `src` directory.
   For example, if your build output directory is `out/Debug-x86` and your build
   target is `//chrome/android:chrome_java`, run:
   `build/android/generate_vscode_project.py --output-dir out/Debug-x86 --build-config gen/chrome/android/chrome_java.build_config.json`.
   This will create `.project` and `.classpath` in the `src` directory.
5. **Reload** your VS Code window to let it start importing the generated
   project.
6. **Open a Java source file then wait a couple of minutes** for the language
   server to build the project.
7. **Done!** You should now have full Java language support for any `.java` file
   that is included in the build.

#### Known issues

* Errors related to `GEN_JNI` are caused by the language server (rightfully)
  getting confused about multiple definitions of the
  [autogenerated](/third_party/jni_zero/README.md) `GEN_JNI` class. This
  is a known quirk of the JNI generator.

#### Troubleshooting

* If you have used the previous instructions to use
  `generate_vscode_classpath.py` or you think something went wrong, try clearing
  the internal state of the language server by executing
  `Java: Clean Java Language Server Workspace` from the command palette. This
  will force the language server to rebuild its internal workspace by importing
  the generated Eclipse JDT project.

#### Automatic formatting

Java code in Chromium is formatted using [clang-format](/docs/clang_format.md).
To get VS Code to use clang-format to format Java files, install the
[*Clang-Format* extension](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format)
and set it as the default formatter for Java in your workspace `settings.json`:

```json
"[java]": {
  "editor.defaultFormatter": "xaver.clang-format"
}
```

To avoid potential formatting differences due to clang-format version skew, it
makes sense to configure the extension to run clang-format in the same way
`git cl format` would. You can do this by adding the following to your
workspace `settings.json`:

```json
"clang-format.executable": "<< PATH TO YOUR CHROMIUM WORKING COPY >>/src/buildtools/linux64/clang-format"
```

## Setup For Chromium

VS Code is configured via JSON files. This section describes how to configure
it for a better Chromium development experience.

*** aside
Note: See [VS Code
documentation](https://code.visualstudio.com/docs/customization/overview) for a
more general introduction to VS Code customization.
***

The Chromium repository comes with some basic configuration. Run the following
commands to initialize VS Code for your Chromium checkout:

```bash
cd /path/to/chromium/src
mkdir .vscode
cp tools/vscode/*.json .vscode/
cp tools/vscode/cpp.code-snippets .vscode/
```

Once you have done, proceed to the next sections to install recommended
extensions and perform customization.

### Install Recommended Extensions

As described in the [Useful Extensions](#useful-extensions) sections, there are
essential extensions to help Chromium development. Follow the steps below:

1. In VS Code's Command Palette (`Ctrl+P`, or `Cmd+Shift+P` in macOS),
   type `Show Recommended Extensions`, and press `Enter`.
2. In the WORKSPACE RECOMMENDATIONS section of the EXTENSIONS sidebar, click the
   `Install Workspace Recommended Extensions` (shown as a cloud icon).

Now you are all set.

### Customize Workspace Settings

Open the file [//tools/vscode/settings.json](/tools/vscode/settings.json),
and check out the default settings there. Feel free to commit added or removed
settings to enable better team development, or change settings locally in
`.vscode/settings.json` to suit personal preference.


*** aside
Note: these settings assume that the workspace folder (the root folder displayed
in the Explorer tab) is Chromium's `src/` directory. If this is not the case,
replace any references to `${workspaceFolder}` with the path to your `src/`.
***

### Tasks

Next, we'll tell VS Code how to compile our code, run tests, and to read
warnings and errors from the build output.

Open the file `.vscode/tasks.json`. This will provide tasks to do basic things.
You might have to adjust the commands to your situation and needs. For example,
before running most of the tasks, you'll need to set the `chromeOutputDir` value
in that file.

Now you can run tasks by using `Ctrl+P` (`Cmd+Shift+P` in macOS) and typing
"task " and then a number of your choice. If you select one of the build tasks,
the build output will display in the terminal pane. Jump through build problems
quickly using `F8` / `Shift-F8`. See [task names](#task-names) for more info on
running tasks.

If you have intellisense enabled but do not have include paths set up correctly,
jumping through problems will also try to navigate through all the include files
it cannot locate and add a lot of noise. You can fix your include path or simply
set intellisense to "tag parser" mode by doing the following:

1. Open Preferences (`Ctrl+Shift+P` &gt; "Preferences: Open User Settings").
2. Type "intellisense engine" in the settings search box.
3. Select "Tag Parser" as the provider.

Note: on a Chromebook, use **🔍+<8th button in the top row that's not ESC>**. In
most cases, this is the top row button that is the closest to be directly above
the 8 key.

### Launch Commands

Launch commands are the equivalent of `F5` in Visual Studio: They launch some
program or a debugger. Optionally, they can run some task defined in
`tasks.json`. Launch commands can be run from the debug view (`Ctrl+Shift+D`).

Open the file at `.vscode/launch.json` and adjust the example launch commands to
your situation and needs (e.g., the value of "type" needs adjustment for
Windows).

### Key Bindings

To edit key bindings, press `Ctrl+K, Ctrl+S`. You'll see the defaults on the
left and your overrides on the right stored in the file
`.vscode/keybindings.json`. Please take a look and adjust them to your situation
and needs. To change a key binding, copy the corresponding key binding to the
right. It's fairly self-explanatory.

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

### Fixes for Known Issues

#### Git on Windows

If you only have the `depot_tools` Git installed on your machine, even though it
is in your PATH, VS Code will ignore it as it seems to be looking for `git.exe`.
You will have to add the following to your settings in order for the Git
integration to work:

```json
{
  "git.path": "C:\\src\\depot_tools\\git.bat"

  // more settings here...
}
```

Tip: you can jump to the settings JSON file by using `Ctrl+Shift+P` and using
the "Preferences: Open User Settings (JSON)" verb (for whatever reason, setting
`git.path` as a folder setting does not appear to work).

### Remote

*** promo
Googlers: See [go/vscode-remote](http://go/vscode-remote) instead.
***

VS Code now has a
[Remote](https://code.visualstudio.com/docs/remote/remote-overview) framework
that allows you to use VS Code on your laptop while your code is hosted
elsewhere. This really shines when used in conjunction with the vscode-clangd plugin,
which allows clangd to run remotely as well.

To get this to run, install the Remote pack extension, and then make sure your
ssh config file has your remote connection:

`~/.ssh/config`:
```
Host my-connection
  HostName my-remote-host.corp.company.com
```

VS Code will then list this connection in the 'Remote Explorer' section on the
left. To launch VS Code with this connection, click on the '+window' icon next
to the listed hostname. It has you choose a folder - use the 'src' folder root.
This will open a new VS Code window in 'Remote' mode. ***Now you can install
extensions specifically for your remote connection, like vscode-clangd, etc.***

#### Chromebooks

For Googlers, [here](http://go/vscode/remote_development_via_web) are
Google-specific instructions for setting up remote development on chromebooks
without using Crostini.

#### Windows & SSH

VS Code remote tools requires 'sshd' which isn't installed on Windows by
default.

For Googlers, sshd should already be installed on your workstation, and VS Code
should work remotely if you followed the setup instructions at
[go/building-chrome-win](http://go/building-chrome-win). If you are still having
problems, please refer to
[go/vscode-remote#windows](http://go/vscode-remote#windows).

Non-Googlers may follow Microsoft's instructions for
[installing the OpenSSH server](https://docs.microsoft.com/en-us/windows-server/administration/openssh/openssh_install_firstuse).
VS Code should work remotely after following this step.

### Snippets

There are some useful snippets provided in
[//tools/vscode/cpp.json](/tools/vscode/cpp.json), which are already installed
to your workspace at `.vscode/cpp.code-snippets`

### Tips

#### The `out` folder

Automatically generated code is put into a subfolder of `out/`, which means that
these files are ignored by VS Code (see files.exclude above) and cannot be
opened e.g. from quick-open (`Ctrl+P`). As of version 1.21, VS Code does not
support negated glob commands, but you can define a set of exclude pattern to
include only `out/Debug/gen`:
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

You might want to disable git status autorefresh to save battery.

```
"git.autorefresh": false,
```

#### Editing in multiple Git repositories

If you frequently work in multiple Git repositories that are part of the Chromium repository, you might find that the built-in tooling does not work as expected for files that exist below folders that are part of a `.gitignore` file checked in to Chromium.

To work around this, you can add the directories you edit as separate `folders` entries in your workspace configuration, and ensure that the directories that are ignored in Chromium are listed **before** the Chromium `src` path.

To edit this, go to `Settings` -> Select the `Workspace` tab, and choose to open as JSON (button in the top right), and configure `folders` like this (change paths to match your local setup and usage):

```
{
  "folders": [
    {
      "path": "chromium/src/third_party/perfetto"
    },
    {
      "path": "chromium/src"
    }
  ]
}
```

### Unable to open $File resource is not available when debugging Chromium on Linux

Chromium [recently changed](https://docs.google.com/document/d/1OX4jY_bOCeNK7PNjVRuBQE9s6BQKS8XRNWGK8FEyh-E/edit?usp=sharing)
the file path to be relative to the output dir. Check
`gn args out/$dir --list` if `strip_absolute_paths_from_debug_symbols` is true (which is the default),
set `cwd` to the output dir. otherwise, set `cwd` to `${workspaceFolder}`.

### More

More tips and tricks can be found
[here](https://code.visualstudio.com/docs/getstarted/tips-and-tricks).
