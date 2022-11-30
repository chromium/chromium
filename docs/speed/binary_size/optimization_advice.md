# Optimizing Chrome's Binary Size

Read first: [binary_size_explainer.md](binary_size_explainer.md)

 >
 > This document primarily focuses on Android and Chrome OS where image size
 > is especially important.
 >

[TOC]

## General Advice

* Chrome image size on Android and Chrome OS is tightly limited.
* Non trivial increases to the Chrome image size need to be included in the
  [Feature proposal process].
* Use [Compressed resources] wherever possible. This is particularly important
  for images and WebUI resources, which can be substantial.
* Recently a [CrOS Image Size Code Mauve] (googlers only) was called due to
  growth concerns.

[CrOS Image Size Code Mauve]: http://go/cros-image-size-code-mauve
[Compressed resources]: #Compressed-resources

### Size Optimization Help
Feel free to email [binary-size@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/binary-size).

### Compressed resources

[Grit] supports gzip and brotli compression for resources in the .grd files
used to build the `resources.pak` file.

Note that `compress="gzip"` is already the default behavior for HTML, JS, CSS
and SVG files, when the `compress` attribute is not specified.

*   Choose between gzip (default) or brotli (with `compress="brotli"`) as
    follows
    *   gzip compression for highly-compressible data typically has minimal
        impact on load times (but it is worth measuring this, see
        [webui_load_timer.cc] for an example of measuring load times).
    *   Brotli compresses more but is much slower to decompress. Use brotli only
        when performance doesn't matter (e.g. internals pages).
* **Android**: Look at the SuperSize reports from the android-binary-size
    trybot to look for unexpected resources, or unreasonably large symbols.

[Grit]: https://www.chromium.org/developers/tools-we-use-in-chromium/grit
[webui_load_timer.cc]: https://cs.corp.google.com/eureka_internal/chromium/src/chrome/browser/ui/webui/webui_load_timer.cc

### Chrome binary size

Changes that will significantly increase the [Chrome binary size] should be
made with care and consideration:

*   Changes that introduce new libraries can have significant impact and should
    go through the [Feature proposal process].
*   Changes intended to replace existing functionality with significant new code
    should include a deprecation plan for the code being replaced.

[Chrome binary size]: https://drive.google.com/a/google.com/open?id=1aeIxj8jPOimmlnqD7PvS6np51DZa96dcF72N6CtO6N8


## Chrome OS Focused Advice

### Compressed l10n Strings

Strings do not compress well individually, but an entire l10n file compresses
very well.

There are two mechanisms for compressing Chrome l10n files.

1.  Compressed .pak files
    *   For desktop Chrome, string resource files generate individual .pak
        files, e.g. `generated_resources_en.pak`.<br/>
        These get combined into locale specific .pak files, e.g.
        `locales/en-US.pak`
    *   On Chrome OS, we set`'compress = true` in [chrome_repack_locales.gni],
        which causes these .pak files to be gzip compressed.<br/>
        (Chrome identifies them as compressed by parsing the file header).
    *   So, *Chrome strings on Chrome OS will be compressed by default*,
        nothing else needs to be done!
1.  Compressing .json l10n files
     *   Extensions and apps store l10n strings as `messages.json` files in
         `{extension dir}/_locales/{locale}`.
         *   For Chrome OS component extensions (e.g. ChromeVox), we include
             these extensions as part of the Chrome image.
         *   These strings get localized across 50+ languages, so it is
             important to compress them.
     *   For *component extensions only*, these files can be gzip compressed
         (and named `messages.json.gz`) as part of their build step.
     *   For extensions using GN:
         1.  Specify `type="chrome_messages_json_gzip"` for each `<output>`
             entry in the .grd file.
         1.  Name the outputs `messages.json.gz` in the .grd and strings.gni
             files.
     * See https://crbug.com/1023568 for details and an example CL.

[chrome_repack_locales.gni]: https://cs.chromium.org/chromium/src/chrome/chrome_repack_locales.gni

### chromeos-assets

*   Input methods, speech synthesis, and apps consume a great deal of disk space
    on the Chrome OS rootfs partition.
*   These assets are not part of the chromium repository, however they do
    affect [rootfs size] on devices.
*   Proposed additions or increases to chromeos-assets should go through the
    [Feature proposal process] and should consider using some form of
    [Downloadable Content] if possible.

