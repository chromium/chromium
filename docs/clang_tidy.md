# Clang Tidy

[TOC]

## Introduction

[clang-tidy](http://clang.llvm.org/extra/clang-tidy/) is a clang-based C++
“linter” tool. Its purpose is to provide an extensible framework for diagnosing
and fixing typical programming errors, like style violations, interface misuse,
or bugs that can be deduced via static analysis.

## Where is it?

clang-tidy is available in two places in Chromium:

- In Chromium checkouts
- In code review on Gerrit

Clang-tidy automatically runs on any CL that Chromium committers upload to
Gerrit, and will leave code review comments there. This is the recommended way
of using clang-tidy.

## Enabled checks

Chromium globally enables a subset of all of clang-tidy's checks (see
`${chromium}/src/.clang-tidy`). We want these checks to cover as much as we
reasonably can, but we also strive to strike a reasonable balance between signal
and noise on code reviews. Hence, a large number of clang-tidy checks are
disabled.

### Adding a new check

New checks require review from cxx@chromium.org. If you propose a check and it
gets approved, you may turn it on, though please note that this is only
provisional approval: we get signal from users clicking "Not Useful" on
comments. If feedback is overwhelmingly "users don't find this useful," the
check may be removed.

Traditionally, petitions to add checks include [an
evaluation](https://docs.google.com/document/d/1i1KmXtDD4j_qjhmAdGlJ6UkYXByVX1Kp952Zusdcl5k/edit?usp=sharing)
of the check under review. Crucially, this includes two things:

- a count of how many times this check fires across Chromium
- a random sample (>30) of places where the check fires across Chromium

It's expected that the person proposing the check has manually surveyed every
clang-tidy diagnostic in the sample, noting any bugs, odd behaviors, or
interesting patterns they've noticed. If clang-tidy emits FixIts, these are
expected to be considered by the evaluation, too.

An example of a previous proposal email thread is
[here](https://groups.google.com/a/chromium.org/g/cxx/c/iZ6-Y9ZhC3Q/m/g-8HzqmbAAAJ).

#### Evaluating: running clang-tidy across Chromium

Running clang-tidy requires some setup. First, you'll need to sync clang-tidy,
which requires adding `checkout_clang_tidy` to your `.gclient` file:

```
solutions = [
  {
    'custom_vars': {
      'checkout_clang_tidy': True,
    },
  }
]
```

Your next run of `gclient runhooks` should cause clang-tidy to be synced.

To run clang-tidy across all of Chromium, you'll need a checkout of Chromium's
[tools/build/](https://chromium.googlesource.com/chromium/tools/build) repository.
Once you have that and a Chromium `out/` dir with an `args.gn`, running
clang-tidy across all of Chromium is a single command:

```
$ cd ${chromium}/src
$ ${chromium_tools_build}/recipes/recipe_modules/tricium_clang_tidy/resources/tricium_clang_tidy_script.py \
    --base_path $PWD \
    --out_dir out/Linux \
    --findings_file all_findings.json \
    --clang_tidy_binary $PWD/third_party/llvm-build/Release+Asserts/bin/clang-tidy \
    --all
```

To only run clang-tidy against certain files, replace the `--all` parameter with
the individual file paths.

All clang-tidy checks are run on Linux builds of Chromium, so please set up your
`args.gn` to build Linux.

`all_findings.json` is where all of clang-tidy's findings will be dumped. The
format of this file is detailed in `tricium_clang_tidy_script.py`.

**Note** that the above command will use Chromium's top-level `.clang-tidy` file
(or `.clang-tidy` files scattered throughout `third_party/`, depending on the
files we lint. In order to test a *new* check, it's recommended that you use
`tricium_clang_tidy_script.py`'s `--tidy_checks` flag. Usage of this looks like:

```
$ cd ${chromium}/src
$ ${chromium_build}/recipes/recipe_modules/tricium_clang_tidy/resources/tricium_clang_tidy_script.py \
    --base_path $PWD \
    --out_dir out/Linux \
    --findings_file all_findings.json \
    --clang_tidy_binary $PWD/third_party/llvm-build/Release+Asserts/bin/clang-tidy \
    --tidy_checks='-*,YOUR-NEW-CHECK-NAME-HERE'
    --all
```

### Ignoring a check

If a check is invalid on a particular piece of code, clang-tidy supports `//
NOLINT` and `// NOLINTNEXTLINE` for ignoring all lint checks in the current and
next lines, respectively. To suppress a specific lint, you can put it in
parenthesis, e.g., `// NOLINTNEXTLINE(modernize-use-nullptr)`. For more, please
see [the documentation](
https://clang.llvm.org/extra/clang-tidy/#suppressing-undesired-diagnostics).

**Please note** that adding comments that exist only to silence clang-tidy is
actively discouraged. These comments clutter code, can easily get
out-of-date, and don't provide much value to readers. Moreover, clang-tidy only
complains on Gerrit when lines are touched, and making Chromium clang-tidy clean
is an explicit non-goal; making code less readable in order to silence a
rarely-surfaced complaint isn't a good trade-off.

If clang-tidy emits a diagnostic that's incorrect due to a subtlety in the code,
adding an explanantion of what the code is doing with a trailing `NOLINT` may be
fine. Put differently, the comment should be able to stand on its own even if we
removed the `NOLINT`. The fact that the comment also silences clang-tidy is a
convenient side-effect.

For example:

Not OK; comment exists just to silence clang-tidy:

```
// NOLINTNEXTLINE
for (int i = 0; i < arr.size(); i++) {
  // ...
}
```

Not OK; comment exists just to verbosely silence clang-tidy:

```
// Clang-tidy doesn't get that we can't range-for-ize this loop. NOLINTNEXTLINE
for (int i = 0; i < arr.size(); i++) {
  // ...
}
```

Not OK; it's obvious that this loop modifies `arr`, so the comment doesn't
actually clarify anything:

```
// It'd be invalid to make this into a range-for loop, since the body might add
// elements to `arr`. NOLINTNEXTLINE
for (int i = 0; i < arr.size(); i++) {
  if (i % 4) {
    arr.push_back(4);
    arr.push_back(2);
  }
}
```

OK; comment calls out a non-obvious property of this loop's body. As an
afterthought, it silences clang-tidy:

```
// It'd be invalid to make this into a range-for loop, since the call to `foo`
// here might add elements to `arr`. NOLINTNEXTLINE
for (int i = 0; i < arr.size(); i++) {
  foo();
  bar();
}
```

In the end, as always, what is and isn't obvious at some point is highly
context-dependent. Please use your best judgement.

## But I want to run it locally

If you want to sync the officially-supported `clang-tidy` to your workstation,
add the following to your .gclient file:

```
solutions = [
  {
    'custom_vars': {
      'checkout_clang_tidy': True,
    },
  },
]
```

If you already have `solutions` and `custom_vars`, just add
`checkout_clang_tidy` to the existing `custom_vars` map.

Once the above update has been made, run `gclient runhooks`, and clang-tidy
should appear at `src/third_party/llvm-build/Release+Asserts/bin/clang-tidy` if
your Chromium tree is sufficiently up-to-date.

### Running clang-tidy locally

**Note** that the local flows with clang-tidy are experimental, and require an
LLVM checkout. Tricium is happy to run on WIP CLs, and we strongly encourage its
use.

That said, assuming you have the LLVM sources available, you'll need to bring
your own `clang-apply-replacements` binary if you want to use the `-fix` option
noted below.

**Note:** If you're on a system that offers a clang tools through its package
manager (e.g., on Debian/Ubuntu, `sudo apt-get install clang-tidy clang-tools`),
you might not need an LLVM checkout to make the required binaries and scripts
(`clang-tidy`, `run-clang-tidy` and `clang-apply-replacements`) available in
your `$PATH`. However, the system packaged binaries might be several versions
behind Chromium's toolchain, so not all flags are guaranteed to work. If this is
a problem, consider building clang-tidy from the same revision the current
toolchain is using, rather than filing a bug against the toolchain component.
This can be done as follows:
```
tools/clang/scripts/build_clang_tools_extra.py \
    --fetch out/Release clang-tidy clang-apply-replacements
```
Running clang-tidy is then (hopefully) simple.
1.  Build chrome normally.
```
ninja -C out/Release chrome
```
2.  Export Chrome's compile command database
```
gn gen out/Release --export-compile-commands
```
3.  Enter the build directory
```
cd out/Release
```
4.  Run clang-tidy.
```
<PATH_TO_LLVM_SRC>/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
    -p . \# Set the root project directory, where compile_commands.json is.
    # Set the clang-tidy binary path, if it's not in your $PATH.
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    # Set the clang-apply-replacements binary path, if it's not in your $PATH
    # and you are using the `fix` behavior of clang-tidy.
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    # The checks to employ in the build. Use `-*,...` to omit default checks.
    -checks=<CHECKS> \
    -header-filter=<FILTER> \# Optional, limit results to only certain files.
    -fix \# Optional, used if you want to have clang-tidy auto-fix errors.
    'chrome/browser/.*' # A regex of the files you want to check.

Copy-Paste Friendly (though you'll still need to stub in the variables):
<PATH_TO_LLVM_SRC>/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
    -p . \
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    -checks=<CHECKS> \
    -header-filter=<FILTER> \
    -fix \
    'chrome/browser/.*'
```

Note that the source file regex must match how the build specified the file.
This means that on Windows, you must use (escaped) backslashes even from a bash
shell.

### Questions

Questions about the local flow? Reach out to rdevlin.cronin@chromium.org,
thakis@chromium.org, or gbiv@chromium.org.

Questions about the Gerrit flow? Email tricium-dev@google.com or
infra-dev+tricium@chromium.org, or file a bug against `Infra>LUCI>BuildService>PreSubmit>Tricium`.
Please CC gbiv@chromium.org and dcheng@chromium.org on any of these.

Discoveries? Update the doc!
