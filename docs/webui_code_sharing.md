<style>
.doc h1 {
  margin: 0;
}

.doc h3,
.doc h4 {
  font-weight: bold;
}

.doc h4 {
  font-style: italic;
}
</style>

# **Sharing Code in WebUI**

## **Summary Diagram**

The following diagram summarizes the correct location for TypeScript/CSS/HTML
WebUI code that is used by 2 or more WebUIs. Details on how to determine which
category a specific piece of code belongs to follow.

![WebUI code sharing diagram](images/webui_code_sharing.png)

## **Step 1: Determine whether the code should be widely or narrowly shared.**

**Widely shared code** should be located in `ui/webui/resources/` and will be
served from `chrome://resources` and `chrome-untrusted://resources` at runtime.
Code in these folders can be used by all UIs in Chrome (trusted and untrusted).
As a result, code in these folders **should be general purpose, and make sense
for any Chrome UI to use, not just UIs with specific properties**.

**Examples of general purpose code:**

*   Core UI elements like `cr_checkbox.ts` (lots of UIs have use for checkboxes)
*   Widely useful utilities like `assert.ts` (lots of UIs need to
    `assert(<some condition>) `or `assertNotReached()`)

***Rule of thumb: If code is needed by 3+ different WebUI surfaces, this
is often a good indicator that it is sufficiently general purpose to be widely
shared.***

**Narrowly shared code** should live in a specific folder that is a sibling of
folders that need to use it. For example, code in
`chrome/browser/resources/settings_shared` is used by `c/b/resources/settings`
and `c/b/resources/password_manager`. Narrowly shared libraries are packaged
with a `build_webui()` rule. UIs that need these libraries add a dependency on
the generated `ts_library()` target, and add the files from the generated `.grd`
to their data source.  Narrowly shared code is served from a designated path
from the individual UIs that use it (e.g. `chrome://settings` and
`chrome://password-manager` both serve code from `settings_shared` from
`/shared/settings/`) and it is only available to these UIs, and not to all
WebUIs in Chrome.

**Examples of code that multiple UIs use, but is not general purpose and
therefore is narrowly shared:**

*   UI code that is only useful for UIs that are in the Side Panel (belongs in
    `chrome/browser/resources/side_panel/shared`)
*   UI code that is only useful for UIs that have access to the
    `settingsPrivate` API and use the settings “prefs” mechanism (belongs in
    `chrome/browser/resources/settings_shared`)
*   UI code for viewing a PDF document (belongs in
    `chrome/browser/resources/pdf`)

## **Step 2, for widely shared code: Determine which subfolder to use**

The organization of `ui/webui/resources` subfolders is as follows:

**`js`**:
Used for general purpose utilities and some browser proxies.
Not for UI elements; should not depend on Polymer or Lit.

**`cr_elements`**:
Used for UI elements, styles, and mixins that meet the following requirements:
* Do not use `$i18n` replacements or the `I18nMixin`.
* Do not use `chrome.send`, Mojo, or extension APIs
For more details see the [cr_elements README](https://chromium.googlesource.com/chromium/src/+/main/ui/webui/resources/cr_elements/README.md)

**`cr_components`**:
Used for more complex UI elements or components that are widely shared, but
don’t fit the requirements for cr_elements. For more details see the
[cr_components README](https://chromium.googlesource.com/chromium/src/+/main/ui/webui/resources/cr_components/README.md)

## **Step 3, for widely shared code: Add unit testing**

**All widely shared code in `ui/webui/resources` should have unit tests at the
time it is added to this folder**. Since the code is widely shared, it is likely
many developers from different teams will need to make changes, and unit tests
reduce the chance of such changes introducing regressions. Regressions in shared
code are also more likely to be high impact, since they can impact many
different UIs.

