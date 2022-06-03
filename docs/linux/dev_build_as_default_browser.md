# Linux Dev Build As Default Browser

Copy a stable version's `.desktop` file and modify it to point to your dev
build:

```
cp /usr/share/applications/google-chrome.desktop \
   ~/.local/share/applications/chromium-mybuild-c-release.desktop
vim ~/.local/share/applications/chromium-mybuild-c-release.desktop

# Change first Exec line in desktop entry: (change path to your dev setup)
Exec=/usr/local/google/home/scheib/c/src/out/Release/chrome %U
```

Set the default:

    xdg-settings set default-web-browser chromium-mybuild-c-release.desktop

Launch, telling Chrome which config you're using:

*   `CHROME_DESKTOP=chromium-mybuild-c-release.desktop out/Release/chrome`
*   Verify Chrome thinks it is default in `about:settings` page.
    *   Press the button to make default if not.

Restore the normal default:

    xdg-settings set default-web-browser google-chrome.desktop

Change the default, run, and restore:

    xdg-settings set default-web-browser chromium-mybuild-c-release.desktop && \
        CHROME_DESKTOP=chromium-mybuild-c-release.desktop out/Release/chrome
    xdg-settings set default-web-browser google-chrome.desktop && \
        echo Restored default browser.
