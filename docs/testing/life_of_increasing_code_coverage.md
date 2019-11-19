# Life of Increasing Code Coverage

The goal of this doc is to provide guidance on how to write better tests using
code coverage information rather than increase code coverage itself.

1. Pay attention to **untested** code in both the
[coverage dashboard](https://analysis.chromium.org/p/chromium/coverage) and
[code coverage in Gerrit](code_coverage_in_gerrit.md) during code review.

2. Is this dead code? If yes, draft a CL
([example](https://chromium-review.googlesource.com/c/chromium/src/+/1550769))
to remove it, otherwise, please go to step 3.

3. Think about why the code is not covered by any test. Is it because it's
too complicated to be testable? If yes, draft a CL to refactor the code and add
tests ([example](https://chromium-review.googlesource.com/c/chromium/src/+/1558233)),
otherwise, please go to step 4.

4. If the code is testable, but a test was forgotten, draft a CL to add
tests for it ([example](https://chromium-review.googlesource.com/c/chromium/src/+/1447030)).

Anytime you upload a CL to refactor or add tests, you can use
[code coverage in Gerrit](code_coverage_in_gerrit.md) to help you verify the
previously untested code is now tested by your CL.

Please refer to [code_coverage.md](code_coverage.md) for how code coverage works
in Chromium in general.

### Contacts

For any breakage report and feature requests, please
[file a bug](https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ETest%3ECodeCoverage).

For questions and general discussions, please join
[code-coverage group](https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage).
