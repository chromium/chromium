web_tests/FlagExpectations stores flag-specific test expectations.
To run layout tests with a flag passed to content_shell, use:

  run_web_tests.py --additional-driver-flag=--name-of-flag

Create a new file:

  FlagExpectations/name-of-flag

The entries in the file is the same as the main TestExpectations file, e.g.
  crbug.com/123456 path/to/your/test.html [ Expectation ]

This file will override the main TestExpectations file when the above command
is run.

If the name-of-flag is too long, or when multiple additional flags are needed,
you can add an entry in web_tests/FlagSpecificConfig, like
  {
    "name": "short-name",
    "args": ["--name-of-flag1", "--name-of-flag2"]
  }

And create a new file in the same format of the above
FlagExpectations/name-of-flag file:

  FlagExpectations/short-name
