# Chrome for Testing Differences

This document summarizes the functional and behavioral differences introduced
when Chrome is built with the `BUILDFLAG(CHROME_FOR_TESTING)` and
`BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)` build flags. The latter enables
Google Chrome for Testing specific branding. This affects application icons,
installation paths (e.g., for native messaging hosts and enterprise policies),
and enables proprietary media codecs (e.g., H.264, AAC, MP3). Without it Chrome
for Testing acts like Chromium for Testing.

### 1. Configuration
Chrome for Testing can be configured by supplying a JSON configuration file
using the `--chrome-for-testing-config` command line switch. Refer to
[docs/chrome_for_testing/chrome_for_testing_configuration.md](chrome_for_testing_configuration.md)
for more details.

### 2. Startup & Configuration
*   **Required Components Loading**: Chrome for Testing (CfT) relies on a
    dedicated JSON configuration loaded during early browser initialization via
    `chrome_for_testing::LoadConfig()`.
*   **Startup Synchronization**: It injects custom synchronization logic into
    `ChromeBrowserMainParts`. The browser explicitly blocks and delays regular
    startup until all "required components" specified in the CfT config are
    installed or updated to the current version.
*   **Component Updates Policy**: By default, automatic component updates are
    disabled (`kComponentUpdatesEnabledByDefault` is forced to false).

### 3. User Data Directory
Chrome for Testing stores its profile and browser state in a dedicated
user data directory location different from standard Chrome user data
directory location. This prevents test profiles from interfering with
day-to-day browser data. Details regarding exact path locations for each
platform can be found in
[docs/user_data_dir.md](../user_data_dir.md).

### 4. Enterprise Policies
Chrome for Testing stores enterprise policies in a location different from
standard Chrome enterprise policies location. Details regarding lookup behavior
and specifics are covered in
[docs/enterprise/policies.md](../enterprise/policies.md).

### 5. User Interface & Infobars
*   **InfoBar Management**:
    *   CfT shows a non removable "Chrome for Testing" informational banner near
        the top of browser windows.
    *   Non interactive infobars (those not requiring user response) can be
        suppressed using the `--disable-infobars` command line switch.
*   **User Education & Choice Screens**:
    *   Disables all User Education components and In-Product Help (IPH)
        prompts by default.
    *   Disables the "Search Engine Choice" dialog workflow that mandates
        selecting a default search engine upon first launch.
    *   *Note: These behaviors can be explicitly restored via keys in the JSON
        configuration if desired.*

### 6. Virtual Clipboard
Chrome for Testing supports an optional process-specific virtual clipboard which
operates independently from the system clipboard. This isolation helps to
keep test execution hermetic.

*Note: Virtual clipboard needs to be explicitly enabled in the JSON
configuration.*

### 7. System Integration & Operating System Specifics
*   **Default Browser Disallowance**: Chrome for Testing rejects any attempt to
    register itself as the OS default web browser or scheme client. The
    `GetDefaultWebClientSetPermission()` method always returns
    `SET_DEFAULT_NOT_ALLOWED`.
*   **macOS Automation Optimizations**:
    *   Suppresses the automatic "Install from Disk Image" (DMG) prompt that
        ordinarily occurs when running from a mounted volume.
    *   Completely strips the `CodeSignCloneManager` functionality. This
        mechanism prepares seamless path migration for silent macOS updates,
        which is redundant and overhead for an automated testing executable.

### 8. DevTools & Debugging
*   **Debug Information Persistence**: Explicitly enables local stack traces
    and crash dumps even in Official branding builds, maintaining a better
    debugging experience that usually relies on developer-only builds.
*   **DevTools Integration**: Hardcodes internal states in
    `ChromeContentBrowserClient` to treat the session configuration as actively
    managed by a DevTools client and passes `&isChromeForTesting=true`
    upstream to the inspector frontend interface.

### 9. Direct Automated Behavior Switches
*   **Sign-In Automation**: Implements the
    `--enterprise-signin-dialog-behavior-for-testing` switch. This enables
    testing infrastructure to auto-answer and auto-approve enterprise profile
    creation dialogs (e.g., `"accept-new-profile"`, `"accept-link-data"`, or
    `"cancel"`) without requiring physical UI interaction or Selenium/CDP
    scripts.

### 10. Pre-built Binaries
Pre-built Chrome for Testing binaries are available across all the supported
desktop platforms and Chrome/Chromium channels. You can access them via the
[Chrome for Testing dashboard](https://googlechromelabs.github.io/chrome-for-testing).
