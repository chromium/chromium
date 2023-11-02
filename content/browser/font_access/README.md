# Font Access API

This directory contains the browser-side implementation of
[Font Access API](https://github.com/WICG/local-font-access/blob/main/README.md).

## Related directories

[`//third_party/blink/renderer/modules/font_access/`](../../../third_party/blink/renderer/modules/font_access/)
contains the renderer-side implementation, and
[`//third_party/blink/public/mojom/font_access`](../../../third_party/blink/public/mojom/font_access)
contains the mojom interface for this API.

## Code map

It consists of the following parts:

 * `FontAccessManager`: `content::FontAccessManager` implements
   `blink::mojom::FontAccessManager`, providing a way to enumerate local fonts.
   It checks for the requirements to access local fonts such as user permission,
   page visibility and transient user activation. Once all the requirements are
   met, it returns the fonts data from `FontEnumerationCache`. In terms of code
   ownership, there is one `FontAccessManager` per `StoragePartitionImpl`;
   `Frame`s are bound to `FontAccessManager` via a `BindingContext`.

 * `FontEnumerationCache`: `content::FontEnumerationCache` is a cache-like
   object that memoizes the data about locally installed fonts as well as the
   operation status, given the user locale. The data is read via
   `content::FontEnumerationDataSource.GetFonts()` and is stored and served in
   `ReadOnlySharedMemoryRegion`. This class is not thread-safe. Each instance
   must be accessed from a single sequence, which must allow blocking.

 * `FontEnumerationDataSource`: `content::FontEnumerationDataSource`, inherited
   by OS-specific subclasses, reads locally installed fonts from the underlying
   OS. While the subclasses of this class contains the implementation for
   reading font data, this class itself does not, as it is instantiated for
   non-supported OS. It is not thread-safe; all methods except for the
   constructor must be used on the same sequence, and the sequence must allow
   blocking I/O operations.
