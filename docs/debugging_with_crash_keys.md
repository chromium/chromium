# Debugging with Crash Keys

Chrome is client-side software, which means that sometimes there are bugs that
can occur only on users' machines ("in production") that cannot be reproduced by
test or software engineering. When this happens, it's often helpful to gather bug-
specific data from production to help pinpoint the cause of the crash. The crash
key logging system is a generic method to help do that.

[TOC]

## High-Level Overview

The core of the crash key logging system is in [//components/crash/core/common/crash_key.h](https://cs.chromium.org/chromium/src/components/crash/core/common/crash_key.h),
which declares a `crash_reporter::CrashKeyString` class. Every crash key has
an associated value maximum length and a string name to identify it. The maximum
length is specified as a template parameter in order to allocate that amount of
space for the value up-front. When a process is crashing, memory corruption can
make it unsafe to call into the system allocator, so pre-allocating space for
the value defends against that.

When a crash key is set, the specified value is copied to its internal storage.
And if the process subsequently crashes, the name-value tuple is uploaded as
POST form-multipart data when the crash report minidump is uploaded to the
Google crash reporting system. (The data therefore are only accessible to those
with access to crash reports internally at Google). For platforms that use
[Crashpad](https://crashpad.chromium.org) as the crash reporting platform, the
crash keys are also stored in the minidump file itself. For platforms that use
Breakpad, the keys are only available at upload.

The crash key system is used to report some common pieces of data, not just
things that happen in exceptional cases: the URL of the webpage, command line
switches, active extension IDs, GPU vendor information, experiment/variations
information, etc.

## Redaction

Beware that certain on certain platforms (e.g. Android WebView) we
[sanitize the stack in the dump](https://cs.chromium.org/chromium/src/third_party/crashpad/crashpad/snapshot/sanitized/memory_snapshot_sanitized.h)
and only crash keys on an
[allowlist](https://cs.chromium.org/chromium/src/android_webview/common/crash_reporter/crash_keys.cc)
will be captured.

## Getting Started with a Single Key-Value Pair

Imagine you are investigating a crash, and you want to know the value of some
variable when the crash occurs; the crash key logging system enables you to do
just that.

#### 1. Declare the Crash Key

A crash key must be allocated using static storage duration, so that there is
space for the value to be set. This can be done as a static variable in the
global or function scope, or in an anonymous namespace:

    static crash_reporter::CrashKeyString<32> crash_key_one("one");

    namespace {
    crash_reporter::CrashKeyString<64> crash_key_two("two");
    }

    void DoSomething(const std::string& arg) {
      static crash_reporter::CrashKeyString<8> three("three");
      crash_key_two.Set(arg);
      three.Set("true");
    }

The template argument specifies the maximum length a value can be, and it
should include space for a trailing NUL byte. Values must be C-strings and
cannot have embedded NULs. The constructor argument is the name of the
crash key, and it is what you will use to identify your data in uploaded
crash reports.

Note that crash key names are global and must not conflict with the
name of any other crash key in Chrome.

If you need to declare an array of crash keys (e.g., for recording N values
of an array), you can use a constructor tag to avoid warnings about `explicit`:

    static ArrayItemKey = crash_reporter::CrashKeyString<32>;
    static ArrayItemKey crash_keys[] = {
      {"array-item-1", ArrayItemKey::Tag::kArray},
      {"array-item-2", ArrayItemKey::Tag::kArray},
      {"array-item-3", ArrayItemKey::Tag::kArray},
      {"array-item-4", ArrayItemKey::Tag::kArray},
    };

The crash key system will require your target to have a dependency on
`//components/crash/core/common:crash_key`. If you encounter link errors for
unresolved symbols to `crashpad::Annotation::SetSize(unsigned int)`, adding
the dependency will resolve them.

#### 2. Set the Crash Key

After a key has been allocated, its `Set(base::StringPiece)` and
`Clear()` methods can be used to record and clear a value. In addition,
crash_key.h provides a `ScopedCrashKeyString` class to set the value for the
duration of a scope and clear it upon exiting.

#### 3. Seeing the Data

Using <http://go/crash> (internal only), find the crash report signature related
to your bug, and click on the "N of M" reports link to drill down to
report-specific information. From there, select a report and go to the
"Product Data" section to view all the crash key-value pairs.

## Dealing with DEPS

Not all targets in the Chromium source tree are permitted to depend on the
`//components/crash/core/common:crash_key` target due to DEPS file
`include_rules`.

If the crash key being added is only a temporary debugging aid to track down a
crash, consider adding the dependency temporarily and removing it when done.
A specific include rule can be added for crash_key.h:

    # DEPS
    include_rules = [
      '+components/crash/core/common/crash_key.h',
    ]

Then simply remove it (and the BUILD.gn dependency) once the crash is resolved
and the crash key deleted.

If this crash key is more permanent, then there is an alternate API in //base
that can be used. This API is used by the //content module to set its permanent
crash key information. Note however that the base-level API is more limited in
terms of features and flexibility. See the header documentation in
[//base/debug/crash_logging.h](https://cs.chromium.org/chromium/src/base/debug/crash_logging.h)
for usage examples.

## Advanced Topics: Stack Traces

Now imagine a scenario where you have a use-after-free. The crash reports coming
in do not indicate where the object being used was initially freed, however,
just where it is later being dereferenced. To make debugging easier, it would be
nice to have the stack trace of the destructor, and the crash key system works
for that, too.

#### 1. Declare the Crash Key

Declaring the crash key is no different than written above, though special
attention should be paid to the maximum size argument, which will affect the
number of stack frames that are recorded. Typically a value of _1024_ is
recommended.

#### 2. Set the Crash Key

To set a stack trace to a crash key, use the `SetCrashKeyStringToStackTrace()`
function in crash_logging.h:

    Usemeafterfree::~Usemeafterfree() {
      static crash_reporter::CrashKeyString<1024> trace_key(
          "useme-after-free-uaf-dtor-trace");
      crash_reporter::SetCrashKeyStringToStackTrace(&trace_key,
                                                    base::debug::StackTrace());
    }

#### 3. Seeing the Data

Unlike with the previous example, a stack trace will just be a string of
hexadecimal addresses. To turn the addresses back into symbols use,
<http://go/crsym> (internal instance of <https://github.com/chromium/crsym/>).
Using the **Crash Key** input type, give it a crash report ID and the name of
your crash key. Crsym will then fetch the symbol data from the internal crash
processing backends and return a formatted, symbolized stack trace.
