# Unicode Overview

This document goes over general concepts of Unicode and text rendering.

## **Breakdown**
This is a general overview of how text gets transformed from raw bytes to a
glyph on the screen. The document is catered towards explaining niche concepts
while also providing some pitfalls to avoid.

Chrome deals with Unicode hence this assumes text will be rendered as unicode
characters. Chrome uses the ICU library for Unicode:
[ICU](https://icu.unicode.org/) which is an open source set of libraries that
provide Unicode support for many different applications.

This doc will be going over 4 stages.
1. Unicode codepoint encodings
2. Codepoints
3. Graphemes
4. Glyphs

## **Unicode codepoint encodings**

Binary and Codepoint encoding are CS concepts with a lot of online resources.
You can search for any online resource to get more familiar with this concept.
- [Unicode Technical Reports](https://www.unicode.org/reports/)

Codepoint encoding is a way for software to convert codepoints to assigned bytes
while codepoint decoding will transform bytes back to characters. The most
commonly used encodings are UTF-8 and UTF-16.

Imagine this as a mapping between binary and codepoints.
- User inputs a string while specifying the codepoint encoding. (Note: Generally
this fallsback to the default encoding scheme)
- Program will transform the string into binary and store it.
- When the program needs to use the string again, it fetches the binary and
decodes it back to the string with the encoding schema.

The main difference between encoding schemas is how much memory is reserved for
each codepoint.
- `UTF-8` - This is a variable length 8 bit encoding scheme that saves the most
memory for the first 127 values (in a single byte) but can also grow in size
all the way up to 6 bytes depending on the codepoint.
- `UTF-16` - This is a variable length (1 or 2 bytes) 16 bit encoding scheme
that can support most codepoints while using less memory than UTF-32. Generally
used if you only need to support most languages/symbols.
- `UTF-32` - This is a fixed width for 32 bits that can support all codepoints.
The tradeoff however is that it will use the most memory per codepoint.

For example:
In UTF-8 the letter `A` is represented as the following:
```
Binary 01000001 => Codepoint (U+0041).
UTF-8:
41
std::string str = "\x41";
UTF-16:
0041
std::u16string str = u"\x41"
UTF-32:
000000041
std::u32string str = U"\U00000041"
```
For characters that might require multiple 4 bytes like the treble clef `𝄞`, the
encoding will change depending on the schema.
```
Binary
11110000 10011101 10000100 10011110
UTF-8:
F0 9D 84 9E
std::string str = "\xF0\x9D\x84\x9E"
UTF-16:
D834 DD1E
std::u16string str = u"\xD834\xDD1E"
UTF-32:
0001D11E
std::u32string str = U"\U0001D11E"
```

## **Codepoints**

A codepoint is a unique number that represents some type of character +
information. Codepoints also have
[properties/attributes](https://unicode-org.github.io/icu/userguide/strings/properties.html)
that describe how to perform rendering on them. (i.e. BiDi, Block, Script, etc)

For example, `U+0041` is a codepoint that represents the letter `A`. Unicode has
a large library of codepoints that handles characters from different languages,
symbols used in pronunciation, and even emojis.

Some codepoints can affect their surronding characters.
For example, diacritical codepoints such as the "combining acute accent"
`(U+0301)` is used to append an accent on a character.
```
◌́
```
Some codepoints do not map to a displayed character.
ZWJ (`U+200D`) is a zero width joiner and is a codepoint that joins two
codepoints together. Left-To-Right Embedding (`U+202A`) is a codepoint that
forces text to be interpreted as left-to-right.

Variation selectors are another set of codepoints that only affects their
surrounding character. These codepoints will affect the presentation of the
preceding character. For example an emoji + U+FE0E will set the emoji to a text
display while emoji + U+FE0F will set the emoji to the colored display. If you
do not specify a variation, the shaping engine will just pick the default glyph
in the font.

```
U+2708 maps to an airplane: ✈️

Adding a variation selector (U+FE0E or U+FE0F) will affect the way the emoji is
displayed

U+2708 U+FE0E = ✈︎
U+2708 U+FE0F = ✈️

```

## **Graphemes**

A grapheme is a sequence of one or multiple codepoints. For example, “e” and “é”
are both graphemes. “e” is a single codepoint while “é” can be either a single
codepoint or multiple codepoints depending on how it’s encoded.

```
É (U-00E9)
```
or
```
“e” + “´”
(U-0065) + (U-00B4)
```
Emojis are another example of grapheme clusters that can be combinations of
multiple codepoints.

```
👨‍✈️ is actually a combination of
👨 Man (U+1F468) +
Zero Width Joiner (U+200D) +
✈️ Airplane (U+2708)
```
**Note: Graphemes are not breakable!**
Because of codepoints such as diatric or joiners that can append multiple
codepoints together to a grapheme, many codepoints can make up a single
grapheme.

For example a grapheme can consist of :
- 1x codepoint: codepoint
- 2x codepoint: base + diatric
- 3x codepoint: joiner codepoint [presentation]
- Nx codepoint: codepoint + joiner + codepoint + joiner + codepoint + etc

Luckily we do not need to implement all of the various combinations or
understand what makes a valid grapheme. Chrome relies on the ICU library to
iterate through graphemes.

## **Glyphs**

A glyph is set of graphic primitives that are painted to the screen that
represents the grapheme. Fonts are pre-made mappings between characters and
glyphs. The pixels printed on the screen will be based on the Font loaded that
maps Graphemes to Glyphs. Fonts can be user controlled so any type of image can
be displayed depending on what is mapped to the grapheme.

Note that there isn't a guarantee mapping of 1:1 between grapheme and glyph, it
is more of an N:M.

Translating Grapheme to Glyphs is a multi-step process that will be covered
here: (TODO - add link to RenderText doc).

## **Pitfalls:**
### ***Rule: Do not break up graphemes!***
Modifying an encoded string is hard and has more risk then you think.

If you are trying to truncate a string to a length of 3, you might try something
like:
```
string new_string = original_string.substr(0,3);
```
But that is incorrect!

Lets imagine `original_string` is actually an emoji and you attempt to truncate
the string to a width of 3.
```
std::string original_string = "😔"; // Encoded as "\xF0\x9F\x98\x94" length = 4
string new_string = original_string.substr(0,3);
```
Well what's wrong here? You might think this is fine since there is only 1
displayed character in the emoji string, but that is incorrect!
`"😔" maps to "F0 9F 98 94"`

The length of the string is actually 4. By taking a substring of emoji with
width of 3, it will corrupt the data by removing the last codepoint of the
string.
`F0 9F 98` ~~`94`~~

This becomes a corrupted string which does not map to a valid unicode character.

Developers might also attempt to highlight only a section of codepoints of the
string.

`SetColor(Yellow, Range(1...3));`

But this is also incorrect depending on which codepoints are highlighted.
Similar to the previous example, if the range is only part of one codepoint
within the grapheme, this will break!

It is impossible to know what color to use for that glyph because its range
does not encompass the entire grapheme.

## **Recommendations:**

### Avoid using custom text modifications

The crux of this document is to highlight the difficulties of Unicode. Before
trying to add custom string modifiers, look at the `gfx::` namespace to see if
the functionality already exists. If it is not covered, please reach out to the
owners and consider adding the new functionality to `gfx::`.
