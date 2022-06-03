# Font Access API

This directory contains the implementation of the Font Access API.

## Related directories

[`//third_party/blink/renderer/platform/fonts`](../../modules/font_access)
contains the platform implementation.

## APIs In this directory

This directory contains the implementation of the new and still under
development [Font Access API](https://github.com/inexorabletash/font-enumeration/blob/master/README.md).

It consists of the following parts:

 * `FontMetadata`: Metadata about a font installed on the system. Provides
   information that uniquely identifies a font, as well as text to present to
   users in their system-configured locale.

 * `FontIterator`, `FontIteratorEntry`: Mechanism providing the ability to
    enumerate system-installed fonts using an asynchronous iterator.

 * `FontManager`: An object that exposes operations to access fonts. In the
   future, this may be extended to add more capabilities.

 * `NavigatorFonts`: An entry point, available in documents and workers on
   `navigator`, that lets a website enumerate locally installed system fonts.
