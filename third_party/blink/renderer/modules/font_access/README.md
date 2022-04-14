# Font Access API

This directory contains the renderer-side implementation of
[Font Access API](https://github.com/WICG/local-font-access/blob/main/README.md).

## Related directories

[`//content/browser/font_access/`](../../../../../content/browser/font_access/)
contains the browser-side implementation, and 
[`blink/public/mojom/font_access`](../../../public/mojom/font_access) contains
the mojom interface for this API.

## APIs In this directory

It consists of the following parts:

 * `FontAccess`: A supplement to window that exposes an operation to access
    local fonts. `FontAccess` maintains a mojo connection with browser-side,
    and is responsible for handling errors, and optionally applying query
    filters provided by `QueryOptions`.

 * `FontData`: Implemented as `FontMetadata.cc`, `FontData` represents data
    about a single font installed on the system. Currently, it provides basic
    information such as postscript name, full name, family and style;
    addiitonally, it exposes a method `blob()` for accessing additional font
    blob data via Blink's `FontCache`.
    (See more details [here](../../../blink/renderer/platform/fonts))
