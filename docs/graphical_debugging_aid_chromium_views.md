# Graphical Debugging Aid for Chromium Views

## Introduction

A simple debugging tool exists to help visualize the views tree during
debugging. It consists of 4 components:

1.  The function `views::PrintViewGraph()` (in the file
    `ui/views/debug_utils.h`),
1.  a custom debugger command
  - For GDB, see
    [gdbinit](https://chromium.googlesource.com/chromium/src/+/main/docs/gdbinit.md),
  - For LLDB, use `tools/lldb/lldb_viewg.py`
  - For other debuggers, it should be relatively easy to adapt the
    above scripts.
1.  the graphViz package (http://www.graphviz.org/ - downloadable for Linux,
    Windows and Mac), and
1.  an SVG viewer (_e.g._ Chrome).

## Details

To use the tool,

1.  Make sure you have 'dot' installed (part of graphViz),
1.  run gdb/lldb on your build and
    1. For GDB see
    [gdbinit](https://chromium.googlesource.com/chromium/src/+/main/docs/gdbinit.md),
    1. For LLDB `command script import tools/lldb/lldb_viewg.py` (this can
    be done automatically in `.lldbinit`),
1.  stop at any breakpoint inside class `View` (or any derived class), and
1.  type `viewg` at the gdb prompt.

This will cause the current view, and any descendants, to be described in a
graph which is stored as `~/state.svg` (Windows users may need to modify the
script slightly to run under CygWin). If `state.svg` is kept open in a browser
window and refreshed each time `viewg` is run, then it provides a graphical
representation of the state of the views hierarchy that is always up to date.

It is easy to modify the gdb script to generate PDF in case viewing with evince
(or other PDF viewer) is preferred.

If you don't use gdb or lldb, you may be able to adapt the script to work with
your favorite debugger. The gdb script invokes

    views::PrintViewGraph(this)

on the current object, returning `std::string`, whose contents must then be
saved to a file in order to be processed by dot.
