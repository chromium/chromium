# RenderText Overview

RenderText’s purpose is to prepare text for text shaping, shape the text using
the [Harfbuzz text shaping
engine](https://harfbuzz.github.io/what-is-harfbuzz.html), and then draw the
text onto the screen using the Skia library. RenderText also has support for
cursors and selections. This class is the level of abstraction for the complex
world of unicode. Chrome developers should rely on it or improve it instead of
developing their own solutions.

RenderTextHarfbuzz is the implementation of RenderText. We used to have platform
specific implementations but have since consolidated this into
RenderTextHarfbuzz.

A RenderText object can be created in a View that wants to display text. The
View can set properties on the RenderText object such as the text to be
displayed, whether or not the text is obscured (such as the case with
passwords), word wrap behavior etc.

`RenderText::Draw` starts the process of drawing the text onto the screen.

We then do the following before passing the results of these steps off to Skia.
1. Apply Additional Layout Attributes to the Text.
2. Itemization (also known as segmentation)
3. Text Shaping
4. Eliding
5. Multiline Text Layout


## Apply Additional Layout Attributes to the Text:
* Before we itemize and shape the text, we may need to apply some additional
  layout attributes.
* This includes:
    * Obscuring the text for passwords
    * Applying styles
    * Handling control characters
    * Applying underlines
* Note that currently some parts of the code will still try to rewrite
  codepoints which is incorrect because text can only be broken up by graphemes.

## Itemization (also known as segmentation):

* This involves breaking up the text into separate runs (TextRunHarfBuzz
  objects). Runs represent sections of text that we can shape with a single
  font.
* We use the ICU library’s bidirectional iterator to break up runs and then
  further break up the runs by script, special characters, and style boundaries.

## Text Shaping:

* We choose the appropriate font to shape the runs with.
* Then go through the list of runs and try to shape each of the runs by creating
  Harfbuzz structures from the input run.
* We call into the Harfbuzz API to shape the text (hb_shape) and populate the
  output with the glyph data.
* If any runs still have missing glyphs, we add them back to the list of runs
  that still need to be shaped.
* For these remaining runs, we have fallback code to attempt to shape them
  again, but it is possible that some fail to be shaped.
* Since fallback occurs on a per-run basis, we then re-itemize the runs,
  accounting for missing glyphs, and restart the shaping stage. This ensures
  that the number of missing glyphs are minimized, as each glyph will then
  attempt to locate an appropriate font.

## Eliding:
* If the text exceeds its constrained dimensions we will need to elide the text.
* We elide based on the elide behavior set on the RenderText object and whether
  the text is multiline.

## Multiline Text Layout:
* If the text is not multiline we create a single line. If the text is multiline
  we break up the text into lines. (see
  [HarfBuzzLineBreaker](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/render_text_harfbuzz.cc;l=409;drc=adb945c5a0060e6024cb174f6027d13d7ff03058;bpv=1;bpt=1))
* Set the shaped text based on these lines.
* Skia will use the lines of the shaped text to draw the text onto the screen.
