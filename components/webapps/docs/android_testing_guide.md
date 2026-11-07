# Testing Web Apps on Android

This document describes how to test Android Web Apps (WebAPKs and TWAs) manually
and via automated tests.

## Manual Testing on Emulator

To test manually using an emulator:

### 1. Setup a Test PWA

You can use a public PWA (like `https://pokedex.org`) or serve one locally.

- **Using a public PWA**: No server setup needed! Just use the URL
  `https://pokedex.org`.
- **Serving locally**:
  1. Use Python to serve a directory with a manifest:
     `python3 -m http.server 8000`.
  2. Forward the port to the emulator: `adb reverse tcp:8000 tcp:8000`.

### 2. Disable Verification for Testing

To trust a TWA or WebAPK during local testing with developer builds, you must
bypass Chrome's security checks.

#### For TWAs (Digital Asset Links)

Chrome requires DAL verification to run a TWA in app mode. To bypass this for a
test URL:

- Pass the flag to Chrome:
  `--disable-digital-asset-link-verification-for-url="https://pokedex.org"`
- Example using build wrapper:
  `out/<Dir>/bin/chrome_public_apk launch --args="--disable-digital-asset-link-verification-for-url=https://pokedex.org"`
- Example using adb command line (requires "Enable command line on non-rooted
  devices" in `chrome://flags`):
  `adb shell "echo '_ --disable-digital-asset-link-verification-for-url=https://pokedex.org' > /data/local/tmp/chrome-command-line"`

#### For WebAPKs (Signature Verification)

Chrome verifies that WebAPKs are signed by the trusted WebAPK minting server.
Locally built debug WebAPKs will fail this check. To bypass this:

- Pass the flag to Chrome: `--skip-webapk-verification`
- Example using build wrapper:
  `out/<Dir>/bin/chrome_public_apk launch --args="--skip-webapk-verification"`

### 3. Install a Test TWA (via Bubblewrap)

You can use [Bubblewrap](https://github.com/GoogleChromeLabs/bubblewrap) to
generate an APK for your test site and install it:
`adb install path/to/generated.apk`

### 4. Install a Test WebAPK (via Chrome)

To test WebAPK installation:

1. Open Chrome on the emulator.
2. Navigate to your test PWA URL (e.g., `https://pokedex.org`).
3. Wait for the install prompt or select "Install app" from the Chrome menu.
4. Verify that the app is installed and appears in the app drawer.

### 5. Verify in Chrome Site Settings

1. Open the installed TWA or WebAPK on the emulator.
2. Go to Chrome > Settings > Site Settings > All Sites.
3. Look for your test site and see if it shows that it is managed by an app or
   has delegated permissions.

## Automated Tests

Chromium has several test suites that cover Web App functionality on Android. To
keep this guide maintainable, tests are grouped by directory and feature area.

Run any test file or directory (Java instrumentation, host-side Robolectric
JUnit, or C++ unit tests) with `tools/autotest.py`, which automatically selects
and builds the matching test target:

```bash
tools/autotest.py -C out/<Dir> <path/to/test_file_or_directory>
```

### 1. Instrumentation Tests (Java/JUnit4)

These tests run on an emulator or physical device to test UI, integration, and
lifecycle. Unlike host-side JUnit tests below, instrumentation tests cannot run
without an active device.

#### Emulator Prerequisite

To start the recommended local development emulator (x64 image, emulator window,
and networking enabled):

```bash
tools/android/avd/avd.py start -v \
    --avd-config tools/android/avd/proto/android_google_apis_x64_local.textpb \
    --emulator-window --no-read-only --enable-network
```

For more options and configuration details, see:

- [Using an Android Emulator](/docs/android_emulator.md)

- [Clank Noogler Guide: Step 5 - Start an emulator](http://go/clank-noogler/get_the_code_and_build_it.md#step-5-start-an-emulator)
  (Google internal)

- [Clank Android Emulator Tips](http://go/clank-emulator) (Google internal)

#### Coverage and Limitations by App Type

1. **WebAPKs**
   ([`chrome/android/javatests/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/webapps/)):

   - **What it can test**:
     - **Full install-to-launch and intent routing**: Browser installation, deep
       linking, browser-to-app intent trampolining, and cross-UID WebAPK service
       binding.
     - **Cross-package OS integration**: `chrome_public_test_apk` pre-installs a
       standalone test WebAPK (`org.chromium.webapk.test`) in its own Android
       package and UID, so full integration launches genuinely cross package and
       UID boundaries through the shell APK into Chrome and associate
       `WebappActivity` with a non-Chrome package in `PackageManager`.
     - **Manifest update detection**: Startup update detection and
       update-request serialization.
     - **Standalone UI and lifecycle**: Post-install `WebappActivity` UI and
       lifecycle bound to `org.chromium.webapk.test` metadata.
   - **What it cannot test**:
     - **Server APK minting and OS installation swaps**: Real WebAPK server APK
       generation and Google Play or OS `PackageInstaller` installation and
       update swaps (the test WebAPK is pre-installed before tests run, so
       installation completes once the request is handled).
     - **Production cryptographic signature validation**: End-to-end production
       minting-key verification (test APKs use local build keys, so production
       signature checks are covered separately in unit tests).
     - **Dynamic shell APK manifest attributes**: Manifest properties that must
       be statically compiled into the test WebAPK's Android manifest (such as
       arbitrary URL scopes, arbitrary Android runtime permissions, or cross-UID
       file-upload share targets).

2. **Client-App Trusted Web Activities (TWAs)**
   ([`chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/)):

   - **What it can test**:
     - **Post-launch TWA UI and navigation**: Standalone top-controls hiding
       versus out-of-scope custom tab toolbar fallback, "Running in Chrome"
       disclosures, orientation, edge-to-edge layout, text Web Share Target
       navigation, and page verification state transitions.
     - **Bound service delegation IPC**: Cross-UID service IPC against the
       lightweight helper service APK (`org.chromium.chrome.tests.support`),
       including
       [`permissiondelegation/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/permissiondelegation/)
       and
       [`digitalgoods/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/digitalgoods/).
     - **App Links verification queries**: `PackageManager` App Links checks.
   - **What it cannot test**:
     - **Separate non-Chrome client app launch and identity**: Launching a TWA
       from an independent third-party client APK and UID. TWA activity tests
       originate inside Chrome's own package and UID (bypassing any external
       client APK launcher) and spoof Digital Asset Links verification in
       memory, while the `org.chromium.chrome.tests.support` helper APK only
       provides bound test services and a deep-link resolution stub (with no TWA
       launcher activity).
     - **Cross-UID intent, session, and URI security boundaries**: Real external
       app intent dispatch, cross-UID session ownership, and cross-UID content
       URI permission grants on shared or opened files.
     - **Third-party runtime permission and task isolation**: Separate-package
       OS runtime permission isolation, independent client task roots and back
       stacks, and live Digital Asset Links network fetches.

3. **Auto-Minted TWAs**
   ([`chrome/android/javatests/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/webapps/)):

   - **What it can test**:
     - **Browser install pipeline and standalone UI**: Browser-side
       installability checks, install coordinator routing, handoff to the system
       installer bridge (validating the manifest URL and title passed toward the
       OS), and verified standalone TWA presentation after simulated install
       completion.
   - **What it cannot test**:
     - **OS `WebAppManager` minting and install confirmation**: Real OS
       `WebAppManager` APK generation, package installation, and interactive OS
       install-confirmation UI.
     - **Separate-package execution and security boundaries**: Launching an
       OS-minted non-Chrome TWA package and UID (post-install launch shares the
       same Chrome-originating TWA limitations as client-app TWA tests above).

4. **Install Prompts & Bottom Sheets**
   ([`chrome/android/javatests/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/webapps/)):

   - **What it can test**:
     - **Installability and prompt routing**: Manifest installability
       evaluation, bottom-sheet rendering, and Universal Install dialog routing
       (install versus shortcut versus opening an existing app).
   - **What it cannot test**:
     - **Post-trigger installation and launch**: Installing or launching an app
       package beyond the dialog trigger boundary.

### 2. Host-Side JUnit Tests (Java)

These Robolectric unit tests run on the development host JVM without requiring
an emulator or device.

- **What they can test**: Fast JVM verification of registration, storage, update
  state machines, install UI mediators, and auto-minted TWA installer logic
  against mocked OS services.
- **What they cannot test**: Native C++ or Blink behavior, real `Activity`
  lifecycles, or multi-process Android UID and Binder enforcement.
- **Directories**:
  - [`chrome/android/junit/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/junit/src/org/chromium/chrome/browser/webapps/):
    WebAPK and webapp registration, storage, lifecycle, and update logic.
  - [`chrome/android/junit/src/org/chromium/chrome/browser/browserservices/`](../../../chrome/android/junit/src/org/chromium/chrome/browser/browserservices/)
    (including `permissiondelegation/` and `ui/`): Trusted Web Activity
    verification, delegation, and UI controllers.
  - [`components/webapps/browser/android/java/src/org/chromium/components/webapps/`](../browser/android/java/src/org/chromium/components/webapps/):
    Install UI mediators, web app utilities, and auto-minted TWA installer unit
    tests.

### 3. WebAPK Shell & Client Library Tests

These tests cover the standalone WebAPK shell APK and client runtime libraries
in isolation from full Chrome and Blink integration (see
[`chrome/android/webapk/README.md`](../../../chrome/android/webapk/README.md)
for specialized shell APK details).

- **What they can test**: Shell APK launch trampolines, service IPC, splash
  screens, and cryptographic APK signature validation.
- **What they cannot test**: Full Chrome browser tab hosting, Blink manifest
  parsing, or end-to-end browser-to-server update pipelines.
- **Directories**:
  - [`chrome/android/webapk/libs/client/junit/`](../../../chrome/android/webapk/libs/client/junit/)
    (Host-side JUnit)
  - [`chrome/android/webapk/libs/runtime_library/javatests/`](../../../chrome/android/webapk/libs/runtime_library/javatests/)
    (Instrumentation)
  - [`chrome/android/webapk/shell_apk/junit/`](../../../chrome/android/webapk/shell_apk/junit/)
    (Host-side JUnit)
  - [`chrome/android/webapk/shell_apk/javatests/`](../../../chrome/android/webapk/shell_apk/javatests/)
    (Instrumentation)

### 4. C++ Unit Tests (Android-specific)

These tests cover shared native installability and WebAPK data pipelines in
isolation from the Android Java framework, `Activity` lifecycles, and
`PackageManager`.

- **What they can test**: Native installability evaluation, app banner
  promotion, WebAPK proto building, icon hashing, and install prompt settings.
- **What they cannot test**: Android Java UI, `Activity` launches,
  `PackageManager` queries, or OS `WebAppManager` service calls.
- **Directories**:
  - [`components/webapps/browser/android/`](../browser/android/)
  - [`components/webapps/browser/banners/`](../browser/banners/) (Shared, but
    relevant to Android promotion)
  - [`components/webapps/browser/installable/`](../browser/installable/)
    (Shared, but relevant to Android installability)
