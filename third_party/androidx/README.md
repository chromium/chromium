# AndroidX Libraries

See also: [//third_party/android_deps/README.md](/third_party/android_deps/README.md).

## Mapping snapshot version to git revision

* Googlers can figure this out via go/clank-autoroll#androidx-regression-range

## Viewing Source Code:

* Git Viewer: https://android.googlesource.com/platform/frameworks/support/
* Code Search: https://cs.android.com/androidx/platform/frameworks/support

## How to add a new androidx library

1. Add the gradle entry for the desired target to `build.gradle.template`
2. Do a trial run (downloads files locally):
   ```
   third_party/androidx/fetch_all_androidx.py --local
   ```
3. Assuming it works fine, upload & submit your change to `build.gradle.template`
4. Wait for the [packager] and [auto-roller] to run (or [trigger the packager manually] to expedite)

[packager]: https://ci.chromium.org/ui/p/chromium/builders/ci/android-androidx-packager
[auto-roller]: https://autoroll.skia.org/r/androidx-chromium
[trigger the packager manually]: https://luci-scheduler.appspot.com/jobs/chromium/android-androidx-packager
