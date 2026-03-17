## Web Apps

Web apps are websites with app-like qualities or capabilities. Chromium supports 'installing' a web app (or any website), which is sometimes required for some of these capabilities to function (e.g. file handlers or window controls overlay).

See [useful concepts and definitions here](concepts.md).

### User entry points

**Desktop**: If a site has a manifest attached with a name, icon, start_url, and display field specified, an installation icon will appear in the omnibox. Users can also install any site they like via `Menu > More tools > Install Page as App`. Apps are visible on chrome://apps on non-CrOS desktop.
**Android**: An ML model is used to selectively show the blocking installation banner for users who are likely to install the app.  Otherwise installation is accessible via `3-dot menu > Add to Homescreen`.

### Developer interface

Sites customize how their installed site integrates at the OS level using a [web app manifest](https://www.w3.org/TR/appmanifest/). See developer guides for in depth overviews:

- https://web.dev/progressive-web-apps/
- https://web.dev/codelab-make-installable/

## Where is the code?

Because Progressive Web Apps spans across Android and Desktop, the code is split across multiple directories depending on its implementation:

* **[Desktop `WebAppProvider`](/chrome/browser/web_applications/README.md)**: Desktop-specific system (Windows, Mac, Linux, ChromeOS) for installing and managing web apps.
* **[Shared `components/webapps`](/components/webapps/README.md)**: Code shared between Android and Desktop, like the `InstallableManager` and `AppBannerManager`.
