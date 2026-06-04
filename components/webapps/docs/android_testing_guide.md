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
  `out/Default/bin/chrome_public_apk launch --args="--disable-digital-asset-link-verification-for-url=https://pokedex.org"`
- Example using adb command line (requires "Enable command line on non-rooted
  devices" in `chrome://flags`):
  `adb shell "echo '_ --disable-digital-asset-link-verification-for-url=https://pokedex.org' > /data/local/tmp/chrome-command-line"`

#### For WebAPKs (Signature Verification)

Chrome verifies that WebAPKs are signed by the trusted WebAPK minting server.
Locally built debug WebAPKs will fail this check. To bypass this:

- Pass the flag to Chrome: `--skip-webapk-verification`
- Example using build wrapper:
  `out/Default/bin/chrome_public_apk launch --args="--skip-webapk-verification"`

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

### 1. Instrumentation Tests (Java/JUnit4)

These tests run on an emulator or physical device to test UI, integration, and
lifecycle.

- **TWA and General Webapp UI/Lifecycle**:
  - **Directory**:
    [`chrome/android/javatests/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/webapps/)
  - **Features**: WebAPK launch/update integration, splash screens, display
    modes, display cutout, and default offline behavior.
- **Trusted Web Activities (TWA) & Permission Delegation**:
  - **Directory**:
    [`chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/`](../../../chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/)
    (and subdirectories like `permissiondelegation/`)
  - **Features**: TWA client verification, digital asset links, permission
    delegation (location, notifications, contacts), and post-launch
    verification.

**How to Run**: Build the test APK:
`autoninja -C out/Default chrome_public_test_apk`

Run all tests in a directory:
`out/Default/bin/run_chrome_public_test_apk -f "org.chromium.chrome.browser.webapps.*"`
`out/Default/bin/run_chrome_public_test_apk -f "org.chromium.chrome.browser.browserservices.*"`

### 2. Host-Side JUnit Tests (Java)

These unit tests run on the development host JVM and test business logic,
storage, and utility classes without requiring a device.

- **Registration, Storage, and Lifecycle Logic**:
  - **Directories**:
    - [`chrome/android/junit/src/org/chromium/chrome/browser/webapps/`](../../../chrome/android/junit/src/org/chromium/chrome/browser/webapps/)
    - [`chrome/android/junit/src/org/chromium/chrome/browser/browserservices/`](../../../chrome/android/junit/src/org/chromium/chrome/browser/browserservices/)
      (including `permissiondelegation/` and `ui/`)
  - **Features**: `InstalledWebappRegistrar`, `InstalledWebappDataRegister`,
    `WebappDataStorage`, and `WebApkSyncService`.

**How to Run**:
`out/Default/bin/run_chrome_junit_tests -f "org.chromium.chrome.browser.webapps.*"`
`out/Default/bin/run_chrome_junit_tests -f "org.chromium.chrome.browser.browserservices.*"`

### 3. WebAPK Shell & Client Library Tests

These tests focus on the WebAPK client-side library and the shell APK code.

- **Directories**:
  - [`chrome/android/webapk/libs/client/junit/`](../../../chrome/android/webapk/libs/client/junit/)
    (Host-side JUnit)
  - [`chrome/android/webapk/libs/runtime_library/javatests/`](../../../chrome/android/webapk/libs/runtime_library/javatests/)
    (Instrumentation)
  - [`chrome/android/webapk/shell_apk/junit/`](../../../chrome/android/webapk/shell_apk/junit/)
    (Host-side JUnit)
  - [`chrome/android/webapk/shell_apk/javatests/`](../../../chrome/android/webapk/shell_apk/javatests/)
    (Instrumentation)
- **Features**: WebAPK connection management, identity service, shell APK launch
  flows, and splash screens.

**How to Run**: Check
[`chrome/android/webapk/README.md`](../../../chrome/android/webapk/README.md)
for instructions on running these specialized tests.

### 4. C++ Unit Tests (Android-specific)

These tests cover the shared native code integration.

- **Directories**:
  - [`components/webapps/browser/android/`](../browser/android/)
  - [`components/webapps/browser/banners/`](../browser/banners/) (Shared, but
    relevant to Android promotion)
  - [`components/webapps/browser/installable/`](../browser/installable/)
    (Shared, but relevant to Android installability)
- **Features**: `Add-to-homescreen` data fetching, WebAPK proto building, icon
  hashing, and install prompt settings.

**How to Run**: Build and run `components_unittests` on Android:
`autoninja -C out/Default components_unittests`
`out/Default/bin/run_components_unittests --gtest_filter="*AddToHomescreen*"`
