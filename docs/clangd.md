# Clangd

## Introduction

[clangd](https://clang.llvm.org/extra/clangd/) is a clang-based [language server](http://langserver.org/).
It brings IDE features (e.g. diagnostics, code completion, code navigations) to
your editor.

## Getting clangd

See [instructions](https://clang.llvm.org/extra/clangd/Installation.html#installing-clangd).

**Googlers:** clangd has been installed on your glinux by default, just use
`/usr/bin/clangd`.

Alternative: use the following command to build clangd from LLVM source, and you
will get the binary at
`out/Release/tools/clang/third_party/llvm/build/bin/clangd`.

```
tools/clang/scripts/build_clang_tools_extra.py --fetch out/Release clangd
```

## Setting Up

1. Make sure generated ninja files are up-to-date.

```
gn gen out/Release
```

2. Generate the compilation database, clangd needs it to know how to build a
source file.

```
tools/clang/scripts/generate_compdb.py -p out/Release > compile_commands.json
```

Note: the compilation database is not re-generated automatically, you'd need to
regenerate it manually when you have new files checked in.

3. Optional: build chrome normally. This ensures generated headers exist and are
up-to-date. clangd will still work without this step, but it may give errors or
inaccurate results for files which depend on generated headers.

```
ninja -C out/Release chrome
```

4. Use clangd in your favourite editor, see detailed [instructions](
https://clang.llvm.org/extra/clangd/Installation.html#getting-started-with-clangd).

## Index

By default, clangd only knows the files you are currently editing. To provide
project-wide code navigations (e.g. find references), clangd neesds a
project-wide index.

You can pass an **experimental** `--background-index` command line argument to
clangd, clangd will incrementally build an index of Chromium in the background.
Note: the first index time may take hours (for reference, it took 2~3 hours on
a 48-core, 64GB machine).

A full index of Chromium (including v8, blink) takes ~550 MB disk space and ~2.7
GB memory in clangd.

## Questions

If you have any questions, reach out to clangd-dev@lists.llvm.org.
