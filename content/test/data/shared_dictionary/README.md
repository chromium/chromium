# Shared Dictionary Tests

- This directory contains files for shared dictionary related browser tests.

- `brotli` command can be installed from https://github.com/google/brotli.

- `path/compressed.data` is created using the following command.

  ```bash
  $ echo -en '\xffDCB' > path/compressed.data
  $ openssl dgst -sha256 -binary test.dict >> path/compressed.data
  $ echo -n 'This is compressed test data using a test dictionary' | \
    brotli -D test.dict >> path/compressed.data
  ```
