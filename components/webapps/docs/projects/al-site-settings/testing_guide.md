# Testing TWA and WebAPK Registration

This document describes how to test the TWA and WebAPK registration and site
settings functionality manually and via automated tests.

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

### 2. Disable Asset Link Verification

Chrome requires Digital Asset Links verification to trust a TWA/WebAPK. For
testing, you must disable this check for your test URL:

- Pass the flag to Chrome (e.g., via `chrome://flags` or command line):
  `--disable-digital-asset-link-verification-for-url="https://pokedex.org"`
- Or if launching Chrome via the build wrapper script, use `--args`:
  `out/Default/bin/chrome_public_apk launch --args="--disable-digital-asset-link-verification-for-url=https://pokedex.org"`
- Or use adb to write to the command line file (requires "Enable command line on
  non-rooted devices" enabled in `chrome://flags` if not rooted):
  `adb shell "echo '_ --disable-digital-asset-link-verification-for-url=https://pokedex.org' > /data/local/tmp/chrome-command-line"`

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

You can run instrumentation tests that execute on the emulator and show the UI.

### Available Tests

These tests are located in
`chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/`:

- `TrustedWebActivityPreferencesUiTest.java`: Tests the "Managed by" UI in site
  settings.
- `ManageTrustedWebActivityDataActivityTest.java`: Tests the activity that
  launches site settings from the TWA.

### How to Run

Build the test APK (using `out/AndroidDesktop` for Desktop Android):
`autoninja -C out/AndroidDesktop chrome_public_test_apk`

Run the specific test:
`out/AndroidDesktop/bin/run_chrome_public_test_apk -f "*TrustedWebActivityPreferencesUiTest*"`
