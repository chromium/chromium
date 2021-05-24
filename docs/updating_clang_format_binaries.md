# Updating Clang format binaries

Instructions on how to update the [clang-format binaries](clang_format.md) that
come with a checkout of Chromium.

## Prerequisites

You'll need a Windows machine, a Linux machine, and a Mac; all capable of
building clang-format. You'll also need permissions to upload to the appropriate
google storage bucket. Chromium infrastructure team members have this, and
others can be granted the permission based on need. Talk to ncarter or hinoka
about getting access.

## Pick a head svn revision

Consult http://llvm.org/svn/llvm-project/ for the current head revision. This
will be the CLANG_REV you'll use later to check out each platform to a
consistent state.

## Build a release-mode clang-format on each platform

Follow the official instructions here:
http://clang.llvm.org/get_started.html.

Windows step-by-step:

```shell
# [double check you have the tools you need]
where cmake.exe  # You need to install this.

# In chromium/src
tools\win\setenv amd64_x86
set CLANG_REV=56ac9d30d35632969baa39829ebc8465ed5937ef  # You must change this value (see above)
rmdir /S /Q llvm-project
git clone https://github.com/llvm/llvm-project
cd llvm-project
git checkout %CLANG_REV%
mkdir build
cd build
set CC=..\..\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe
set CXX=..\..\third_party\llvm-build\Release+Asserts\bin\clang-cl.exe
set CFLAGS=-m32
set CXXFLAGS=-m32
cmake -G Ninja ..\llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_USE_CRT_RELEASE=MT ^
    -DLLVM_ENABLE_ASSERTIONS=NO -DLLVM_ENABLE_THREADS=NO ^
    -DLLVM_ENABLE_PROJECTS=clang
ninja clang-format
bin\clang-format.exe --version
```

Mac & Linux step-by-step:

```shell
# Check out.
export CLANG_REV=56ac9d30d35632969baa39829ebc8465ed5937ef   # You must change this value (see above)
git clone https://github.com/llvm/llvm-project
cd llvm-project
git checkout $CLANG_REV
mkdir build
cd build

# On Mac, do the following:
MACOSX_DEPLOYMENT_TARGET=10.9 cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_ENABLE_ASSERTIONS=NO \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DLLVM_ENABLE_THREADS=NO \
    -DLLVM_ENABLE_ZLIB=OFF \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' \
    ../llvm/

# On Linux, do the following:
# Note the relative paths that point to your local Chromium checkout.
# TODO(thakis): Remove DLLVM_ENABLE_Z3_SOLVER in the next roll. At the pinned
# revision, Z3 detection does not work with a sysroot, but at LLVM trunk it's
# already fixed.
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DLLVM_ENABLE_ASSERTIONS=NO \
    -DLLVM_ENABLE_TERMINFO=OFF \
    -DLLVM_ENABLE_THREADS=NO \
    -DLLVM_ENABLE_ZLIB=OFF \
    -DLLVM_ENABLE_Z3_SOLVER=NO \
    -DCMAKE_C_COMPILER=$HOME/src/chrome/src/third_party/llvm-build/Release+Asserts/bin/clang \
    -DCMAKE_CXX_COMPILER=$HOME/src/chrome/src/third_party/llvm-build/Release+Asserts/bin/clang++ \
    -DCMAKE_ASM_COMPILER=$HOME/src/chrome/src/third_party/llvm-build/Release+Asserts/bin/clang \
    -DCMAKE_CXX_STANDARD_LIBRARIES="-static-libgcc -static-libstdc++" \
    -DCMAKE_SYSROOT=$HOME/src/chrome/src/build/linux/debian_sid_amd64-sysroot \
    ../llvm/

# Finally, build the actual clang-format binary with Ninja
ninja clang-format
strip bin/clang-format


```

Platform specific notes:

*   Windows: Visual Studio 2013 only.
*   Linux: so far (as of January 2014) we've just included a 64-bit binary. It's
    important to disable threading, else clang-format will depend on
    libatomic.so.1 which doesn't exist on Precise.
*   Mac: Remember to set `MACOSX_DEPLOYMENT_TARGET` when building! If you get
    configure warnings, you may need to install XCode 5 and avoid a goma
    environment.

## Upload each binary to google storage

Copy the binaries into your chromium checkout (under
`src/buildtools/(win|linux64|mac)/clang-format(.exe?)`). For each binary, you'll
need to run `upload_to_google_storage.py` according to the instructions in
[README.txt](https://chromium.googlesource.com/chromium/src/+/main/buildtools/clang_format/README.txt).
This will upload the binary into a publicly accessible google storage bucket,
and update `.sha1` file in your Chrome checkout. You'll check in the `.sha1`
file (but NOT the clang-format binary) into source control. In order to be able
to upload, you'll need write permission to the bucket -- see the prerequisites.

## Copy the helper scripts and update README.chromium

There are some auxiliary scripts that ought to be kept updated in lockstep with
the clang-format binary. These get copied into
`buildtools/clang_format/script` in your Chromium checkout.

The `README.chromium` file ought to be updated with version and date info.

## Upload a CL according to the following template

    Update clang-format binaries and scripts for all platforms.

    I followed these instructions:
    https://chromium.googlesource.com/chromium/src/+/main/docs/updating_clang_format_binaries.md

    The binaries were built at clang revision ####### on ####DATETIME####.

    Bug:

The change should **always** include new `.sha1` files for each platform (we
want to keep these in lockstep), should **never** include `clang-format`
binaries directly. The change should **always** update `README.chromium`

clang-format binaries should weigh in at 1.5MB or less. Watch out for size
regressions.
