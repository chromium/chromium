# Web App File Handling

Web App File Handling allows installed web apps to register as file handlers
with the operating system for specific MIME types and file extensions (specified
via the `file_handlers` member of the Web App Manifest).

When a user opens a supported file from their file manager, the OS launches the
associated PWA and delivers the file handle(s) to the application's JavaScript
`launchQueue`.

## Platform Implementations

- **Desktop (Windows, Mac, Linux, ChromeOS)**:

  - Managed by
    [`OsIntegrationManager`](/chrome/browser/web_applications/os_integration/os_integration_manager.h)
    and
    [`WebAppFileHandlerManager`](/chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h).
  - Prompts the user with a permission confirmation dialog before opening files
    on launch
    ([`FileHandlerLaunchDialogView`](/chrome/browser/ui/views/web_apps/file_handler_launch_dialog_view.h)).
  - For OS-level registration details, see
    [OS Integration Guide](/chrome/browser/web_applications/docs/os_integration.md#file-handlers).

- **Android (TWAs / WebAPKs)**:

  - Handled via Android intent filters and verified through Digital Asset Links
    (DAL).
  - For routing and launch parameter queuing across the JNI boundary, see
    [TWA Launch Parameters Handling](/components/webapps/docs/android_twa_launch_params.md).

## Web Standards & Explainers

- [W3C File Handling Explainer](https://github.com/WICG/file-handling/blob/main/explainer.md)
- [web.dev: Let installed web applications be file handlers](https://web.dev/file-handling/)
