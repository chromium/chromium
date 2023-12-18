# Fuzzing browsertests

Fuzzing is effective if either:

* it's guided by code coverage, and can execute incredible numbers of test cases
  per second to explore the codebase (thousands); or
* it has a smart mutator of some kind (out of scope here).

If you have an API to be fuzzed, make a simple libfuzzer fuzzer for just that
API, to get the speed required to explore its attack surface. If however we want
to fuzz a larger, more complex set of Chromium code, we usually need an entire
browser process environment around us. The browser process takes seconds to
start, preventing coverage guided fuzzing from being effective.

We now have an experimental 'in process fuzz test' framework which attempts to:
* Start the browser process _once_
* Execute lots of fuzz cases in that pre-existing browser.
This _may_ amortize the start up cost sufficiently to make such coverage-guided
fuzzing plausible. We don't yet know. But this document shows how to use it,
just in case.

# Writing an in process fuzz case

* Use the template `chrome/test/fuzzing/in_process_fuzzer.gni`
* Provide a source code file which inherits from `InProcessFuzzer`. This
  must override the `Fuzz` method. You'll find that your base class inherits
  from the full browser test infrastructure, so you can do anything you'd
  do in a normal Chrome browser test.
* In your `cc` file, also use the macro `REGISTER_IN_PROCESS_FUZZER` to
  declare that this is the one and only such fuzzer in your executable.

# Running such an in process fuzz case

These cases can be run either with libfuzzer or centipede.

For libfuzzer, provide gn arguments `use_sanitizer_coverage = true`,
`use_libfuzzer = true`, `is_component_build = false` and `is_asan = true`
(other permutations may work).

This will give you a single binary you can run like this:
`my_fuzzer /tmp/corpus -rss_limit_mb=81920`

However, you'll more likely want to use
[centipede](https://github.com/google/centipede) which has an
out-of-process co-ordinator.

To use centipede, specify `use_centipede = true` instead of `use_libfuzzer =
true`. You should also build the `centipede` target as well as your fuzzer.
You'll then want to run centipede using some command like:

```
mkdir wd && ASAN_OPTIONS=detect_odr_violation=0 out/ASAN/centipede --binary=out/ASAN/html_in_process_fuzz_tests --workdir=wd --shmem_size_mb 4096 --rss_limit_mb 0  --batch_size 100 --log_features_shards 2 --exit_on_crash 1
```
