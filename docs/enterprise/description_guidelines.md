## Line return
When writing multiline descriptions, single line returns may be used to keep
the line lengths reasonably short (~80 characters). Those single line returns
will be ignored in the generated [documentation](https://chromeenterprise.google/intl/en_ca/policies).

If you want an explicit line return in the generated [documentation](https://chromeenterprise.google/intl/en_ca/policies),
use double line returns.

## Product names
To ensure consistency in policy descriptions, the following is a mapping of
how various product names and the like should be referenced. All placeholders
tags must be opened and closed on the same line to avoid validation errors.

* Chrome: `<ph name="PRODUCT_NAME">$1<ex>Google Chrome</ex></ph>`
* ChromeOS: `<ph name="PRODUCT_OS_NAME">$2<ex>Google ChromeOS</ex></ph>`
* ChromeOS Flex: `<ph name="PRODUCT_OS_FLEX_NAME">Google ChromeOS Flex</ph>`
* Chrome Browser Cloud Management: `<ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>`
* Chrome Cleanup: `<ph name="CHROME_CLEANUP_NAME">Chrome Cleanup</ph>`
* Chrome Sync: `<ph name="CHROME_SYNC_NAME">Chrome Sync</ph>`
* Chrome Remote Desktop: `<ph name="CHROME_REMOTE_DESKTOP_PRODUCT_NAME">Chrome Remote Desktop</ph>`
* Linux: `<ph name="LINUX_OS_NAME">Linux</ph>`
* Internet Explorer: `<ph name="IE_PRODUCT_NAME">Internet® Explorer®</ph>`
* Google Admin console: `<ph name="GOOGLE_ADMIN_CONSOLE_PRODUCT_NAME">Google Admin console</ph>`
* Google Calendar: `<ph name="GOOGLE_CALENDAR_NAME">Google Calendar</ph>`
* Google Cast: `<ph name="PRODUCT_NAME">Google Cast</ph>`
* Google Cloud Print: `<ph name="CLOUD_PRINT_NAME">Google Cloud Print</ph>`
* Google Drive: `<ph name="GOOGLE_DRIVE_NAME">Google Drive</ph>`
* Google Photos: `<ph name="GOOGLE_PHOTOS_PRODUCT_NAME">Google Photos</ph>`
* Google Update: `<ph name="GOOGLE_UPDATE_NAME">Google Update</ph>`
* Google Workspace: `<ph name="GOOGLE_WORKSPACE_PRODUCT_NAME">Google Workspace</ph>`
* Lacros: `<ph name="LACROS_NAME">Lacros</ph>`
* Android: `<ph name="ANDROID_NAME">Android</ph>`
* macOS: `<ph name="MAC_OS_NAME">macOS</ph>`
* iOS: `<ph name="IOS_NAME">iOS</ph>`
* Windows: `<ph name="MS_WIN_NAME">Microsoft® Windows®</ph>`
* Microsoft Active Directory: `<ph name="MS_AD_NAME">Microsoft® Active Directory®</ph>`
* Microsoft Azure Active Directory `<ph name="MS_AAD_NAME">Microsoft® Azure® Active Directory®</ph>`
* Fuchsia: `<ph name="FUCHSIA_OS_NAME">Fuchsia</ph>`

## Sensitive policies
Malwares could abuse policies to control users' device. Some policies are
particularly dangerous so there will be additional management requirements
before applying them. For those policies, please append the following statements
in the end of policy description.
```
On <ph name="MS_WIN_NAME">Microsoft® Windows®</ph>, this policy is only available on instances that are joined to a <ph name="MS_AD_NAME">Microsoft® Active Directory®</ph> domain, joined to <ph name="MS_AAD_NAME">Microsoft® Azure® Active Directory®</ph>` or enrolled in `<ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>`.

On <ph name="MAC_OS_NAME">macOS</ph>, this policy is only available on instances that are managed via MDM, joined to a domain via MCX or enrolled in `<ph name="CHROME_BROWSER_CLOUD_MANAGEMENT_NAME">Chrome Browser Cloud Management</ph>`.
```
