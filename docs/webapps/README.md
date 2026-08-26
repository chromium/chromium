# Web Apps in Chromium

Web apps are websites with app-like qualities or capabilities. Chromium supports
"installing" a web app (or any website), which enables OS-level integration such
as windowing, file handlers, protocol handlers, shortcut menus, badging, and
background synchronization.

See [Web Apps Concepts](concepts.md) for core definitions (Manifest,
Installability, Promotability, Scope, Display Modes, Identifiers).

## User Entry Points

- **Desktop**: If a site provides a valid web app manifest (name, icon,
  `start_url`, and non-browser `display` mode), an install icon appears in the
  omnibox. Users can also install any site via
  `Chrome Menu > Cast, Save, and Share > Install page as app...`. Installed apps
  are managed on `chrome://apps`.
- **Android**: Installation is triggered via ambient badges, bottom sheets, or
  `3-dot menu > Add to Home screen` / `Install app`, installing either a WebAPK
  or adding a homescreen shortcut.

## Developer Interface

Sites customize OS integration using the W3C
[Web Application Manifest](https://www.w3.org/TR/appmanifest/) specification.
See web.dev guides:

- [Progressive Web Apps Overview](https://web.dev/progressive-web-apps/)
- [Make PWAs Installable](https://web.dev/codelab-make-installable/)

## Subsystem Architecture & Where Code Lives

Because Web Apps span Blink, Content, Shared Components, and platform-specific
embedder layers, the code is structured into several core directories:

```
[Blink Manifest Parser]      third_party/blink/renderer/modules/manifest/
           │ (Mojo IPC)
           ▼
[Content Manifest Host]      content/browser/manifest/
           │
           ▼
[Shared Components]          components/webapps/ (Shared logic)
           │
     ┌─────┴───────────────────────────┐
     ▼                                 ▼
[Desktop PWA Engine]                   [Android WebAPKs & TWAs]
chrome/browser/web_applications/       chrome/android/webapk/
chrome/browser/ui/web_applications/    chrome/android/java/.../webapps/
chrome/browser/ui/views/web_apps/      chrome/browser/android/webapk/
```

### 1. Cross-Platform Shared Components

- **[Shared Web Apps Component](/components/webapps/README.md)**
  (`components/webapps`): Common logic for installability verification
  ([`InstallableManager`](/components/webapps/browser/installable/installable_manager.h)),
  promotion banners
  ([`AppBannerManager`](/components/webapps/browser/banners/app_banner_manager.h)),
  and identifiers ([`identifiers.md`](/components/webapps/docs/identifiers.md)).

### 2. Desktop PWA Subsystem (Windows, Mac, Linux, ChromeOS)

- **[Core Desktop Engine](/chrome/browser/web_applications/README.md)**
  (`chrome/browser/web_applications`): Profile-keyed `WebAppProvider` system,
  commands, fine-grained locks, LevelDB storage, sync integration, and OS
  integration.
- **[Desktop UI Controllers](/chrome/browser/ui/web_applications/README.md)**
  (`chrome/browser/ui/web_applications`): Browser window controllers
  (`WebAppBrowserController`), omnibox actions, navigation capturing, and Mac
  App Shims.
- **[Desktop Views UI](/chrome/browser/ui/views/web_apps/README.md)**
  (`chrome/browser/ui/views/web_apps`): Views dialogs (install bubbles,
  permission prompts, update reviews) and integration testing driver.

### 3. Android Web Apps (WebAPKs & TWAs)

- **[Android Web Apps Architecture](/components/webapps/docs/android_architecture.md)**:
  WebAPKs, Trusted Web Activities (TWAs), auto-minted TWAs, and Android
  lifecycle management.
- **[WebAPK Shell & Runtime](/chrome/android/webapk/README.md)**
  (`chrome/android/webapk`): Minted Android APK wrapper, client libraries, and
  local test setup.
- **[Android Registration & Permissions](/components/webapps/docs/android_registration_and_permissions.md)**:
  OS package registration, notification/location permission delegation, and
  uninstallation tracking.
- **[Android Testing Guide](/components/webapps/docs/android_testing_guide.md)**:
  Robolectric JUnit and on-device instrumentation testing.

### 4. Manifest Parsing & Extraction

- **[Blink Manifest Parser](/third_party/blink/renderer/modules/manifest/README.md)**
  (`third_party/blink/renderer/modules/manifest`): W3C manifest parsing in the
  untrusted renderer process converting JSON into `blink.mojom.Manifest`.
- **[Content Manifest Coordinator](/content/browser/manifest/README.md)**
  (`content/browser/manifest`): Browser-side manifest fetching and icon
  downloading coordination.
