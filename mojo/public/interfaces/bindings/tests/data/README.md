# Mojo conformance test data
The files in [validations](validations) are test data for mojo conformance
tests, which validate the memory layout of mojo messages.

Run validation tests using the following command:

```
autoninja -C out/Default mojo_unittests
out/Default/bin/run_mojo_unittests --gtest_filter=ValidationTest.*
```

Note that you do not need to rebuild after changing the test data.

## Adding test data

1. Add a test method if necessary in
   [validation test interfaces](../validation_test_interfaces.mojom).
1. Add a .data and .expected file in [validations](validations).
   * The syntax for the .data file can be found in the
     [validation test input parser](/mojo/public/cpp/bindings/tests/validation_test_input_parser.h).
1. Run `ValidationTest.*` to ensure that tests work as expected.
1. Update [validation_data_files.gni](../validation_data_files.gni) with your
   test files.
1. Use [python script](/build/ios/update_bundle_filelist.py) to update
   [validation_unittest_bundle_data.filelist](../validation_unittest_bundle_data.filelist).
   * The presubmit will give you a copy pastable command, so there is no need to
     to figure out how to invoke the script.
