# Chromium Python style guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)._

Chromium follows [PEP-8](https://www.python.org/dev/peps/pep-0008/) unless an
exception is listed below.

See also the [Chromium OS Python Style
Guidelines](https://sites.google.com/a/chromium.org/dev/chromium-os/python-style-guidelines).

You can propose changes to this style guide by sending an email to
`python@chromium.org`. Ideally, the list will arrive at some consensus and you
can request review for a change to this file. If there's no consensus,
[`//styleguide/python/OWNERS`](https://chromium.googlesource.com/chromium/src/+/master/styleguide/python/OWNERS)
get to decide.

Blink code in `third_party/blink` uses [Blink style](blink-python.md).

[TOC]

## Differences from PEP-8

* Use two-space indentation instead of four-space indentation.
* Use `CamelCase()` method and function names instead of `unix_hacker_style()`
  names.

(The rationale for these is mostly legacy: the code was originally written
following Google's internal style guideline, the cost of updating all of the
code to PEP-8 compliance was not small, and consistency was seen to be a
greater virtue than compliance.)

## Tools

### pylint
[Depot tools](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
contains a local copy of pylint, appropriately configured.
* Directories need to opt into pylint presumbit checks via:
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
based_on_style = chromium
```

Entire files can be formatted (rather than just touched lines) via:
```sh
git cl format --python --full
```

#### Bugs
* Are tracked here: https://github.com/google/yapf/issues.
* For Chromium-specific bugs, please discuss on `python@chromium.org`.

#### Editor Integration
See: https://github.com/google/yapf/tree/master/plugins
