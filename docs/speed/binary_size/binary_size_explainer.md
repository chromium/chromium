# Binary Size Explainer

This document explains the overhead of each kind of binary size, where
"overhead" is the performance cost of existing (e.g.  does not include the
overhead of loading an image, just the overhead of an image existing).  This
document focuses on Android, but several types of size are the same on other
platforms.

*** note
See [optimization_advice.md] for ideas on how to reduce
size.
***

[optimization_advice.md]: optimization_advice.md

[TOC]

## How big is Chrome?

* Chrome's per-commit size is tracked in [chromeperf](https://chromeperf.appspot.com/report):
  * For Android, look for "resource\_sizes".
    * Googlers, see also [go/chromeapksizes], and [go/chromemilestonesizes].
  * For other platforms: look for "sizes".
* As of 2019, Chrome for Android (arm32) grows by about 100kb per week.
* To get a feeling for how large individual features are, open a
  [milestone size breakdown] and group by "Component" (Googlers only).
  * For non-googlers, run `//tools/binary_size/supersize archive` on a release
    build to create a `.size` file, and upload it to [the viewer].

[go/chromeapksizes]: http://go/chromeapksizes
[go/chromemilestonesizes]: http://go/chromemilestonesizes
[milestone size breakdown]: https://goto.google.com/chrome-supersize
[the viewer]: https://chrome-supersize.firebaseapp.com/viewer.html

## Why care about binary size?

* Takes disk space away from users.
  * Much less so than Chrome's cache, but this is space that cannot be
    cleared, and on Android is displayed prominently in the Play Store and in
    system settings.
* Takes disk space on system images for Android and Chrome OS.
  * It routinely happens that system images fill up, and all components need
    to do their part to fit into the space.
* Binary size is a "proxy metric" for other performance metrics.
  * It's a metric that is trivial to measure, and correlates to RAM usage,
    start-up speed etc.  (there are nuances, which we try to capture in this
    doc).
* Binary size affects users' perception of Chrome's performance.
  * E.g. Large app size leads users to think Chrome is "bloated".
* Binary size is much easier to stay on top of than it is to retroactively fix.

Googlers - see [go/chromebinarysizematters] for more links to studies.

[go/chromebinarysizematters]: http://go/chromebinarysizematters

### How much effort should I spend to reduce binary size?

Chrome is currently not under any hard growth limits, but to mitigate
unnecessary bloat of this shared resource, everyone should ensure that
reasonable diligence is taken to minimize growth.

