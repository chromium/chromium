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

- **What:** Checks that [compressed fuchsia archive] size increases by no more
  than 12kb.
  - These packages are `cast_runner` and `web_engine`.
- **Why:** Chrome-Fuchsia deploys on platforms with small footprints. As each
  release rolls, Fuchsia runs its own set of size checks that will reject a
  release if the Chrome-Fuchsia packages exceed the allocated size budget. This
  builder tries to mitigate the unending growth of Chromium's impact on
  Chrome-Fuchsia continuous deployment.

[compressed fuchsia archive]: #compressed-vs-uncompressed

#### What to do if the Check Fails?

- The `Read diff results` stdout will also give a breakdown of growth by
  package, if any:
```json
{
  "archive_filenames": [],
  "compressed": {
    "cast_runner": 0,
    "chrome_fuchsia": 40960,  # chrome_fuchsia = web_engine + cast_runner
    "web_engine": 40960  # This package grew by 40kB (post-compression)
  },
  "links": [],
  "status_code": 1,
[...]
  "uncompressed": {
    "cast_runner": 0,
    "chrome_fuchsia": 33444,
    "web_engine": 33444  # This package grew by 32.66kB (pre-compression)
  }
}
```
  - If `cast_runner` grew in size, you may need assistance from
  the Chrome-Fuchsia team (fuchsia-dev@chromium.org).
