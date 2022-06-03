# Locale uses in Fonts

This document summarizes how locales are used in fonts.

[TOC]

## Parsing the Language Tag

The [lang attribute] spec defines to use [BCP 47] as the language tag.
Since ICU Locale class is not fully compatbile with [BCP 47],
Blink uses its own simple parser
in [platform/text/LocaleToScriptMapping.cpp](../text/LocaleToScriptMapping.cpp).

`LayoutLocale::get()` parses [BCP 47] language tags
and provides methods related with fonts and layout.

`ComputedStyle::getFontDescription().locale()`
computes the [language of a node] as defined in the spec.
Note that this includes not only
the value of the `lang` attribute of the element and its ancestors,
but also the language of the document from the [content-language] header.
Refer to the [language of a node] spec for more details.

The [language of a node] could still be unknown.
`localeOrDefault()` gives you the default language in such case.
The default language is what Chrome uses for its UI,
which is passed to the renderer through `--lang` command line argument.
This could be the same or different from the language of the platform.

Note that `ComputedStyle::locale()` is an `AtomicString`
for the style system to work without special casing,
while `FontDescription::locale()` is a pointer to `LayoutLocale`.

[lang attribute]: https://html.spec.whatwg.org/C/#the-lang-and-xml:lang-attributes
[BCP 47]: https://tools.ietf.org/html/bcp47
[language of a node]: https://html.spec.whatwg.org/C/#language
[content-language]: https://html.spec.whatwg.org/C/#pragma-set-default-language

## Generic Family

Users can configure their preferred fonts for [generic-family]
using the [Advanced Font Settings].
Blink has this settings in `GenericFontFamilySettings`.
In this class, each [generic-family] has a `ScriptFontFamilyMap`,
which is a map to fonts with `UScriptCode` as the key.

To look up the font to use for a [generic-family],
Blink uses the following prioritized list to determine the script.

1. The [language of a node] as defined in HTML, if known.
2. The default language.

This result is available at `ComputedStyle::getFontDescription().localeOrDefault().script()`.

[generic-family]: https://drafts.csswg.org/css-fonts/#generic-family-value
[Advanced Font Settings]: https://chrome.google.com/webstore/detail/advanced-font-settings/caclkomlalccbpcdllchkeecicepbmbm

## Installed Font Fallback

[CSS Fonts] defines a concept of [installed font fallback],
though its behavior is UA dependent.

As Blink tries to match the font fallback behavior
to the one in the platform,
the logic varies by platforms.
While the complete logic varies by platforms,
we try to share parts of the logic where possible.

[CSS Fonts]: https://drafts.csswg.org/css-fonts/
[installed font fallback]: https://drafts.csswg.org/css-fonts/#installed-font-fallback

### Emojis

If we've determined that a character is [emoji-default], also known as "emoji
in emoji" representation, we treat the character a bit differently. The goal is
to not only find a font that supports emojis, but also to prioritize color
emoji fonts over traditional monochrome fonts that happen to have the glyph.

On Android/Skia, Linux, and Windows, Blink will pass the special locale
`und-Zsye` to the operating system when looking for an emoji font. The [Zsye]
script tag is defined by UTS #51 as "prefer emoji style for characters that
have both text and emoji styles available", which is precisely what we need.

On Linux, Blink will additionally always use U+1F46A FAMILY (👪) when matching
potential candidates to increase the odds of finding the right emoji font, in
case the installed emoji font doesn't support the actual emoji in question.

[emoji-default]: https://unicode.org/reports/tr51/#Presentation_Style
[Zsye]: https://unicode.org/reports/tr51/#Emoji_Script

### Unified Han Ideographs

As seen in [CJK Unified Ideographs code charts] in Unicode,
glyphs of Han Ideographs vary by locales.

To render correct glyphs,
the installed font fallback uses the following prioritized list of locales.

1. The [language of a node] as defined in HTML, if known.
2. The list of languages the browser sends in the [Accept-Language] header.
3. The default language.
4. The system locale (Windows only.)

The prioritized list alone may not help the Unified Han Ideographs.
For instance, when the top of the list is "en-US",
it gives no clue to choose the correct font for the Unified Han Ideographs.
For this purpose,
`LayoutLocale::hasScriptForHan()` determines whether
the locale can choose the correct font for the Unified Han Ideographs or not.

When the installed font fallback needs to determine the font
for a Unified Han Ideograph,
it uses `scriptForHan()` of the first locale in the prioritized list
that has `hasScriptForHan()` true.

`scriptForHan()` may be different from `script()`,
in cases such as "en-JP", which indicates an English user in Japan.
Such locale is not major but is not rare either.
Some organizations are known to require to use English versions of the OS,
but their region is not US.

The `script()` of "en-JP" is Latin for the [generic-family] to work correctly,
but its `scriptForHan()` can indicate that
the user prefers Japanese variants of glyphs for the Unified Han Ideographs.

There are multiple cases where such locale can appear in the list:

* A site can use such language tag in HTML or in the [content-language] header
when its UI is in English,
but knows that the user is in Japan,
either by IP address, user preferences of the logged on user,
or any other methods.
* The system (e.g., Windows) can produce such language tag
when its language and region are set differently.

This algorithm is currently used in Windows and Linux.
Android, before N, does not have the language settings and thus
unable to provide the [Accept-Language] list for this algorithm to consume,
but Android N Preview supports multi-locale
and the work to feed the list from OS to the [Accept-Language] list is going on.
Mac relies on Core Graphics to do the job.

The prioritized list is not consistent across platforms today
and this is being addressed.

[CJK Unified Ideographs code charts]: http://unicode.org/charts/PDF/U4E00.pdf
[Accept-Language]: https://tools.ietf.org/html/rfc7231#section-5.3.5
