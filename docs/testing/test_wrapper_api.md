# Test Wrapper API

In order to simplify the calling conventions for tests that we run on our
continuous integration system, we require them to follow a simple API
convention. For a given GN label //X:Y and build directory //Z in a checkout
(e.g., //url:url_unittests and //out/Release), we expect there to be:

    * A file `$Z/$Y.runtime_deps` containing a list of files needed to run
      the test (in the format produced by `gn desc //Z //X:Y runtime_deps`,
      which is a newline-separated list of paths relative to Z)
    * An executable file `$Z/bin/run_$Y` which does everything needed to set
      up and run the test with all of the appropriate flags. This will usually
      be a vpython script.
    * (on Windows) A file `$Z/bin/run_$Y.bat` file that will turn around
      and invoke the corresponding run_$ vpython script.

If you create a directory snapshot with the files listed in the .runtime_deps
file, cd to $Z, and run bin/run_$Y, then the test should run to completion
successfully.

The script file MUST honor the `GTEST_SHARD_INDEX` and `GTEST_TOTAL_SHARDS`
environment variables as documented in
[the Test Executable API](test_executable_api.md) and SHOULD conform to
the Test Executable API in other respects (i.e., honor the
`--isolated-script-test-filter` arg and other command line flags specified
in that API).

TODO(crbug.com/40564748): Convert everything to the Test Executable API, and
change the above SHOULD to a MUST.
