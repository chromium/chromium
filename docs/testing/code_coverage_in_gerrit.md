# Code Coverage in Gerrit

Tests are critical because they find bugs and regressions, enforce better
designs and make code easier to maintain. **Code coverage helps you ensure your
tests are thorough**.

Chromium CLs can show a line-by-line breakdown of test coverage. **You can use
it to ensure you only submit well-tested code**.

To see code coverage for a Chromium CL, **trigger a CQ dry run**, and once the
builds finish and code coverage data is processed successfully, **look at the
change view to see absolute and incremental code coverage percentages**:

![code_coverage_percentages]

Absolute coverage percentage is the percentage of lines covered by tests
out of **all the lines** in the file, while incremental coverage percentage only
accounts for **newly added or modified lines**.

To further dig into specific lines that are not covered by tests, **look at the
right column of the side by side diff view**:

![code_coverage_annotations]

**Code coverage data is shared between patchsets that are commit-message-edit or
trivial-rebase away**, however, if a newly uploaded patchset has
non-trivial code change, a new CQ dry run must be triggered before coverage data
shows up again.

The code coverage tool currently supports:
* C/C++ code for [Chromium on Linux].
* C/C++ code for [Chromium on Chromium OS].

support for more platforms and more languages is in progress.

## Contacts

### Reporting problems
For any breakage report and feature requests, please [file a bug].

### Mailing list
For questions and general discussions, please join [code-coverage group].

## FAQ
### Why is coverage not shown even though the try job finished successfully?

There are several possible reasons:
* A particular source file/test may not be available on a particular project or
platform. As of now, only `chromium/src` project and `Linux` platform is
supported.
* There is a bug in the pipeline. Please [file a bug] to report the breakage.

### How does it work?

Please refer to [code_coverage.md] for how code coverage works in Chromium in
general, and specifically, for per-CL coverage in Gerrit, the
[clang_code_coverage_wrapper] is used to compile and instrument ONLY the source
files that are affected by the CL for the sake of performance and a
[chromium-coverage Gerrit plugin] is used to display code coverage information
in Gerrit.


[choose_tryjobs]: images/code_coverage_choose_tryjobs.png
[linux_coverage_rel]: images/code_coverage_linux_coverage_rel.png
[code_coverage_annotations]: images/code_coverage_annotations.png
[code_coverage_percentages]: images/code_coverage_percentages.png
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ETest%3ECodeCoverage
[code-coverage group]: https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage
[code_coverage.md]: code_coverage.md
[clang_code_coverage_wrapper]: clang_code_coverage_wrapper.md
[chromium-coverage Gerrit plugin]: https://chromium.googlesource.com/infra/gerrit-plugins/code-coverage/
[Chromium on Chromium OS]: https://chromium.googlesource.com/chromium/src/+/master/docs/chromeos_build_instructions.md
[Chromium on Linux]: https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md
