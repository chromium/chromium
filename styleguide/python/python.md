# Chromium Python Style Guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)._

We currently require Python 3.8 (as in, that's what the bots use, so don't
assume or require anything older *or* newer), but most newer versions of
Python3 will work fine for most things. There is an appropriate version
of Python3 in `$depot_tools/python-bin`, if you don't have one already.

We (often) use a tool called [vpython] to manage Python packages; vpython
is a wrapper around virtualenv. However, it is not safe to use vpython
regardless of context, as it can have performance issues. All tests are
run under vpython, so it is safe there, and vpython is the default for
running scripts during PRESUBMIT checks (input_api.python3_executable points to
vpython3 and is used in GetPythonUnitTests), but you should not use vpython
during gclient runhooks, or during the build unless a
[//build/OWNER](../../build/OWNERS) has told you that it is okay to do so.

Also, there is some performance overhead to using vpython, so prefer not
to use vpython unless you need it (to pick up packages not available in the
source tree).

"shebang lines" (the first line of many unix scripts, like `#!/bin/sh`)
aren't as useful as you might think in Chromium, because
most of our python invocations come from other tools like Ninja or
the swarming infrastructure, and they also don't work on Windows.
So, don't expect them to help you. That said, a python 3 shebang is one way to
indicate to the presubmit system that test scripts should be run under Python 3
rather than Python 2.

However, if your script is executable, you should still use one, and for
Python you should use `#!/usr/bin/env python3` or `#!/usr/bin/env vpython3`
in order to pick up the right version of Python from your $PATH rather than
assuming you want the version in `/usr/bin`; this allows you to pick up the
versions we endorse from
`depot_tools`.

Chromium follows [PEP-8](https://www.python.org/dev/peps/pep-0008/).

It is also encouraged to follow advice from
[Google's Python Style Guide](https://google.github.io/styleguide/pyguide.html),
which is a superset of PEP-8.

See also:
* [ChromiumOS Python Style Guide](https://chromium.googlesource.com/chromiumos/docs/+/HEAD/styleguide/python.md)
* [Blink Python Style Guide](blink-python.md)

[TOC]

## Our Previous Python Style

Chromium used to differ from PEP-8 in the following ways:
* Use two-space indentation instead of four-space indentation.
* Use `CamelCase()` method and function names instead of `unix_hacker_style()`
  names.
* 80 character line limits rather than 79.

New scripts should not follow these deviations, but they should be followed when
making changes to files that follow them.

## Making Style Guide Changes

You can propose changes to this style guide by sending an email to
[`python@chromium.org`]. Ideally, the list will arrive at some consensus and you
can request review for a change to this file. If there's no consensus,
[`//styleguide/python/OWNERS`](https://chromium.googlesource.com/chromium/src/+/main/styleguide/python/OWNERS)
get to decide.

## Portability

There are a couple of differences in how text files are handled on Windows that
can lead to portability problems. These differences are:

* The default encoding when reading/writing text files is cp1252 on Windows and
utf-8 on Linux, which can lead to Windows-only test failures. These can be
avoided by always specifying `encoding='utf-8'` when opening text files.

* The default behavior when writing text files on Windows is to emit \r\n
(carriage return line feed) line endings. This can lead to cryptic Windows-only
test failures and is generally undesirable. This can be avoided by always
specifying `newline=''` when opening text files for writing.

That is, use these forms when opening text files in Python:

* reading: with open(filename, 'r', encoding='utf-8') as f:
* writing: with open(filename, 'w', encoding='utf-8', newline='') as f:

## Tools

### pylint
[Depot tools](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
contains a local copy of pylint, appropriately configured.
* Directories need to opt into pylint presubmit checks via:
   `input_api.canned_checks.RunPylint()`.

### YAPF
[YAPF](https://github.com/google/yapf) is the Python formatter used by:

```sh
git cl format --python
```

Directories can opt into enforcing auto-formatting by adding a `.style.yapf`
file with the following contents:
```
[style]
based_on_style = pep8
```

Entire files can be formatted (rather than just touched lines) via:
```sh
git cl format --python --full
```

YAPF has gotchas. You should review its changes before submitting. Notably:
 * It does not re-wrap comments.
 * It won't insert characters in order wrap lines. You might need to add ()s
   yourself in order to have to wrap long lines for you.
 * It formats lists differently depending on whether or not they end with a
   trailing comma.


#### Bugs
* Are tracked here: https://github.com/google/yapf/issues.
* For Chromium-specific bugs, please discuss on [`python@chromium.org`].

#### Editor Integration
See: https://github.com/google/yapf/tree/main/plugins

[vpython]: https://chromium.googlesource.com/infra/infra/+/refs/heads/main/doc/users/vpython.md
[`python@chromium.org`]: https://groups.google.com/a/chromium.org/g/python
