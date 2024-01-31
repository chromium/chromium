# Shared Libraries on Android
This doc outlines some tricks / gotchas / features of how we ship native code in
Chrome on Android.

[TOC]

## Library Packaging
 * Android N, O & P (MonochromePublic.aab):
   * `libmonochrome.so` is stored uncompressed within the apk (an
     AndroidManifest.xml attribute disables extraction).
   * It is loaded directly from the apk by the system linker.
   * It exports all JNI symbols and does not use explicit JNI registration.
   * It is not loaded by `libchromium_android_linker.so` and relies on the
     system's webview zygote for RELRO sharing.
 * Android Q (TrichromeChrome.aab + TrichromeLibrary.apk):
   * Trichrome uses the exact same native library as Monochrome:
     `libmonochrome.so`.
   * `libmonochrome.so` is stored in the shared APK (TrichromeLibrary.apk)
     so that it can be shared with TrichromeWebView.
   * It is loaded by `libchromium_android_linker.so` using
     `android_dlopen_ext()` to enable RELRO sharing.

## Build Variants (eg. monochrome_64_32_apk)
The packaging above extends to cover both 32-bit and 64-bit device
configurations.

Chrome support 64-bit builds, but these do not ship to Stable.
The system WebView APK that ships to those devices contains a 32-bit library,
and for 64-bit devices, a 64-bit library as well (32-bit WebView client apps
will use the 32-bit library, and vice-versa).

### Monochrome
Monochrome's intent was to eliminate the duplication between the 32-bit Chrome
and WebView libraries (most of the library is identical). In 32-bit Monochrome,
a single combined library serves both Chrome and WebView needs. The 64-bit
version adds an extra WebView-only library.

More recently, additional Monochrome permutations have arrived. First, Google
Play will eventually require that apps offer a 64-bit version to compatible
devices. In Monochrome, this implies swapping the architecture of the Chrome and
WebView libraries (64-bit combined lib, and extra 32-bit WebView lib). Further
down the road, silicon vendors may drop 32-bit support from their chips, after
which a pure 64-bit version of Monochrome will apply. In each of these cases,
the library name of the combined and WebView-only libraries must match (an
Android platform requirement), so both libs are named libmonochrome.so (or
libmonochrome_64.so in the 64-bit browser case).

Since 3 of these variations require a 64-bit build config, it makes sense to
also support the 4th variant on 64-bit, thus allowing a single builder to build
all variants (if desired). Further, a naming scheme must exist to disambiguate
the various targets:

**monochrome_(browser ABI)_(extra_webview ABI)**

For example, the 64-bit browser version with extra 32-bit WebView is
**monochrome_64_32_apk**. The combinations are as follows:

Builds on | Variant | Description
--- | --- | ---
32-bit | monochrome | The original 32-bit-only version
64-bit | monochrome | The original 64-bit version, with 32-bit combined lib and 64-bit WebView. This would be named monochrome_32_64_apk if not for legacy naming.
64-bit | monochrome_64_32 | 64-bit combined lib with 32-bit WebView library.
64-bit | monochrome_64 | 64-bit combined lib only, for eventual pure 64-bit hardware.
64-bit | monochrome_32 | A mirror of the original 32-bit-only version on 64-bit, to allow building all products on one builder. The result won't be bit-identical to the original, since there are subtle compilation differences.

### Trichrome
Trichrome has the same 4 permutations as Monochrome, but adds another dimension.
Trichrome returns to separate apps for Chrome and WebView, but places shared
resources in a third shared-library APK. The table below shows which native
libraries are packaged where. Note that **dummy** placeholder libraries are
inserted where needed, since Android determines supported ABIs from the presence
of native libraries, and the ABIs of a shared library APK must match its client
app.

