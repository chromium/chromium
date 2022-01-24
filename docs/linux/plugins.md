# Linux Plugins

## Background reading materials

### Plugins in general

*   [Gecko Plugin API reference](https://developer.mozilla.org/en-US/docs/Plugins/Guide)
    -- most important to read
*   [Mozilla plugins site](http://www.mozilla.org/projects/plugins/)
*   [XEmbed extension](https://developer.mozilla.org/en/XEmbed_Extension_for_Mozilla_Plugins)
    -- newer X11-specific plugin API
*   [NPAPI plugin guide](http://gplflash.sourceforge.net/gplflash2_blog/npapi.html)
    from GPLFlash project

### Chromium-specific

*   [Chromium's plugin architecture](https://dev.chromium.org/developers/design-documents/plugin-architecture)
    -- may be out of date but will be worth reading

## Code to reference

*   [Mozilla plugin code](http://mxr.mozilla.org/firefox/source/modules/plugin/base/src/)
    -- useful reference
*   [nspluginwrapper](http://gwenole.beauchesne.info//en/projects/nspluginwrapper)
    -- does out-of-process plugins itself

## Terminology

*   _Internal plugin_: "a plugin that's implemented in the chrome dll, i.e.
    there's no external dll that services that mime type. For Linux you'll just
    have to worry about the default plugin, which is what shows a puzzle icon
    for content that you don't have a plugin for.  We use that to allow the user
    to download and install the missing plugin."

## Flash

*   [Adobe Flash player dev center](http://www.adobe.com/devnet/flashplayer/)
*   [penguin.swf](http://blogs.adobe.com/penguin.swf/) -- blog about Flash on
    Linux
*   [tips and tricks](http://macromedia.mplug.org/) -- user-created page, with
    some documentation of special flags in `/etc/adobe/mms.cfg`
*   [official Adobe bug tracker](https://bugs.adobe.com/flashplayer/)

## Useful Tools

*   `xwininfo -tree` -- lets you inspect the window hierarchy of a window and
    get the layout of child windows.
*   "[DiamondX](http://multimedia.cx/diamondx/) is a simple NPAPI plugin built
    to run on Unix platforms and exercise the XEmbed browser extension."
    *   To build a 32-bit binary:
        `./configure CFLAGS='-m32' LDFLAGS='-L/usr/lib32 -m32'`