[rootfs size]: https://docs.google.com/document/d/1d3Y2ngMGEP_yfxBFrgOE-dinDILfyAUY9LT0JLlq6zg/edit?usp=sharing
[Downloadable Content]: https://docs.google.com/presentation/d/1wM-eDX-BQavecQz20gPxRF6CxWp1k1sh7zSKMxz4BLI/edit?usp=sharing


## Android Focused Advice

### Optimizing Translations (Strings)

 * Use [Android System strings](https://developer.android.com/reference/android/R.string.html) where appropriate
 * Ensure that strings in .grd files need to be there. For strings that do
   not need to be translated, put them directly in source code.

### Optimizing Images

 * Would a vector image work?
   * Images that can be described by a series of paths should generally be
     stored as vectors.
   * For images used in native code: [VectorIcon](https://chromium.googlesource.com/chromium/src/+/HEAD/components/vector_icons/README.md).
   * For Android drawables: [VectorDrawable](https://developer.android.com/guide/topics/graphics/vector-drawable-resources).
     * Convert from `.svg` following [this guide](https://developer.android.com/studio/write/vector-asset-studio.html#svg).
     * (Googlers): Find most icons as .svg at [go/icons](https://goto.google.com/icons).
 * Would **lossy** compression make sense (often true for large images)?
   * If so, [use lossy webp](https://codereview.chromium.org/2615243002/).
   * And omit some densities (e.g. add only an xxhdpi version).
 * For lossless `.png` images, see how few unique colors you can use without a
   noticeable difference.
   * This can often reduce an already optimized .png by 33%-50%.
   * [Use pngquant](https://pngquant.org) to try this out.
     * Requires trial and error for each number of unique colors.
     * Use one of the GUI tools linked from the website to do this easily.
 * Finally - Ensure .png files are fully optimized.
   * Use [tools/resources/optimize-png-files.sh](https://cs.chromium.org/chromium/src/tools/resources/optimize-png-files.sh).
   * There is some [Googler-specific guidance](https://goto.google.com/clank/engineering/best-practices/adding-image-assets) as well.

#### What Build-Time Image Optimizations are There?
 * For non-ninepatch images, `drawable-xxxhdpi` are omitted (they are not
   perceptibly different from xxhdpi in most cases).
 * For non-ninepatch images within res/ directories (not for .pak file images),
   they are converted to webp.
   * Use the `android-binary-size` trybot to see the size of the images as webp,
     or just build `ChromePublic.apk` and use `unzip -l` to see the size of the
     images within the built apk.

### Optimizing Android Resources
 * Use config-specific resource directories sparingly.
   * Introducing a new config has [a large cost][arsc-bloat].

[arsc-bloat]: https://medium.com/androiddevelopers/smallerapk-part-3-removing-unused-resources-1511f9e3f761#0b72

### Optimizing Code

In most parts of the codebase, you should try to optimize your code for binary
size rather than performance. Most code runs "fast enough" and only needs to be
performance-optimized if identified as a hot spot. Individual code size affects
overall binary size no matter the utility of the code.

What this *could* mean in practice?
 * Use a linear search over an array rather than a binary search over a sorted
   one.
 * Reuse common code rather than writing optimized purpose-specific code.

Practical advice:
 * When making changes, look at symbol breakdowns with SuperSize reports from
   the [android-binary-size trybot][size-trybot].
   * Or use [//tools/binary_size/diagnose_bloat.py][diagnose_bloat] to create
     diffs locally.
 * Ensure no symbols exist that are used only by tests.
 * Be concise with strings used for error handling.
   * Identical strings throughout the codebase are de-duped. Take advantage of
     this for log strings and exception messages.
   * For exceptions, prefer to omit a message altogether unless it provides
     more detail than the stack trace will.

#### Optimizing Native Code
 * If there's a notable increase in `.data.rel.ro`:
   * Ensure there are not [excessive relocations][relocations].
 * If there's a notable increase in `.rodata`:
   * See if it would make sense to compress large symbols here by moving them to
     .pak files.
   * Gut-check that all unique string literals being added are actually useful.
 * If there's a notable increase in `.text`:
   * If there are a lot of symbols from C++ templates:
     * Try moving parts of the templatized function that don't use the template
       parameters to [non-templated helper functions][template_bloat_one]).
     * Or see if the signature can be re-worked such that there are
       [fewer variants of template parameters](template_bloat_two).
   * Try to leverage identical-code-folding as much as possible by making the
     shape of your code consistent.
     * E.g. Use PODs wherever possible, and especially in containers. They will
       likely compile down to the same code as other pre-existing PODs.
       * Try also to use consistent field ordering within PODs.
     * E.g. a `std::vector` of bare pointers will very likely be ICF'ed, but one
       that uses smart pointers gets type-specific destructor logic inlined into
       it.
     * This advice is especially applicable to generated code.
   * If symbols are larger than expected, use the `Disassemble()` feature of
     [`supersize console`][supersize-console] to see what is going on.
     * Watch out for inlined constructors & destructors. E.g. having parameters
       that are passed by value forces callers to construct objects before
       calling.
       * E.g. For frequently called functions, it can make sense to provide
         separate `const char *` and `const std::string&` overloads rather than
         a single `base::StringPiece`.

#### Optimizing Java Code
 * If you're adding a new feature, see if it makes sense for it to be packaged
   into its own [feature split]. E.g.:
   * Has a non-trivial amount of Dex (>50kb)
   * Not needed on startup
   * Has a small integration surface (calls into it must be done with
     reflection).
 * Prefer fewer large JNI calls over many small JNI calls.
 * Minimize the use of class initializers (`<clinit>()`).
   * If R8 cannot determine that they are "trivial", they will prevent
     inlining of static members on the class.
   * In C++, static objects are created at compile time, but in Java they
     are created by executing code within `<clinit>()`. There is often little
     advantage to initializing class fields statically vs. upon first use.
 * Try to use default values for fields rather than explicit initialization.
   * E.g. Name booleans such that they start as "false".
   * E.g. Use integer sentinels that have initial state as 0.
 * Minimize the number of callbacks / lambdas that each API requires.
   * Each callback / lambda is syntactic sugar for an anonymous class, and all
     classes have a constructor in addition to the callback method.
   * E.g. rather than have `onFailure()` vs `onSuccess()`, have an
     `onFinished(bool)`.
   * E.g. rather than have `onTextChanged(newValue)`, `onDateChanged(newValue)`,
     ..., have a single `onChanged()`, where callbacks use getters to retrieve
     the new values.
     * This design allows classes to use a shared callback for multiple listeners.
     * This design simplifies data flow by forcing the use of getters (assuming
       getters exist in the first place).
 * Do not override `equals()`, `toString()`, `hashCode()` unless necessary. Since
   these methods are defined on `Object`, R8 can basically never remove them.
 * Ensure unused code is optimized away by R8.
   * See [here][proguard-build-doc] for more info on how Chrome uses ProGuard.
   * Add `@CheckDiscard` to methods or classes that you expect R8 to inline.
   * Guard code with `BuildConfig.ENABLE_ASSERTS` to strip it in release builds.
   * Use [//third_party/r8/playground][r8-playground] to figure out how various
     coding patterns are optimized by R8.
   * Build with `enable_proguard_obfuscation = false` and use
     `//third_party/android_sdk/public/build-tools/*/dexdump` to see how code was
     optimized directly in apk / bundle targets.

[feature split]: /docs/android_dynamic_feature_modules.md
[proguard-build-doc]: /build/android/docs/java_optimization.md
[size-trybot]: /tools/binary_size/README.md#Binary-Size-Trybot-android_binary_size
[diagnose_bloat]: /tools/binary_size/README.md#diagnose_bloat_py
[relocations]: /docs/native_relocations.md
[template_bloat_one]: https://bugs.chromium.org/p/chromium/issues/detail?id=716393
[template_bloat_two]: https://chromium-review.googlesource.com/c/chromium/src/+/2639396
[supersize-console]: /tools/binary_size/README.md#Usage_console
[r8-playground]: /third_party/r8/playground

### Optimizing Third-Party Android Dependencies

 * Look through SuperSize symbols to see whether unwanted functionality
   is being pulled in.
   * Use ProGuard's [-whyareyoukeeping] to see why unwanted symbols are kept
     (e.g. to [//base/android/proguard/chromium_apk.flags](/base/android/proguard/chromium_apk.flags)).
   * Try adding [-assumenosideeffects] rules to strip out unwanted calls.
 * Consider removing all resources via `strip_resources = true`.
 * Remove specific drawables via `resource_exclusion_regex`.

[-whyareyoukeeping]: https://r8-docs.preemptive.com/#keep-rules
[-assumenosideeffects]: https://r8-docs.preemptive.com/#general-rules


[Feature proposal process]: http://www.chromium.org/developers/new-features
