# Trybot: fuchsia-binary-size

[TOC]

## About

The fuchsia-binary-size trybot exists for two reasons:
1. To measure and make developers aware of the binary size impact of commits
on Chrome-Fuchsia products. Reducing growth on Chrome-Fuchsia products
on the limited-size platforms is key to shipping Chrome (and its derivatives)
successfully on Fuchsia.
2. To perform checks that require comparing builds with & without patch.

## Measurements and Analysis

The bot provides analysis using:
* [binary_sizes.py]: Computes growth across compressed archives,
 breaking them down by blob size.
* [binary_size_differ.py]: Computes deltas from output of the
  binary_sizes.py script.

[binary_sizes.py]: /build/fuchsia/binary_sizes.py
[binary_size_differ.py]: /build/fuchsia/binary_size_differ.py

## Checks:

- The changes are aggregated in the `Generate commit size analysis files` step of the trybot
- The difference with & without the patch are compared in the `Generate Diffs`
  step. This summarizes how each package grew, shrank, or stayed the same.

***note
**Note:** The tool currently excludes shared-libary contributions to the
package size.
***

### Binary Size Increase

- **What:** Checks that [compressed fuchsia archive] size increases by no more than 12kb.
  - These packages are `cast_runner` and `web_engine`.
- **Why:** Chrome-Fuchsia deploys on platforms with small footprints. As each
  release rolls, Fuchsia runs its own set of size checks that will reject a
  release if the Chrome-Fuchsia packages exceed the allocated size budget. This
  builder tries to mitigate the unending growth of Chromium's impact on
  Chrome-Fuchsia continuous deployment.

[compressed fuchsia archive]: #compressed-vs-uncompressed

#### What to do if the Check Fails?

- Look at the provided `commit size analysis files` stdout to understand where the size is coming from.
  - The `Read diff results` stdout will also give a breakdown of growth by
    package, if any.
- See if any of the generic [optimization advice] is applicable.
- If you are writing a new feature or including a new library you might want to
  think about skipping the `web_engine`/`cast_runner` binaries and to restrict this new
  feature/library to desktop platforms that might care less about binary size.
  - This can be done by removing it with the `is_fuchsia` BUILD tag and
    `OS_FUCHSIA` macro.
  - If this change belongs on a full-browser, but not `web_engine`/`cast_runner`,
    you should also guard against the  `ARCH_CPU_ARM64` tag, as this
    CPU-architecture is the only (current) set that requires size-checks.
- See [the section below](#obvious-regressions)
- If reduction is not practical, add a rationale for the increase to the commit
  description. It should include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.

- Add a footer to the commit description along the lines of:
    - `Fuchsia-Binary-Size: Size increase is unavoidable (see above).`
    - `Fuchsia-Binary-Size: Increase is temporary.`
    - `Fuchsia-Binary-Size: See commit description.` <-- use this if longer than one line.

***note
**Note:** Make sure there are no blank lines between `Fuchsia-Binary-Size:` and other
footers.
***

[optimization advice]: /docs/speed/binary_size/optimization_advice.md

## Compressed vs Uncompressed

The size metric we care about the most is the compressed size. This is an
**estimate** of how large the Chrome-Fuchsia packages will be when delivered on
device (actual compression can vary between devices, so the computed numbers may
not be accurate). However,  you may see the uncompressed and compressed size grow by
different amounts (and sometimes the compressed size is larger than the
uncompressed)!

This is due to how sizes are calculated and how the compression is done.
The uncompressed size is exactly that: the size of the package before it is
compressed.

Compression is done via the `blobfs-compression` tool, exported from the
Fuchsia SDK. This compresses the file into a package that is ready to be
deployed to the Fuchsia device. With the current (default) compression-mode,
this compresses the package in page-sizes designed for the device and filesystem.
Since each page is at least 8K, **increments in the compressed size are always
multiples of 8K with the current compression mode**. So, if your change causes
the previous compression page to go over the limit, you may see an 8K increase
for an otherwise small change.

Large changes will increase more than a page's work (to at least 16K), which is
why we only monitor 12K+ changes (12K isn't possible due to the 8K page size)
and not 8K+.

## Running a local binary-size check

If you want to check your changes impact to binary size locally (instead of
against the trybot), do the following:
### 0. First time compiling Chrome-Fuchsia

Add the following to your `.gclient` config:

```
{
  # ... whatever you have here ...
  "solutions": [
    {
      # ...whatever you have here...
    }
  ],
  "target_os": [
    "fuchsia"
  ],
}

```


Then run `gclient sync` to add the fuchsia-sdk to your `third_party` directory.

### 1. GN Args
Set up a build directory with the following GN Args:

```
dcheck_always_on = false
is_debug = false
is_official_build = true
target_cpu = "arm64"
target_os = "fuchsia"
use_goma = true
```

### 2. Build

Build the `fuchsia_sizes` target:

```
autoninja -C <path/to/out/dir> fuchsia_sizes
```

### 3. Run the size script

Run the size script with the following command:

```
build/fuchsia/binary_sizes.py --build-out-dir <path/to/out/dir>
```

The size breakdown by blob and package will be given, followed by a summary at
the end, for `chrome_fuchsia`, `web_engine`, and `cast_runner`. The number that is
deployed to the device is the `compressed` version.

## How to reduce your binary-size for Fuchsia
TODO(crbug.com/1296349): Fill this out.

### Obvious regressions

#### Many new blobs
(shamelessly stolen from this
[doc](https://docs.google.com/document/d/1K3fOJJ3rsKA5WtvRCJtuLQSqn7MtOuVUzJA9cFXQYVs/edit#), but looking for any tips on how to improve this for Fuchsia specifically)

Look at blobs and see that there aren't a huge number of blobs added in.

- What does it mean if blobs are added in?
  - Something changed in the dependencies, causing new files to be pulled in.
- Is there a way to reverse-engineer a blobs source?
  - Locales are language pack
  - Try searching BUILD files to see if the .so was included from somewhere
    in particular.

## If All Else Fails

- For help, email [chrome-fuchsia-team@google.com]. They're expert
  Chrome-Fuchsia developers!
- Not all checks are perfect and sometimes you want to overrule the trybot (for
  example if you did your best and are unable to reduce binary size any
  further).
- Adding a “Fuchsia-Binary-Size: $ANY\_TEXT\_HERE” footer to your cl (next to “Bug:”)
  will bypass the bot assertions.
    - Most commits that trigger the warnings will also result in Telemetry
      alerts and be reviewed by a binary size sheriff. Failing to write an
      adequate justification may lead to the binary size sheriff filing a bug
      against you to improve your cl.

[chrome-fuchsia-team@chromium.org]: https://groups.google.com/a/chromium.org/forum/#!forum/binary-size

## Code Locations

- [Trybot recipe](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/binary_size_fuchsia_trybot.py),
[recipe module](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/binary_size/api.py)
- [Link to src-side binary_size script](/build/fuchsia/binary_sizes.py)
- [Link to src-side binary_size_differ script](/build/fuchsia/binary_size_differ.py)
