# Local Modification: androidx.graphics.core

This directory contains the C++ files for `androidx.graphics:graphics-core` so
that we can compile them as part of `libchrome.so`.

## Structure

- `cpp/`: Contains C++ sources downloaded from AOSP.
  - `graphics-core.cpp` is modified to rename `JNI_OnLoad` to
    `GraphicsCore_JNI_OnLoad` to avoid duplicate symbol errors when linking
    into `libchrome.so`.
- `graphics_core_jni_initializer.cpp` implements the JNI entry point to
  trigger native registration via JNI Zero.
- `java/`: Contains our local JNI bridge `GraphicsCoreJniLoader.java`.
  - We do NOT compile the AndroidX Java/Kotlin sources locally.
  - Instead, we use a bytecode rewriter (`SystemLoadLibraryRemover`) configured
    in `customizations.gni` to remove `System.loadLibrary("graphics-core")`
    calls from the prebuilt AAR.
  - The bytecode rewriter replaces these calls with a call to
    `org.chromium.androidx.GraphicsCoreJniLoader.ensureInitialized()`.
  - `GraphicsCoreJniLoader.java` uses JNI Zero to call into our C++
    initializer.

## Updating the Source

To update the C++ files from upstream, run the update script:

```bash
./update_source.sh
```

This script will download the C++ files from AOSP (using the `androidx-main`
branch) and apply the necessary modifications.
