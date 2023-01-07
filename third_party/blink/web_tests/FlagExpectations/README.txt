web_tests/FlagExpectations stores flag-specific test expectations.
Please see [1] for details, and see [2] to see if a flag-specific test
configuration is suitable for you.

To run layout tests with a flag passed to content_shell, use:

  run_web_tests.py --flag-specific=config-name

It requires web_tests/FlagSpecificConfig to contain an entry for "config-name",
e.g.:
  {
    "name": "config-name",
    "args": ["--flag1", "--flag2"]
  }

run_web_tests.py will pass "--flag1 --flag2" to content_shell.

You can create a new file:

  FlagExpectations/config-name

The entries in the file is the same as the main TestExpectations file, e.g.
  crbug.com/123456 path/to/your/test.html [ Expectation ]

This file will override the main TestExpectations file when the above command
is run.

[1] https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md#flag_specific-or-additional_driver_flag
[2] https://chromium.googlesource.com/chromium/src/+/main/docs/testing/web_tests.md#Choosing-between-flag_specific-and-virtual-test-suite
