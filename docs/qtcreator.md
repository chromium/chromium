# Use Qt Creator as an IDE or debugger UI

[Qt Creator](https://www.qt.io/ide/)
([Wiki](https://en.wikipedia.org/wiki/Qt_Creator)) is a cross-platform C++ IDE.

You can use Qt Creator as a daily IDE on Linux or Mac, or just as a GDB/LLDB
frontend (which does not require project configuration).

[TOC]

## IDE

### Workflow features

* Built-in code completion.
* Navigate to classes, files, or symbols with `ctrl+k` or `cmd+k` (macOS).
* Switch between declaration and definition with `F2`.
* Build with `ctrl+shift+b` or `shift+cmd+b` (macOS).
* Build and run with `ctrl+r` or `cmd+r` (macOS), or debug with `F5`.
* Switch between the header file and cpp file with `F4`.

### Setup

1. Install the latest Qt Creator.
2. Under chromium/src `gn gen out/Default --ide=qtcreator`.
3. Start it with `qtcreator out/Default/qtcreator_project/all.creator`.
4. Help - Plugins - check ClangCodeModel to enable std completion.

It takes 3 minutes to parse all of chromium's C++ files on my workstation!!! And
it does not block while parsing.

#### Code Style

1. Help - About Plugins (or app menu on macOS), enable Beautifier.
2. Tools - Options (Preferences on macOS) - Beautifier
   Make sure to tick - Enable auto format on file save"
   Select ClangFormat as the tool
   Go to Clang Format tab
   Change the Clang format command to: `$chromium_checkout_dir/src/buildtools/$os/clang-format`, and
   set `Use predefined style: file`. You can also set a keyboard shortcut
   for it.
3. Tools - Options - C++ - Code Style, import this xml file.

```
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE QtCreatorCodeStyle>
<!-- Written by QtCreator 4.2.1, 2017-02-08T19:07:34. -->
<qtcreator>
 <data>
  <variable>CodeStyleData</variable>
  <valuemap type="QVariantMap">
   <value type="bool" key="AlignAssignments">true</value>
   <value type="bool" key="AutoSpacesForTabs">false</value>
   <value type="bool" key="BindStarToIdentifier">false</value>
   <value type="bool" key="BindStarToLeftSpecifier">false</value>
   <value type="bool" key="BindStarToRightSpecifier">false</value>
   <value type="bool" key="BindStarToTypeName">true</value>
   <value type="bool"
     key="ExtraPaddingForConditionsIfConfusingAlign">true</value>
   <value type="bool" key="IndentAccessSpecifiers">true</value>
   <value type="bool" key="IndentBlockBody">true</value>
   <value type="bool" key="IndentBlockBraces">false</value>
   <value type="bool" key="IndentBlocksRelativeToSwitchLabels">false</value>
   <value type="bool" key="IndentClassBraces">false</value>
   <value type="bool" key="IndentControlFlowRelativeToSwitchLabels">true</value>
   <value type="bool"
     key="IndentDeclarationsRelativeToAccessSpecifiers">false</value>
   <value type="bool" key="IndentEnumBraces">false</value>
   <value type="bool" key="IndentFunctionBody">true</value>
   <value type="bool" key="IndentFunctionBraces">false</value>
   <value type="bool" key="IndentNamespaceBody">false</value>
   <value type="bool" key="IndentNamespaceBraces">false</value>
   <value type="int" key="IndentSize">2</value>
   <value type="bool" key="IndentStatementsRelativeToSwitchLabels">true</value>
   <value type="bool" key="IndentSwitchLabels">false</value>
   <value type="int" key="PaddingMode">2</value>
   <value type="bool" key="ShortGetterName">true</value>
   <value type="bool" key="SpacesForTabs">true</value>
   <value type="int" key="TabSize">2</value>
  </valuemap>
 </data>
 <data>
  <variable>DisplayName</variable>
  <value type="QString">chrome</value>
 </data>
</qtcreator>
```

#### Build & Run

1. (Optional) Enable the issues pane for easy navigation to the location of
   compiler errors. qtcreator expects to find compiler errors on stderr, but
   ninja forwards all subcommand output to stdout. So use the following wrapper
   script to forward it to stderr instead.
```
#!/bin/sh
/path/to/depot_tools/ninja "$@" >&2
```
2. In the left panel - Projects, set up the ninja command in the build and
   clean steps, and add the path to chrome in the run configuration.


## Debugger

**You can skip the project settings and use QtCreator as a single file
standalone GDB or LLDB (macOS) frontend.**

For macOS :
1. Open the file you want to debug.
2. Debug - Start Debugging - Attach to running Application, you may need to
   open chromium's task manager to find the process id.

For Linux :
1. Tools - Options - Build & Run - Debuggers, make sure GDB is set.
2. Tools - Options - Kits, change the Desktop kit to GDB (LLDB doesn't work on
   Linux).
3. Open the file you want to debug.
4. Debug - Start Debugging - Attach to running Application, you may need to
   open chromium's task manager to find the process id.

### Tips, tricks, and troubleshooting

#### [Linux] The debugger exits immediately

Ensure yama allows you to attach to another process:

```
$ echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
```


#### [Linux] The debugger does not stop on breakpoints

Ensure you are using GDB on Linux, not LLDB.

#### More

Linux :
See
https://chromium.googlesource.com/chromium/src/+/main/docs/linux/debugging.md

macOS :
https://chromium.googlesource.com/chromium/src/+/main/docs/mac/debugging.md
