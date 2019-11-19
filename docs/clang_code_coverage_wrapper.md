# The Clang Code Coverage Wrapper

The Clang code coverage wrapper
([//build/toolchain/clang_code_coverage_wrapper.py]) is a compiler wrapper that
adds code coverage flags to the invocations of Clang C/C++ compiler, which can
be used to instrument and generate code coverage data for a subset of the source
files. The main use case of this wrapper is to generate code coverage reports
for changed files at a per-CL level during try jobs.

One might ask: why this compiler wrapper instead of just instrumenting all the
source files?

Efficiency is the main consideration because comparing to instrument everything,
only instrument what-is-needed takes less time to build the test targets,
produces binaries that are 10 times smaller, run tests twice faster and
generates coverage reports significantly faster. An experiment was done to
evaluate the effectiveness of instrument what-is-needed, and in the experiment,
`unit_tests` is used as the sample test target and 8 randomly chosen C++ source
files are used as the samples files to instrument. The results are presented in
the following table:

| Type               | Build time | Binary size | Tests run time | Reports generation time |
|--------------------|------------|-------------|----------------|-------------------------|
| No instrumentation | 43m        | 240M        | 40s            | 0s                      |
| What-is-needed     | 43m        | 241M        | 40s            | 0.14s                   |
| Everything         | 53m        | 2.3G        | 80s            | 1m10s                   |

It can be seen from the results that the overhead introduced by instrument
everything is huge, while it's negligible for instrument what-is-needed.

## How to use the coverage wrapper?
To get the coverage wrapper hook into your build, add the following flags to
your `args.gn` file, assuming build directory is `out/Release`.

```
use_clang_coverage = true
coverage_instrumentation_input_file = "//out/Release/coverage_instrumentation_input.txt"
```

The path to the coverage instrumentation input file should be a source root
absolute path, and the file consists of multiple lines where each line
represents a path to a source file, and the specified paths must be relative to
the root build directory. e.g. `../../base/task/post_task.cc` for build
directory `out/Release`.

Then, use the [//tools/code_coverage/coverage.py] script to build, run tests and
generate code coverage reports for the exercised files.

## Caveats
One caveat with this compiler wrapper is that it may introduce unexpected
behaviors in incremental builds when the file path to the coverage
instrumentation input file changes between consecutive runs, and the reason is
that the coverage instrumentation input file is explicitly passed to the
coverage wrapper as a command-line argument, which makes the path part of the
Ninja commands used to compile the source files, so change of the path between
consecutive runs causes Ninja to perform a full rebuild.

Due to the reasons mentioned above, users of this script are strongly advised to
always use the same path such as
`${root_build_dir}/coverage_instrumentation_input.txt`.

## Want to learn more about code coverage in Chromium?
For more background information on how code coverage works in Chromium, please
refer to [code_coverage.md]

## Contacts

### Reporting problems
We're still evaluating the tools, for any breakage report and feature requests,
please [file a bug].

### Mailing list
For questions and general discussions, please join [code-coverage group].

[//build/toolchain/clang_code_coverage_wrapper.py]: ../build/toolchain/clang_code_coverage_wrapper.py
[code_coverage.md]: testing/code_coverage.md
[//tools/code_coverage/coverage.py]: ../tools/code_coverage/coverage.py
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ETest%3ECodeCoverage
[code-coverage group]: https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage
