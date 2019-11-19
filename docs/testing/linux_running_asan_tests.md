# Running Chrome tests with AddressSanitizer (asan) and LeakSanitizer (lsan)

Running asan/lsan tests requires changing the build and setting a few
environment variables.

Changes to args.gn (ie, `out/Release/args.gn`):

```python
is_asan = true
is_lsan = true
```

Setting up environment variables and running the test:

```sh
$ export ASAN_OPTIONS="symbolize=1 external_symbolizer_path=./third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer detect_leaks=1 detect_odr_violation=0"
$ export LSAN_OPTIONS=""
$ out/Release/browser_tests
```

Stack traces (such as those emitted by `base::debug::StackTrace().Print()`) may
not be fully symbolized. The following snippet can symbolize them:

```sh
$ out/Release/browser_tests 2>&1 | ./tools/valgrind/asan/asan_symbolize.py
```