- If you are writing a new feature or including a new library, consider:
  - If it is a feature only intended for Chrome (a full-browser), Contact
  fuchsia-dev@chromium.org to help fix this.
  - Whether it belongs on a size-contrained device (think a low-end phone with a
  <1GB in total disk storage). fuchsia-dev@chromium.org can be helpful:
    - If it does, look into how to reduce the
    [overall size](#obvious-regressions)
    - If it does not, look to remove it. See below.

If you find it *should* be removed from a size-constrained platform, you should
guard the code with a `BUILDFLAG` and disable the associated feature in
size-constrained builds by explicitly setting the GN arg in
[`size_optimized_cast_receiver_args.gn`](../../../build/config/fuchsia/size_optimized_cast_receiver_args.gn).

- See if any of the generic [optimization advice] is applicable.
- See [the section below](#obvious-regressions)
- If reduction is not practical, add a rationale for the increase to the commit
  description, and [skip the check](#skipping-the-check). It could include:
    - A list of any optimizations that you attempted (if applicable)
    - If you think that there might not be a consensus that the code your adding
      is worth the added file size, then add why you think it is.

### Skipping the check

Add a **footer** to the commit description along the lines of:

- `Fuchsia-Binary-Size: Size increase is unavoidable.`
- `Fuchsia-Binary-Size: Increase is temporary.`
- `Fuchsia-Binary-Size: See commit description.` <-- use this if longer
than one line.

***note
**Note:** Make sure there are no blank lines between `Fuchsia-Binary-Size:` and
other footers.
***

[optimization advice]: /docs/speed/binary_size/optimization_advice.md

## Compressed vs Uncompressed

The size metric we care about the most is the compressed size. This is an
**estimate** of how large the Chrome-Fuchsia packages will be when delivered on
device (actual compression can vary between devices, so the computed numbers may
not be accurate). However,  you may see the uncompressed and compressed size
grow by different amounts (and sometimes the compressed size is larger than the
uncompressed)!

This is due to how sizes are calculated and how the compression is done.
The uncompressed size is exactly that: the size of the package before it is
compressed.

Compression is done via the `blobfs-compression` tool, exported from the
Fuchsia SDK. This compresses the file into a package that is ready to be
deployed to the Fuchsia device. With the current (default) compression-mode,
this compresses the package in page-sizes designed for the device and
filesystem. Since each page is at least 8K, **increments in the compressed size
are always multiples of 8K with the current compression mode**. So, if your
change causes the previous compression page to go over the limit, you may see
an 8K increase for an otherwise small change.

Large changes will increase more than a page's work (to at least 16K), which is
why we only monitor 12K+ changes (12K isn't possible due to the 8K page size)
and not 8K+.

**You are responsible only for pre-compression size increases. If your change
did not cause a pre-compression size increase, but still failed the builder,
please ignore it using the `Fuchsia-Binary-Size:`
[footer](#skipping-the-check).**

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
import("//build/config/fuchsia/size_optimized_cast_receiver_args.gn")
dcheck_always_on = false
is_debug = false
is_official_build = true
target_cpu = "arm64"
target_os = "fuchsia"
use_remoteexec = true  # If appropriate.
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
the end, for `chrome_fuchsia`, `web_engine`, and `cast_runner`. The number that
is deployed to the device is the `compressed` version.

## How to reduce your binary-size for Fuchsia
TODO(crbug.com/40821616): Fill this out.

### Obvious regressions

#### Many new blobs
(shamelessly stolen from this
[doc](https://docs.google.com/document/d/1K3fOJJ3rsKA5WtvRCJtuLQSqn7MtOuVUzJA9cFXQYVs/edit#),
but looking for any tips on how to improve this for Fuchsia specifically)

Look at blobs and see that there aren't a huge number of blobs added in.

- What does it mean if blobs are added in?
  - Something changed in the dependencies, causing new files to be pulled in.
- Is there a way to reverse-engineer a blobs source?
  - Locales are language pack
  - Try searching BUILD files to see if the .so was included from somewhere
    in particular.

### Bloaty

[Bloaty](https://github.com/google/bloaty) can be used
to determine the composition of the binary (and can be helpful for determining
the cause of the increase).

0. (first time only) Install Bloaty using these
[instructions](https://github.com/google/bloaty#install).
1. Build two copies of the packages (see
   [above](#running-a-local-binary_size-check)) -
   with and without your changes in two separate output directories.
2. Generate Bloaty results. You can run Bloaty against the stripped binaries
   (`<out dir>/web_engine_exe` and `<out dir>/cast_runner_exe`). However, if
   you want more information, you will have to run it against the unstripped
   binaries (located in `<out dir>/exe.unstripped`. You only need to run Bloaty
   against the binary your change affected.

```bash
$ bloaty -d compileunits,symbols $OUT_DIR/exe.unstripped/web_engine_exe \
  -n $ROW_LIMIT -s vm
```

`-n $ROW_LIMIT` determines the number of rows to show per level before
collapsing. Setting to `0` shows all rows. Default is 20.

`-s vm` indicates to sort by Virtual Memory (VM) size increase. This is the
metric that grows somewhat closely to the binary-size bot's size metric.

**NOTE**: that the sizes reported from Bloaty will not be exactly the same as
those reported by the [`binary_sizes` script](#run-the-size-script) since
Bloaty analyzes the uncompressed (and potentially unstripped) binary, but
the reported relative growth can point you in the right direction. The
`File Size` can vary a lot due to debug symbol information. The `VM Size` is
usually a good lead.

**If Bloaty reports your change decreased the uncompressed size, use a
[footer](#skipping-the-check) to
ignore the check.**

You can also directly generate a comparison with the following:

```bash
$ bloaty -d compileunits,symbols \
  $OUT_DIR_WITH_CHANGE/exe.unstripped/web_engine_exe -n $ROW_LIMIT -s vm -- \
  $OUT_DIR_WITHOUT_CHANGE/exe.unstripped/web_engine_exe
```

You can find out more about sections of ELF binaries
[here](https://refspecs.linuxbase.org/LSB_3.0.0/LSB-PDA/LSB-PDA/specialsections.html).

## If All Else Fails

- For help, email [fuchsia-dev@chromium.org]. They're expert
  Chrome-Fuchsia developers! See [here for more
  details](/docs/fuchsia/README.md).
- Not all checks are perfect and sometimes you want to overrule the trybot (for
  example if you did your best and are unable to reduce binary size any
  further).
- Check out the
  [Chromium binary-size](https://groups.google.com/a/chromium.org/g/binary-size) Google
  group.
- Adding a “Fuchsia-Binary-Size: $ANY\_TEXT\_HERE”
  [footer](#skipping-the-check) to your cl (next to “Bug:”)  will bypass
  the bot assertions.
    - Most commits that trigger the warnings will also result in Telemetry
      alerts and be reviewed by a binary size sheriff. Failing to write an
      adequate justification may lead to the binary size sheriff filing a bug
      against you to improve your cl.

## Code Locations

- [Trybot recipe](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipes/binary_size_fuchsia_trybot.py),
[recipe module](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/binary_size/api.py)
- [Link to src-side binary_size script](/build/fuchsia/binary_sizes.py)
- [Link to src-side binary_size_differ script](/build/fuchsia/binary_size_differ.py)
