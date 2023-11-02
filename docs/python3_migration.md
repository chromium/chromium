# Migrating Chromium to Python3

This page describes the current status and how to migrate code.

[crbug.com/941669](https://crbug.com/941669) tracks the overall migration.

*It is now safe to write new code using Python3 as long as it isn't called
from other Python2 files. See the
[Python style guide](../styleguide/python/python.md) for the latest on this.*

## Status

As of the time of writing (2021-07-19), we're in the following state:

* depot_tools is fully Python3-compatible.
* [gclient hooks](#gclient-hooks) are being migrated to use Python3
  ([crbug.com/1208028](https://crbug.com/1208028)).
* GN is fully Python3-compatible, meaning that all the scripts invoked
  through exec_script() are using Python3.
* The [build](#gn_ninja-actions) (scripts invoked by Ninja) uses Python3.
* We are updating the various test harnesses and frameworks to use
  Python3, but most still use Python2. It is possible to use
  Python3 for tests if you're ready to do so
  ([crbug.com/1275016](https://crbug.com/1275016)).
* [PRESUBMIT checks](#presubmit-checks) are being migrated to use Python3
  ([crbug.com/1207012](https://crbug.com/1207012)).
* Python3-compatible pylint checks are available
  ([crbug.com/1207012](https://crbug.com/1207012)).


## Migrating Python code from 2 to 3

This document will not attempt to replicate the general information in
the [Python.org Porting HOWTO](https://docs.python.org/3/howto/pyporting.html)
or the many, many other guides on the Internet.

However, here are a few notes that may be helpful in a Chromium context:

* Most of our Python code is pretty straightforward and moves easily
  from Python2 to Python3, so don't stress out about this!

* When migrating code, please make the Right Changes, rather than the
  minimum needed to get things to work. For example, make sure your code
  is clear about whether something should be a byte string or a unicode
  string, rather than trying to handle both "to be compatible".
  (If you really do need the code to handle both, though, that can be okay.)

* Do not assume you can use [vpython] regardless of context! vpython has
  performance issues in some situations and so we don't want to use it yet for
  things invoked by gclient, PRESUBMITs, or Ninja. However, all tests are run
  under vpython and so you can assume it there.

* Some people find the `2to3` tool to be useful to partially or
  completely automate the migration of existing files, and the
  `six` module to shim compatibility across the two. The `six` module
  is available in vpython and in `//third_party/six/src/six.py`.

* shebang lines mostly don't matter. A "shebang line" is the line at the
  beginning of many unix scripts, like `#!/usr/bin/env python`. They are
  not that important for us because most of our python invocations come
  from tools like Ninja or Swarming and invoke python directly. So, while
  you should keep them accurate where they are useful, we can't rely
  on them to tell which code has been migrated and which hasn't.

* The major gotchas for us tend to have to do with processing output
  from a subprocess (e.g., `subprocess.check_output()`). By default
  output is returned as a binary string, so get in the habit of calling
  `.check_output().decode('utf-8')` instead. This is compatible across
  2 and 3. 'utf-8' is the default in Python3 (ASCII was the default in
  Python2), but being explicitly is probably a good idea until we have
  migrated everything.

* Be aware that `filter` and `map` return generators in Python3, which
  are one-shot objects. If you reference them inside another loop, e.g.,

      foo = [ ... ]
      bar = filter(some_function, [ ...])
      for x in foo:
          for y in bar:
              do_something(x, y)

  this won't work right, because on the second and subsequent iterations,
  bar will be an empty list.

  Best practice is to use a list comprehension instead of `map` or `filter`,
  but you can also explicitly cast the results of map or filter to a list
  if the list comprehension is too awkward.

* Some modules (like `urllib2`) were renamed and/or moved around in Python 3.
  A Google search will usually quickly tell you the new location.

* Watch out for places where you reference `basestring` or `unicode` directly.
  `six` provides some compatibility types to help here.

## Testing your migrations

Generally speaking, test your changes the same way we do everything else:
make a change locally, and rely on the CQ and CI bots to catch problems.

However, here are some specific guidelines for the different contexts
where we use Python:

### gclient hooks

To switch a gclient hook from Python2 to Python3, simply change `python`
to `python3` in the DEPS file, and make sure things still run :).

### GN/Ninja actions

All targets in the build use Python3 now, and anything declared via an
`action()` rule in GN will use Python3.

### Tests

Test targets that run by invoking python scripts (like telemetry_unittests
or blink_web_tests) should eventually migrate to using the [script_test]
GN templates. Once you do that, they will use Python3 by default.

Some tests still need to be migrated to `script_test()`
([crbug.com/1208648](https://crbug.com/1208648)). The process for
doing that is not yet well documented, so ask on python@chromium.org (or
ask dpranke@ directly).

There is no general mechanism for migrating tests that are C++ executables
that launch python via subprocesses, so you're on your own for dealing with
that.

### Presubmit checks

Presubmit checks are run using Python 2 by default. To run them using
Python3, add the line `USE_PYTHON3 = True` to the PRESUBMIT.py file in
question (effectively creating a global variable).

[script_test]: https://source.chromium.org/?q=script_test%20file:testing%2Ftest.gni&ss=chromium
[vpython]: https://chromium.googlesource.com/infra/infra/+/refs/heads/main/doc/users/vpython.md