Builds on | Variant | Chrome | Library | WebView
--- | --- | --- | --- | ---
32-bit | trichrome | `32/dummy` | `32/combined` | `32/dummy`
64-bit | trichrome | `32/dummy`, `64/dummy` | `32/combined`, `64/dummy` | `32/dummy`, `64/webview`
64-bit | trichrome_64_32 | `32/dummy`, `64/dummy` | `32/dummy`, `64/combined` | `32/webview`, `64/dummy`
64-bit | trichrome_64 | `64/dummy` | `64/combined` | `64/dummy`
64-bit | trichrome_32 | `32/dummy` | `32/combined` | `32/dummy`

## Crashpad Packaging
 * Crashpad is a native library providing out-of-process crash dumping. When a
   dump is requested (e.g. after a crash), a Crashpad handler process is started
   to produce a dump.
 * Chrome (Android L through M):
   * libchrome_crashpad_handler.so is a standalone executable containing all of
     the crash dumping code. It is stored compressed and extracted automatically
     by the system, allowing it to be directly executed to produce a crash dump.
 * Monochrome (N through P) and SystemWebView (L through P):
    * All of the Crashpad code is linked into the package's main native library
      (e.g. libmonochrome.so). When a dump is requested, /system/bin/app_process
      is executed, loading CrashpadMain.java which in turn uses JNI to call into
      the native crash dumping code. This approach requires building CLASSPATH
      and LD_LIBRARY_PATH variables to ensure app_process can locate
      CrashpadMain.java and any native libraries (e.g. system libraries, shared
      libraries, split apks, etc.) the package's main native library depends on.
 * Monochrome, Trichrome, and SystemWebView (Q+):
    * All of the Crashpad handler code is linked into the package's native
      library. libcrashpad_handler_trampoline.so is a minimal executable
      packaged with the main native library, stored uncompressed and left
      unextracted. When a dump is requested, /system/bin/linker is executed to
      load the trampoline from the APK, which in turn `dlopen()`s the main
      native library to load the remaining Crashpad handler code. A trampoline
      is used to de-duplicate shared code between Crashpad and the main native
      library packaged with it. This approach isn't used for P- because the
      linker doesn't support loading executables on its command line until Q.
      This approach also requires building a suitable LD_LIBRARY_PATH to locate
      any shared libraries Chrome/WebView depends on.

## Debug Information
**What is it?**
 * Sections of an ELF that provide debugging and symbolization information (e.g. ability convert addresses to function & line numbers).

**How we use it:**
 * ELF debug information is too big to push to devices, even for local development.
 * All of our APKs include `.so` files with debug information removed via `strip`.
 * Unstripped libraries are stored at `out/Default/lib.unstripped`.
   * Many of our scripts are hardcoded to look for them there.