* "Reasonable diligence" requires judgement and varies depending on the change.
  A rough guideline:
  * For small (<50kb) increases: spend a day trying to reduce (unless it's obviously not possible).
  * For larger increases: Try to understand and document why the change requires so much size.
  * If you are unsure, reach out on [binary-size@] for advice.
* The [android-binary-size][size_trybot] trybot will alert for single commits that increase
  binary size on 32-bit Android by more than 16kb.
  * While such increases are often fine, this helps prevent unexpected
  increases.
* It typically takes about a week of engineering time to reduce Android's binary size by 50kb.

[binary-size@]: https://groups.google.com/a/chromium.org/g/binary-size/
[size_trybot]: /docs/speed/binary_size/android_binary_size_trybot.md

## How Chrome is Packaged

### WebView vs Chrome

Android WebView is used by the majority of Android apps, so overhead (other
than clean memory) introduced into WebView has a multiplicative effect on the
OS.  See
[android\_build\_instructions.md]
for how packaging of Chrome / WebView changes by OS version.

[android\_build\_instructions.md]: /docs/android_build_instructions.md#Multiple-Chrome-Targets

### APK Splits

Chrome ships as an [Android App Bundle], and consists of several APK splits.

[Android App Bundle]: /docs/android_dynamic_feature_modules.md#about-bundles

#### The "base" split

* Loaded on start-up by every process.
* Keeping its dex size minimal is crucial, since it has both RAM and start-up
  overhead _per-renderer_.

#### In the "chrome" feature split

* Loaded on start-up by the browser process.
* Important to keep dex size small in order to have Chrome start-up quickly,
  and to minimize our baseline memory requirement.

#### In another feature split

* Since they are loaded on-demand, binary size matters proportionally to the
  number of users that use the module.

#### In a DFM

* These are feature splits that are not even downloaded until needed. Binary
  size matters proportionally to the number of users that use the module.

## Types of Overhead

Here we define some terms used to describe overhead.

**Clean Memory:**

* Backed by a file on disk, and mapped to virtual memory via `mmap()`.
* Shared between processes.
* Paged into RAM on-demand by the OS.
* Fast when disk cache is hot, and slower when disk cache is cold.

**Dirty Memory:**

* Not backed by a file, and thus cannot be safely paged out (except to zram)

## Overhead for Each Type of Binary Size

### Native code Size

#### Code (.text)

Machine instructions.

* **RAM:** _Clean Memory_, with good locality thanks to PGO.
* **Start-up:** No effect beyond page faults.

#### Read-only Data (.rodata)

String literals, global constants, etc.

* **RAM:** _Clean Memory_, with poor locality (most is paged in after a short amount of time)
* **Start-up:** No effect beyond page faults.

#### Read-only-after-relocated Data (.data.rel.ro)

Read-only data that must be adjusted based on the base load address of the executable.

* **RAM:** _Dirty-ish Memory_. It's complicated.  Refer to
  [native_relocations.md].
* **Start-up:** Some overhead. Again, read the linked doc.

[native relocations.md]: /docs/native_relocations.md#why-do-they-matter

#### Mutable Data (.data)

Global non-const variables.

* **RAM:** _Dirty Memory_
* **Start-up:** The entire section is loaded before any code is run.

### Native resources (.pak)

UI Images, chrome:// pages, UI strings, etc.

* **RAM:** _Clean Memory_
* **Start-up:** Platform-dependent. None on Android. Page faults on desktop.
* Locality improved [using an orderfile] on some platforms.

[using an orderfile]: /tools/gritsettings/README.md#:~:text=internal%20translation%20process.-,startup_resources_%5Bplatform%5D.txt,-%3A%20These%20files%20provide

### Android: Java code size (.dex)

Java bytecode in the [DEX file format], stored in `classes.dex`, `classes2.dex`,
...  When Android installs Chrome, this bytecode is turned into machine code and
stored as `.odex` & `.vdex` files.  The size of these files depends on the OS
version and dex compilation profile.  Generally, they are 1x the size of
uncompressed `.dex` on Android Go, and 4x the size of uncompressed `.dex` on
other devices.

* **RAM:** Mostly _Clean Memory_, but Some _Dirty Memory_ as well.
  * E.g.: The number of method declarations (referred to as "method count")
    directly corresponds to dirty RAM, where 1 entry = 4 bytes (on arm32).
* **Start-up:** Impact proportional to overall size.  Mitigated by packaging
  code into [feature splits] when possible.

[DEX file format]: https://source.android.com/devices/tech/dalvik/dex-format
[feature splits]: /docs/android_dynamic_feature_modules.md#:~:text=files%2C%20known%20as%20%E2%80%9C-,feature%20splits,-%E2%80%9D.%20Feature%20splits%20have

#### More about Method Count

When changing `.java code`, the change in method count is
[shown in code reviews][size_trybot]. The count is the number of method
references added/removed after optimization.  Which methods are added/removed
can be seen in the "APK Breakdown" link by checking "Method Count Mode".

Method count is a useful thing to look at because:

* Each method reference has overhead within the dex file format, and for smallish methods, contributes more to binary size than its corresponding executable code.
* Method references that survive R8 optimization show how optimizable your abstractions are. Try to use low-overhead (or zero-overhead) abstractions. If you find that you're adding a lot of methods, you should see whether a different abstraction would result in fewer methods.

### Android: Resources

#### resources.arsc

All files within `res/`, as well as individual entries with `res/values` files
contribute to this file.  It consists of a string table, plus one 2D array
for each resource type (strings, drawable, etc).  The overhead (not included
actual data) for each table is: `# [unique configs] * # of resources * sizeof(entry)`.

* **RAM:** _Clean Memory_, with poor locality (most is paged in after a short
  amount of time)
* **Start-up:** No effect beyond page faults.
* Its table-based file format means it's important to keep the number of unique
  configs as small as possible.

[unique configs]: https://developer.android.com/guide/topics/resources/providing-resources#QualifierRules

#### res/... (drawables/layouts/xml)

Files that are packaged within the `res/` directory of the `.apk`.  These also
have an entry within `resources.arsc` that maps to them from a resource ID.

* **RAM:** None unless resources are accessed.
* **Start-up**: None unless resources are accessed.


### Other Assets

ICU data, V8 snapshot, etc.

* **RAM:** _Clean Memory_
* **Start-up:** No effect beyond page faults.
