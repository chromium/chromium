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
accounts for **newly added or modified lines**. Both these coverage metrics, are further
classified into Unit Tests coverage(coverage from just unit tests) and All Tests coverage(covererd by all tests running in CQ, including unit tests). All Tests coverage is a superset of Unit Tests coverage.

To further dig into specific lines that are not covered by tests, **look at the
right column of the side by side diff view, and specifically notice the
background color of each line number**, where a light orange color indicates
missing coverage and a light blue color indicates existing coverage. Moreover
hovering over the line number shows an informative tooltip with
"Not covered by tests" or "Covered by tests" text respectively. It only shows
All Tests Coverage right now

![code_coverage_annotations]

**Code coverage data is shared between patchsets that are commit-message-edit or
trivial-rebase away**, however, if a newly uploaded patchset has
non-trivial code change, a new CQ dry run must be triggered before coverage data
shows up again.

The code coverage tool supports coverage for C/C++, JAVA and Javascript on all major platforms(Linux, MacOS, Windows, Android, iOS and ChromeOS)

## CLs Blocked Due to Low Coverage
For some teams in Chrome, we have turned on a coverage check, which blocks a CL from submission if the incremental coverage is below a preset threshold(default = 70%). CLs with insufficient test coverage have a `CodeCoverage-1` label added to them, which prevents them from being submitted. Also, a descriptive message is added to the CL, notifying developer of why the CL was blocked, and how to resolve it.
![low_coverage_message]

Once the tests are added, another run of coverage builders (through CQ+1 or CQ+2) changes the label to `CodeCoverage+1`, allowing CLs to proceed with submission.

Tests themselves, as well as test-only files, are generally excluded from coverage checks based on their path or filename. If you are getting coverage warnings for test-related files themselves, check whether the files end in "test" or "tests" (for example, "SomethingTest.java" or "something_unittests.cc") or that their path contains a directory named exactly "test", "tests", or "testing". There is no manual list to which files can be added for long-term exclusion.

Devs can also choose to bypass this block, in case they think they are being unfairly punished. They can do so by adding a *Low-Coverage-Reason: reason* footer to the change description. This should follow certain formatting constraints which are mentioned below

### Mention the Bypass Category

The `reason` string should mention the category the bypass reason belongs to. For e.g. *Low-Coverage-Reason: TRIVIAL_CHANGE This change contains only minor cosmetic changes.* (TRIVIAL_CHANGE is the category)

Available category choices are:
* **TRIVIAL_CHANGE**: CL contains mostly minor changes e.g. renaming, file moves, logging statements, simple interface definitions etc.
* **TESTS_ARE_DISABLED**: Corresponding tests exist, but they are currently disabled.
* **TESTS_IN_SEPARATE_CL**: Developer plan to write tests in a separate CL (Should not be exercised often as per best practices)
* **HARD_TO_TEST**: The code under consideration is hard to test. For e.g. Interfaces with system, real hardware etc.
* **COVERAGE_UNDERREPORTED**: To be used when the developer thinks that tests exist, but corresponding coverage is missing.
* **LARGE_SCALE_REFACTOR**: The current change is part of a large scale refactor. Should explain why the refactor shouldn't have tests.
* **EXPERIMENTAL_CODE**: The current code is experimental and unlikely to be released to users.
* **OTHER**: None of the above categories are the right fit

In case the developer doesn't specify the coverage category as prescribed, a warning will be shown in the UI, with details on how to fix
![impropery_formatted_coverage_footer]

### No empty line after the footer
In order for *Low-Coverage-Reason: reason* to work properly, it should occur after the last empty line in CL description, otherwise gerrit recognizes it as part of the commit message, rather than the footer i.e. Following would not work
![empty_line_after_footer]

Removing the empty line should fix it
![no_empty_line_after_footer]

### Be careful with long footer strings
Either keep the footer message in one line i.e. do not add line breaks; or if you do, add whitespace on new footer lines, otherwise [gerrit doesn’t parse them right]. e.g. a long footer message can be written as
![long_footer]
or
![line_break_footer]

## Contacts

### Reporting problems
For any breakage report and feature requests, please [file a bug].

### Mailing list
For questions and general discussions, please join [code-coverage group].

## FAQ
### Why is coverage not shown even though the try job finished successfully?

There are several possible reasons:
* A particular source file/test may not be available on a particular project or
platform.
* There is a bug in the pipeline. Please [file a bug] to report the breakage.

### How does it work?

Please refer to [code_coverage.md] for how code coverage works in Chromium in
general, and specifically, for per-CL coverage in Gerrit, the
[clang_code_coverage_wrapper] is used to compile and instrument ONLY the source
files that are affected by the CL for the sake of performance and a
[chromium-coverage Gerrit plugin] is used to display code coverage information
in Gerrit.


[choose_tryjobs]: images/code_coverage_choose_tryjobs.png
[code_coverage_annotations]: images/code_coverage_annotations.png
[code_coverage_percentages]: images/code_coverage_percentages.png
[low_coverage_message]: images/low_coverage_message.png
[empty_line_after_footer]: images/empty_line_after_footer.png
[no_empty_line_after_footer]: images/no_empty_line_after_footer.png
[long_footer]: images/long_footer.png
[line_break_footer]: images/line_break_footer.png
[impropery_formatted_coverage_footer]: images/improperly_formatted_coverage_footer.png
[file a bug]: https://bugs.chromium.org/p/chromium/issues/entry?components=Infra%3ETest%3ECodeCoverage
[code-coverage group]: https://groups.google.com/a/chromium.org/forum/#!forum/code-coverage
[code_coverage.md]: code_coverage.md
[clang_code_coverage_wrapper]: https://chromium.googlesource.com/chromium/src/+/main/docs/clang_code_coverage_wrapper.md
[chromium-coverage Gerrit plugin]: https://chromium.googlesource.com/infra/gerrit-plugins/code-coverage/
[gerrit doesn’t parse them right]: https://bugs.chromium.org/p/chromium/issues/detail?id=1459714#c9
