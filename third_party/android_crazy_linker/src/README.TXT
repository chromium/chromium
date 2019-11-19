Introduction:
-------------

A custom dynamic linker for Android programs that adds a few interesting
features compared to /system/bin/linker:

  - Support changing the library search path. The system linker, when used
    inside Android applications, is limited to the value of LD_LIBRARY_PATH
    at boot time, that only looks into system directories, not application
    ones.

    This linker allows the client to add application paths before the
    default system ones, this has two benefits:

      - Library dependencies are loaded automatically in the right order.

      - Libraries from the application paths are favored over system ones.
        This avoids conflicts when one of your application's libraries
        has the same name than a system one (which happens randomly
        on certain devices due to system application bundling).

    (Note: The system linker issue above has been fixed in Android 4.3).

  - Supports any number of shared libraries. On older Android platforms,
    the system linker will refuse to load more than 64 or 128 libraries
    in a single process (Note: Fixed in Android 4.3).

  - Supports loading a library at an explicit (page-aligned) memory
    address. The system linker always randomizes the address. Note that
    this is generally a _bad_ idea for security reasons. Consider using
    this only when using shared RELROs (see below).

  - Supports loading a library from an explicit (page-aligned) file
    offset. This can be useful to load a library directly from an .apk,
    provided that it is uncompressed and at a page-aligned offset.

  - Support sharing of RELRO sections. When two processes load the same
    library at exactly the same address, the content of its RELRO section
    is identical. By default, each instance uses private RAM pages to host
    it, but it is possible to use a single ashmem region to share the same
    data instead.

See include/crazy_linker.h for the API and its documentation.

See LICENSE file for full licensing details (hint: BSD)

A few notes:

  - Do not use this if you don't know what you're doing. Read the API
    documentation first, and look at the test programs for usage examples.

  - The crazy linker will always use the system linker to load NDK-exposed
    system libraries (e.g. liblog.so and others). This avoids having two
    instances of the same library in the same process, and correctly
    resolving any symbols from system libraries.

  - Any library loaded by the crazy linker, and which uses functions of
    libdl.so will continue to work. However, calls to dlopen(), dlsym(),
    et al will be redirected to the crazy linker's own wrappers.

    This ensures that if a library is loaded by the crazy linker, any of
    its dependencies will be loaded by it too.

  - Libraries loaded with the crazy linker are visible to GDB, or Breakpad,
    and stack unwinding / C++ exception propagation should just work.


Caveats:
--------

  You can't call the crazy_linker code directly from Java in your Android
  application (it's a static library). You need to put it into your own
  shared library, loaded with System.loadLibrary() instead (or alternatively,
  inside your NativeActivity's shared library).

  Also, libraries loaded with the crazy linker are not visible to the system
  one. In practice, it means that lazy native method lookup will not work. I.e.:

  The first time you call a native method like:

    'mypackage.MyClass.myNativeMethod()'

  The VM will look into existing native libraries with dlsym() for a
  function symbol named like:

    Java_mypackage_MyClass_myNativeMethod

  This will not work if the symbol is inside a library loaded with the
  crazy_linker.

  To work-around this, register the native methods explicitely
  in your JNI_OnLoad() by calling env->RegisterNatives() with the
  appropriate parameters.


Usage instructions:
-------------------

  1/ Add the following to your module definition in your project's Android.mk:

        LOCAL_STATIC_LIBRARIES := crazy_linker

  2/ Also Include the top-level crazy_linker Android.mk, as in:

        include /path/to/crazy_linker/Android.mk

  3/ In your C or C++ source:

        #include <crazy_linker.h>

    Read the header for API documentation.

  If your library implements native methods, it must explicitely register
  them with env->RegisterNatives() before they become usable.

BUGS & TODOs:
-------------

  - Libraries loaded by the crazy linker are not automatically closed when
    the process exits.

  - dlopen() when called inside a library loaded by the crazy linker doesn't
    support RTLD_MAIN or RTLD_NEXT.

Testing:
--------

  See android_linker_testing.md

  See DESIGN.TXT for an overview of the library's design.