## Unwind Info & Frame Pointers
**What are they:**
 * Unwind info is data that describes how to unwind the stack. It is:
   * It is required to support C++ exceptions (which Chrome doesn't use).
   * It can also be used to produce stack traces.
   * It is generally stored in an ELF section called `.eh_frame` & `.eh_frame_hdr`, but arm32 stores it in `.ARM.exidx` and `.ARM.extab`.
     * You can see these sections via: `readelf -S libchrome.so`
 * "Frame Pointers" is a calling convention that ensures every function call has the return address pushed onto the stack.
   * Frame Pointers can also be used to produce stack traces (but without entries for inlined functions).

**How we use them:**
 * We disable unwind information (search for [`exclude_unwind_tables`](https://cs.chromium.org/search/?q=exclude_unwind_tables+file:%5C.gn&type=cs)).
 * For all architectures except arm64, we disable frame pointers in order to reduce binary size (search for [`enable_frame_pointers`](https://cs.chromium.org/search/?q=enable_frame_pointers+file:%5C.gn&type=cs)).
 * Crashes are unwound offline using `minidump_stackwalk`, which can create a stack trace given a snapshot of stack memory and the unstripped library (see [//docs/testing/using_breakpad_with_content_shell.md](testing/using_breakpad_with_content_shell.md))
 * To facilitate heap profiling, we ship unwind information to arm32 canary & dev channels as a separate file: `assets/unwind_cfi_32`

## JNI Native Methods Resolution
 * For ChromePublic.apk:
   * `JNI_OnLoad()` is the only exported symbol (enforced by a linker script).
   * Native methods registered explicitly during start-up by generated code.
 * For MonochromePublic.apk and TrichromeChrome.aab:
   * `JNI_OnLoad()` and `Java_*` symbols are exported by linker script.
   * No manual JNI registration is done. Symbols are resolved lazily by the runtime.

## Packed Relocations
 * All flavors of `lib(mono)chrome.so` enable "packed relocations", or "APS2 relocations" in order to save binary size.
   * Refer to [this source file](https://android.googlesource.com/platform/bionic/+/refs/heads/master/tools/relocation_packer/src/delta_encoder.h) for an explanation of the format.
 * To process these relocations:
   * Pre-M Android: Our custom linker must be used.
   * M+ Android: The system linker understands the format.
 * To see if relocations are packed, look for `LOOS+#` when running: `readelf -S libchrome.so`
 * Android P+ [supports an even better format](https://android.googlesource.com/platform/bionic/+/8b14256/linker/linker.cpp#2620) known as RELR.
   * We'll likely switch non-Monochrome apks over to using it once it is implemented in `lld`.

## RELRO Sharing
**What is it?**
 * RELRO refers to the ELF segment `GNU_RELRO`. It contains data that the linker marks as read-only after it applies relocations.
   * To inspect the size of the segment: `readelf --segments libchrome.so`
   * For `lib(mono)chrome.so` the region occupies about 2.4MiB on arm32 and 4.7 MiB on arm64
 * If two processes map this segment to the same virtual address space, then pages of memory within the segment which contain only relative relocations (99% of them) will be byte-for-byte identical.
 * "RELRO sharing" is when this segment is moved into shared memory and shared by multiple processes.
 * Processes `fork()`ed from the app zygote (where the library is loaded) share RELRO (via `fork()`'s copy-on-write semantics), but this region is not shared with other process types (privileged, utility, GPU)

**How does it work?**
 * For a more detailed description, refer to comments in [Linker.java](https://cs.chromium.org/chromium/src/base/android/java/src/org/chromium/base/library_loader/Linker.java).
 * For Android N-P:
   * The OS maintains a RELRO file on disk with the contents of the GNU_RELRO segment.
   * All Android apps that contain a WebView load `libmonochrome.so` at the same virtual address and apply RELRO sharing against the memory-mapped RELRO file.
   * Chrome uses `WebViewLibraryPreloader` to call into the same WebView library loading code.
     * When Monochrome is the WebView provider, `libmonochrome.so` is loaded with the system's cached RELRO's applied.
   * `System.loadLibrary()` is called afterwards.
     * When Monochrome is the WebView provider, this only calls JNI_OnLoad, since the library is already loaded. Otherwise, this loads the library and no RELRO sharing occurs.
 * For non-low-end Android O-P (where there's a WebView zygote):
   * For non-renderer processes, the above Android N+ logic applies.
   * For renderer processes, the OS starts all Monochrome renderer processes by `fork()`ing the WebView zygote rather than the normal application zygote.
     * In this case, RELRO sharing would be redundant since the entire process' memory is shared with the zygote with copy-on-write semantics.
 * For Android Q+ (Trichrome):
   * TrichromeWebView works the same way as on Android N-P.
   * TrichromeChrome uses `android_dlopen_ext()` and `ASharedMemory_create()` to
     perform RELRO sharing, and then relies on a subsequent call to
     `System.loadLibrary()` to enable JNI method resolution without loading the
     library a second time.
   * For renderer processes, TrichromeChrome `fork()`s from a chrome-specific
     app zygote. `libmonochrome.so` is loaded in the zygote before `fork()`.
     * Similar to O-P, app zygote provides copy-on-write memory semantics so
       RELRO sharing is redundant.
 * For Android R+ (still Trichrome)
   * The RELRO region is created in the App Zygote, picked up by the Browser
     process, which then redistributes the region to all other processes. The
     receiving of the region and remapping it on top of the non-shared RELRO
     happens asynchronously after the library has been loaded. Native code is
     generally already running at this point. Hence the replacement must be
     atomic.

## Partitioned libraries
Some Chrome code is placed in feature-specific libraries and delivered via
[Dynamic Feature Modules](android_dynamic_feature_modules.md).

A linker-assisted partitioning system automates the placement of code into
either the main Chrome library or feature-specific .so libraries. Feature code
may continue to make use of core Chrome code (eg. base::) without modification,
but Chrome must call feature code through a virtual interface.

**How partitioning works**

The lld linker is now capable of producing a [partitioned
library](https://lld.llvm.org/Partitions.html), which is effectively an
intermediate single file containing multiple libraries. A separate tool
*(llvm-objcopy)* then splits the file into standalone .so files, invoked through
a [partitioned shared library](https://cs.chromium.org/chromium/src/build/partitioned_shared_library.gni)
GN template.

The primary partition is Chrome's main library (eg. libchrome.so), and other
partitions may contain feature code (eg. libvr.so). By specifying a list of
C/C++ symbols to use as entrypoints, the linker can collect all code used only
through these entrypoints, and place it in a particular partition.

To facilitate partitioning, all references from Chrome to the feature
entrypoints must be indirect. That is, Chrome must obtain a symbol from the
feature library through dlsym(), cast the pointer to its actual type, and call
through the resulting pointer.

Feature code retains the ability to freely call back into Chrome's core code.
When loading the library, the feature module system uses the feature name to
look up a partition name *(libfoo.so)* in an address offset table built into the
main library. The resulting offset is supplied to android_dlopen_ext(), which
instructs Android to load the library in a particular reserved address region.
This allows the feature library's relative references back to the main library
to work, as if the feature code had been linked into the main library
originally. No dynamic symbol resolution is required here.

**Implications on code placement**

* Any symbol referenced by multiple partitions ends up in the main library (even
  if all calling libraries are feature partitions).
* Symbols that aren't feature code (eg. base::) will be pulled into the
  feature's library if only that feature uses the code. This is a benefit, but
  can be unexpected.

**Builds that support partitioned libraries**

Partitioned libraries are usable when all of the following are true:
* Component build is disabled (component build splits code across GN component
  target boundaries instead).
* The compiler is Clang.
* The linker is lld.

## Library Prefetching
 * During start-up, we `fork()` a process that reads a byte from each page of the library's memory (or just the ordered range of the library).
   * See [//base/android/library_loader/](../base/android/library_loader/).

## Historical Tidbits
 * We used to use the system linker on M (`ModernLinker.java`).
   * This was removed due to [poor performance](https://bugs.chromium.org/p/chromium/issues/detail?id=719977).
 * We used to use `relocation_packer` to pack relocations after linking, which complicated our build system and caused many problems for our tools because it caused logical addresses to differ from physical addresses.
   * We now link with `lld`, which supports packed relocations natively and doesn't have these problems.
 * We used to use the Crazy Linker until Android M was deprecated
   * It allowed storing `libchrome.so` uncompressed within the apk before the
     system linker allowed it (with the name `crazy.libchrome.so` to avoid extraction).
   * It was loaded directly from the apk via `libchromium_android_linker.so`.
   * Only JNI_OnLoad was exported. Explicit JNI registration was required
     because the Android runtime uses the system's `dlsym()`, which doesn't know
     about Crazy-Linker-opened libraries. (see [JNI README](/third_party/jni_zero/README.md)).

## See Also
 * [//docs/android_build_instructions.md#Multiple-Chrome-APK-Targets](android_build_instructions.md#Multiple-Chrome-APK-Targets)
 * [//third_party/android_crazy_linker/README.chromium](../third_party/android_crazy_linker/README.chromium)
 * [//base/android/linker/BUILD.gn](../base/android/linker/BUILD.gn)
