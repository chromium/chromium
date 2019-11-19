# The Clang Static Analyzer

The Clang C/C++ compiler comes with a static analyzer which can be used to find
bugs using path sensitive analysis. Path sensitive analysis is
a technique that explores all the possible branches in code and
records the codepaths that might lead to bad or undefined behavior,
like an uninitialized reads, use after frees, pointer leaks, and so on.

See the [official Clang static analyzer page](http://clang-analyzer.llvm.org/)
for more background information.

We used to have a bot that continuously ran with the static analyzer,
but people used to not look at it much.

The static analyzer can still be invoked with [clang-tidy](clang_tidy.md).

## Recommended checks
Clang's static analyzer comes with a wide variety of checkers. Some of the
checks aren't useful because they are intended for different languages,
platforms, or coding conventions than the ones used for Chromium development.

Checkers we found useful were:

    -analyzer-checker=core
    -analyzer-checker=cpp
    -analyzer-checker=unix
    -analyzer-checker=deadcode

As of this writing, the checker suites we support are
[core](https://clang-analyzer.llvm.org/available_checks.html#core_checkers),
[cplusplus](https://clang-analyzer.llvm.org/available_checks.html#cplusplus_checkers), and
[deadcode](https://clang-analyzer.llvm.org/available_checks.html#deadcode_checkers).

## Addressing false positives

Some of the errors you encounter will be false positives, which occurs when the
static analyzer naively follows codepaths which are practically impossible to
hit at runtime. Fortunately, we have a tool at our disposal for guiding the
analyzer away from impossible codepaths: assertion handlers like
DCHECK/CHECK/LOG(FATAL).  The analyzer won't check the codepaths which we
assert are unreachable.

An example would be that if the analyzer detected the function argument
`*my_ptr` might be null and dereferencing it would potentially segfault, you
would see the error `warning: Dereference of null pointer (loaded from variable
'my_ptr')`.  If you know for a fact that my_ptr will not be null in practice,
then you can place an assert at the top of the function: `DCHECK(my_ptr)`. The
analyzer will no longer generate the warning.

Be mindful about only specifying assertions which are factually correct! Don't
DCHECK recklessly just to quiet down the analyzer. :)

Other types of false positives and their suppressions:
* Unreachable code paths. To suppress, add the `ANALYZER_SKIP_THIS_PATH();`
  directive to the relevant code block.
* Dead stores. To suppress, use the macro
  `ANALYZER_ALLOW_UNUSED(my_var)`. This also suppresses dead store warnings
  on conventional builds without static analysis enabled!

See the definitions of the `ANALYZER_*` macros in base/logging.h for more
detailed information about how the annotations are implemented.

## Logging bugs

If you find any issues with the static analyzer, or find Chromium code behaving
badly with the analyzer, please check the `Infra>CodeAnalysis` CrBug component
to look for known issues, or file a bug if it is a new problem.
