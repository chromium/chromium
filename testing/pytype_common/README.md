This directory contains Python code for running pytype (a Python 3 type hinting
analyzer) on Chromium Python code. pytype can infer types from un-annotated
code, so it is not necessary to have type hinting added for all analyzed code
and its dependencies. However, it is still recommended to do so eventually to
ensure that the type hinting is as accurate as possible and to ensure that
humans have up-to-date information about what functions take and return.


To run pytype, simply import `pytype_runner.py` and call its `run_pytype`
function with the correct arguments as specified in its docstring.

It is recommended to NOT run this as part of presubmit, as depending on how many
dependencies your code has, it can end up analyzing many files and taking
multiple minutes. This time goes down dramatically once pytype has a cache
built, but there is currently no way to ensure bots have a warm cache, so it
should not be relied on.
