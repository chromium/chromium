# Trybot: android-binary-size

[TOC]

## About

The android-binary-size trybot exists for three reasons:
1. To measure and make developers aware of the binary size impact of commits.
2. To perform checks that require comparing builds with & without patch.
3. To provide bot coverage for building with `is_official_build=true`.

## Measurements and Analysis

The bot provides analysis using:
* [resource_sizes.py]: The delta in metrics are reported. Most of these are
  described in [//docs/speed/binary_size/metrics.md][metrics].
* [SuperSize]: Provides visual and textual binary size breakdowns.

[resource_sizes.py]: /build/android/resource_sizes.py
[metrics]: /docs/speed/binary_size/metrics.md
[SuperSize]: /tools/binary_size/README.md

## Checks:

- Results are shown under Gerrit's "Checks" tab in the "Info" section.

### Binary Size Increase

- **What:** Checks that [normalized apk size] increases by no more than 16kb on
  arm32, and 64kb on high-end arm64.
- **Why:** To ensure that larger-than-average size changes are understood and
  intentional.

[normalized apk size]: /docs/speed/binary_size/metrics.md#normalized-apk-size

#### ARM64 vs ARM32

If your CL shows a large increase in ARM64 size, but not for ARM32, keep in mind:
- ARM32 instructions are ~half the size of ARM64 instructions.
- ARM32 builds are optimized-for-size (`-Os`) and ARM64 optimizes for speed (`-O2`).

To create a SuperSize report for ARM64, do so locally via:

```sh
tools/binary_size/diagnose_bloat.py --arm64
```

#### What to do if the Check Fails?

- Look at the provided symbol diffs to understand where the size is coming from.
- See if any of the generic [optimization advice] is applicable.
- If you are writing a new feature or including a new library you might want to
  think about skipping the Android platform and to restrict this new
  feature/library to desktop platforms that might care less about binary size.
- If reduction is not practical, add a rationale for the increase to the commit
  description. It should include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.
        - To get a feeling for how large existing features are, refer to
          [go/chrome-supersize](Googlers only).
- If an **auto-roll commit triggered this failure**:
    - The purpose of blocking large rolls is so that we can have the roll commits
      annotated with what is causing the large growth, and to ensure teams are
      aware of large size changes.
    - Please include in the roll commit message what is causing the increase, as
      well as any pointers to discussions / justifications of the size increase.
    - If there is a possibility of any follow-up size reductions, please include
      `Bug:` lines for bugs that will track this work.

- Add a footer to the commit description along the lines of:
    - `Binary-Size: Size increase is unavoidable (see above).`
    - `Binary-Size: Increase is temporary.`
    - `Binary-Size: See commit description.` <-- use this if longer than one
      line.

***note
**Note:** Make sure there are no blank lines between `Binary-Size:` and other
footers.
***

[optimization advice]: /docs/speed/binary_size/optimization_advice.md
[go/chrome-supersize]: https://goto.google.com/chrome-supersize



### Dex Method Count

- **What:** Checks that the number of Java / Kotlin methods after optimization
  does not increase by more than 50.
- **Why:** Ensures that large changes to this metric are scrutinized.

#### What to do if the Check Fails?

- Look at the bot's "Dex Class and Method Diff" output to see which classes and
  methods survived optimization.
- See if any of [Java Optimization] tips are applicable.
- If the increase is from a new dependency, ensure that there is no existing
  library that provides similar functionality.
- If reduction is not practical, add a rationale for the increase to the commit
  description. It should include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.
        - To get a feeling for how large existing features are, open the latest
          [milestone size breakdown] and select "Method Count Mode".
- Add a footer to the commit description along the lines of:
    - `Binary-Size: Added a new library.`
    - `Binary-Size: Enables a large feature that was previously flagged.`

[Java Optimization]: /docs/speed/binary_size/optimization_advice.md#Optimizating-Java-Code

### Mutable Constants

- **What**: Checks that all globals named `kVariableName` or `VARIABLE_NAME`
  are in read-only sections of the binary (either `.rodata` or `.data.rel.do`).
- **Why**: Guards against accidentally missing a `const` keyword. Non-const
  variables have a larger memory footprint than const ones.
- For more context see [https://crbug.com/747064](https://crbug.com/747064).

#### What to do if the Check Fails?

- Make the symbol read-only (usually by adding "const").
- If you can't make it const, then rename it.
- If the symbol is logically const, and you really don't want to rename it to
  reveal that it is actually mutable, you can annotate it with the
  [LOGICALLY_CONST] macro.
- To check what section a symbol is in for a local build:
  ```sh
  ninja -C out/Release obj/.../your_file.o
  third_party/llvm-build/Release+Asserts/bin/llvm-nm out/Release/.../your_file.o --format=darwin
  ```
  - Only `format=darwin` shows the difference between `.data` and `.data.rel.ro`.
  - You need to use llvm's `nm` only when thin-lto is enabled
    (when `is_official_build=true`).

Here's the most common example:
```c++
const char * kMyVar = "...";  // A *mutable* pointer to a const char (bad).
const char * const kMyVar = "..."; // A const pointer to a const char (good).
constexpr char * kMyVar = "..."; // A const pointer to a const char (good).
const char kMyVar[] = "..."; // A const char array (good).
```

For more information on when to use `const char *` vs `const char[]`, see
[//docs/native_relocations.md](/docs/native_relocations.md).

[LOGICALLY_CONST]: https://source.chromium.org/search?q=symbol:LOGICALLY_CONST

### Added Symbols named "ForTest"

- **What:** This checks that we don't have Java symbols with "ForTest" in their
  name in an optimized release APK.
- **Why:** To prevent shipping unused test-only code to end-users.

#### What to do if the Check Fails?

- Make sure your ForTest methods are not called and your ForTest variables are
  not set, in non-test code.
- The check does not care for annotations, it is literally looking at the final
  release APK searching for symbols with "ForTest" in the name. If your method
  is verifiably unreachable from any code, R8 should be able to remove it and
  the check should not fail.
- If your check is failing this could mean that R8 was not able to determine
  that it cannot be called (i.e. your testing infrastructure is being shipped to
  our users). For example if a method marked as @CalledByNative calls your
  ForTest method, the check may fail since R8 cannot remove anything reachable
  from a @CalledByNative method. If this native method is only used in tests,
  you can use @CalledByNativeForTesting instead.

### Uncompressed Pak Entry

- **What:** Checks that `.pak` file entries that are not translatable strings
  and are stored compressed. Limit currently set to 1KB.
- **Why:** Compression makes things smaller and there is normally no reason to
  leaving resources uncompressed.

#### What to do if the Check Fails?

- Ensure that `compress="false"` is **not** being used in the `.grd` entry for
  the resource, so that the default behavior of `compress="gzip"` is triggered
  (for HTML, JS, CSS, and SVG files).

### Expectation Failures

- **What & Why:** Learn about these expectation files [here][expectation files].

[expectation files]: /chrome/android/expectations/README.md

#### What to do if the Check Fails?

- The output of the failing step contains the command to run to update the
  relevant expectation file. Run this command to update the expectation files.

### If All Else Fails

- For help, email [binary-size@chromium.org]. Hearing about your issues helps us
  to improve the tools!
- Not all checks are perfect and sometimes you want to overrule the trybot (for
  example if you did your best and are unable to reduce binary size any
  further).
- Adding a "Binary-Size: $ANY\_TEXT\_HERE" footer to your CL (next to "Bug:")
  will bypass the bot assertions.
    - Most commits that trigger the warnings will also result in Telemetry
      alerts and be reviewed by a binary size sheriff. Failing to write an
      adequate justification may lead to the binary size sheriff filing a bug
      against you to improve your CL.

[binary-size@chromium.org]: https://groups.google.com/a/chromium.org/forum/#!forum/binary-size

## Bot Links Provided by the Last Step

### Size Assertion Results

- Shows the list of checks that ran grouped by passing and failing checks.
- Read this to know which checks failed the tryjob.

### Supersize text diff

- This is the text diff produced by the supersize tool.
- It lists all changed symbols and for each one, which section it lives in,
  which source file it came from as well as what is its size before, after and
  the delta for your CL.
- It also contains a histogram of symbol size deltas.
- You can use this to find which symbols grew and where the binary size impact
  of your CL comes from.

### Supersize html diff

- Visual representation of the text diff above.
- It shows size deltas per file and directory
- It allows you to filter symbols by type/section/size/etc.

## Code Locations

- [Trybot recipe](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/binary_size_trybot.py),
[CI recipe](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/binary_size_generator_tot.py),
[recipe module](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/binary_size/api.py)
- [Link to src-side checks](/tools/binary_size/trybot_commit_size_checker.py)
- [Link to Gerrit Plugin](https://chromium.googlesource.com/infra/gerrit-plugins/chromium-binary-size/)
