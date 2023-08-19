# Shared Dictionary Tests

- This directory contains files for shared dictionary related browser tests.

- `brotli` command can be installed from https://github.com/google/brotli.

- `path/compressed.data` is created using the following command.

  ```bash
  $ echo -n 'This is compressed test data using a test dictionary' | \
    brotli -D test.dict > path/compressed.data
  ```
