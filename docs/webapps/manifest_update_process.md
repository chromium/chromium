# Manifest Update Process

High level information: https://web.dev/manifest-updates/

The manifest update process is required whenever a new manifest is served to an existing web_app, which would mean that the database entries corresponding to that web application needs to be updated. To do this, the following steps are kicked off:

- Whenever the tab has kicked off a [navigation to an url for a web_app][1], a check is kicked off to determine if a manifest update is required or not. The update process is aborted if the following conditions are satisfied:

    - If the app is not installed.
    - If the installed app is a [System Web App][2]
    - If the installed app is a [placeholder app][3]
    - If there was an existing update already running for a specific app id.
    - If there has already been a manifest update within the past 24 hours. This is done to ensure similar behavior as on [Android][4].
    This also helps prevent downloading megabytes of images every time a page with a valid manifest link is visited.
- All pending updates are tracked in a [map of update states keyed by app id][5] by the [`ManifestUpdateManager`][6]. If a successful update needs to happen, the process goes ahead by executing the following steps:

    - Wait for the [page to finish loading][7].
    - On successful page load, the [`ManifestUpdateCheckCommand`][8] takes over to perform the following tasks:
      - Fetching manifest data from the site.
      - Loading saved manifest data from disk.
      - Computing the differences between the site and on disk.
      - Resolving changes to identity sensitive fields (app icon and name) by either allowing, requesting user confirmation or reverting changes.
      - For any errors in the above steps or if there are no changes, the whole operation is aborted and the command gracefully exits.
    - Once all the data has been fetched, wait for all existing app windows for that app to close.
    - On all windows being closed, the [`ManifestUpdateFinalizeCommand`][9] runs to write the new data to the DB and verify that a successful write has completed.

# Testing

- [`ManifestUpdateManagerBrowserTest`][10] contains browser tests for the entire end-to-end working of the ManifestUpdateManager.
- [`ManifestUpdateCheckCommandTest`][11] contains unit tests for the comparison part of the manifest update operation.
- [`ManifestUpdateFinalizeCommandTest`][12] contains unit tests for the data writing section of the manifest update operation.


[1]: https://source.chromium.org/search?q=WebAppTabHelper::PrimaryPageChanged
[2]: https://source.chromium.org/search?q=SystemWebAppManager
[3]: https://source.chromium.org/search?q=Placeholder%20f:webapps%2Fconcepts.md
[4]: https://source.chromium.org/search?q=WebAppDataStorage.java%20UPDATE_INTERVAL
[5]: https://source.chromium.org/search?q=manifest_update_manager.h%20update_stages_
[6]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/web_applications/manifest_update_manager.h
[7]: https://source.chromium.org/search?q=ManifestUpdateManager%20PreUpdateWebContentsObserver&sq=
[8]: https://source.chromium.org/search?q=ManifestUpdateCheckCommand
[9]: https://source.chromium.org/search?q=ManifestUpdateFinalizeCommand
[10]: https://source.chromium.org/search?q=ManifestUpdateManagerBrowserTest
[11]: https://source.chromium.org/search?q=ManifestUpdateCheckCommandTest&sq=
[12]: https://source.chromium.org/search?q=ManifestUpdateFinalizeCommandTest
