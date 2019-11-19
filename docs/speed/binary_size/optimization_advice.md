# Optimizing Chrome's Binary Size

 >
 > This advice focuses on Android.
 >

[TOC]

## How To Tell if It's Worth Spending Time on Binary Size?

 * Binary size is a shared resource, and thus its growth is largely due to the
   tragedy of the commons.
 * It typically takes about a week of engineering time to reduce Android's
   binary size by 50kb.
 * As of 2019, Chrome for Android (arm32) grows by about 100kb per week.
 * To get a feeling for how large existing features are, refer to the
   [milestone size breakdowns] and group by "Component".

[milestone size breakdowns]: https://storage.googleapis.com/chrome-supersize/index.html

## Optimizing Translations (Strings)

 * Use [Android System strings](https://developer.android.com/reference/android/R.string.html) where appropriate
 * Ensure that strings in .grd files need to be there. For strings that do
   not need to be translated, put them directly in source code.

## Optimizing Non-Image Native Resources in .pak Files

 * Ensure `compress="gzip"` or `compress="brotli"` is used for all
   highly-compressible (e.g. text) resources.
   * Brotli compresses more but is much slower to decompress. Use brotli only
     when performance doesn't matter (e.g. internals pages).
 * Look at the SuperSize reports from the android-binary-size trybot to look for
   unexpected resources, or unreasonably large symbols.

## Optimizing Images

 * Would a vector image work?
   * Images that can be described by a series of paths should generally be
     stored as vectors.
     * The one exception is if the image will be used pre-Lollipop in a
       notification or application icon.
   * For images used in native code: [VectorIcon](https://chromium.googlesource.com/chromium/src/+/HEAD/components/vector_icons/README.md).
   * For Android drawables: [VectorDrawable](https://developer.android.com/guide/topics/graphics/vector-drawable-resources).
     * Convert from `.svg` online using https://inloop.github.io/svg2android/.
     * Optimize vector drawables with [avocado](https://bugs.chromium.org/p/chromium/issues/detail?id=982302).
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

### What Build-Time Image Optimizations are There?
 * For non-ninepatch images, `drawable-xxxhdpi` are omitted (they are not
   perceptibly different from xxhdpi in most cases).
 * For non-ninepatch images within res/ directories (not for .pak file images),
   they are converted to webp.
   * Use the `android-binary-size` trybot to see the size of the images as webp,
     or just build `ChromePublic.apk` and use `unzip -l` to see the size of the
     images within the built apk.

## Optimizing Android Resources
 * Use config-specific resource directories sparingly.
   * Introducing a new config has [a large cost][arsc-bloat].

[arsc-bloat]: https://medium.com/androiddevelopers/smallerapk-part-3-removing-unused-resources-1511f9e3f761#0b72

## Optimizing Code

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
     this for error-related strings.

### Optimizing Native Code
 * If there's a notable increase in `.data.rel.ro`:
   * Ensure there are not [excessive relocations][relocations].
 * If there's a notable increase in `.rodata`:
   * See if it would make sense to compress large symbols here by moving them to
     .pak files.
   * Gut-check that all unique string literals being added are actually useful.
 * If there's a notable increase in `.text`:
   * If there are a lot of symbols from C++ templates, try moving functions
     that don't use template parameters to
     [non-templated helper functions][template_bloat]).
     * And extract parts of functions that don't use them into helper functions.
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

### Optimizing Java Code
 * Prefer fewer large JNI calls over many small JNI calls.
 * Minimize the use of class initializers (`<clinit>()`).
   * If R8 cannot determine that they are "trivial", they will prevent
     inlining of static members on the class.
   * In C++, static objects are created at compile time, but in Java they
     are created by executing code within `<clinit>()`. There is often little
     advantage to initializing class fields statically vs. upon first use.
 * Don't use default interface methods on interfaces with multiple implementers.
   * Desugaring causes the methods to be added to every implementor separately.
   * It's more efficient to use a base class to add default methods.
 * Use `String.format()` instead of concatenation.
   * Concatenation causes a lot of StringBuilder code to be generated.
 * Try to use default values for fields rather than explicit initialization.
   * E.g. Name booleans such that they start as "false".
   * E.g. Use integer sentinels that have initial state as 0.
 * Minimize the number of callbacks / lambdas that each API requires.
   * Each callback / lambda is syntactic sugar for an anonymous class, and all
     classes have a constructor in addition to the callback method.
   * E.g. rather than have `onFailure()` vs `onSuccess()`, have an
     `onFinished(bool)`.
   * E.g. rather than have `onTextChanged()`, `onDateChanged()`, ..., have a
     single `onChanged()` that assumes everything changed.
 * Ensure unused code is optimized away by ProGuard / R8.
   * Add `@CheckDiscard` to methods or classes that you expect R8 to inline.
   * Add `@RemovableInRelease` to force a method to be a no-op when DCHECKs
     are disabled.
   * See [here][proguard-build-doc] for more info on how Chrome uses ProGuard.

[proguard-build-doc]: /build/android/docs/java_optimization.md
[size-trybot]: /tools/binary_size/README.md#Binary-Size-Trybot-android_binary_size
[diagnose_bloat]: /tools/binary_size/README.md#diagnose_bloat_py
[relocations]: /docs/native_relocations.md
[template_bloat]: https://bugs.chromium.org/p/chromium/issues/detail?id=716393
[supersize-console]: /tools/binary_size/README.md#Usage_console

## Optimizing Third-Party Android Dependencies

 * Look through SuperSize symbols to see whether unwanted functionality
   is being pulled in.
   * Use ProGuard's [-whyareyoukeeping] to see why unwanted symbols are kept
     (e.g. to [//base/android/proguard/chromium_apk.flags](/base/android/proguard/chromium_apk.flags)).
   * Try adding [-assumenosideeffects] rules to strip out unwanted calls
     (equivalent to adding @RemovableInRelease annotations).
 * Consider removing all resources via `strip_resources = true`.
 * Remove specific drawables via `resource_blacklist_regex`.

[-whyareyoukeeping]: https://r8-docs.preemptive.com/#keep-rules
[-assumenosideeffects]: https://r8-docs.preemptive.com/#general-rules
## Size Optimization Help

 * Feel free to email [binary-size@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/binary-size).
