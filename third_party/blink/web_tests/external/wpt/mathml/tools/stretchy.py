#!/usr/bin/python

from utils import mathfont
import fontforge

# Create a WOFF font with glyphs for all the operator strings.
font = mathfont.create("stretchy", "Copyright (c) 2021 Igalia S.L.")

# Set parameters for stretchy tests.
font.math.MinConnectorOverlap = mathfont.em / 2

# These two characters will be stretchable in both directions.
horizontalArrow = 0x295A # LEFTWARDS HARPOON WITH BARB UP FROM BAR
verticalArrow = 0x295C # UPWARDS HARPOON WITH BARB RIGHT FROM BAR

mathfont.createSizeVariants(font)

# Add stretchy vertical and horizontal constructions for the horizontal arrow.
mathfont.createSquareGlyph(font, horizontalArrow)
mathfont.createStretchy(font, horizontalArrow, True)
mathfont.createStretchy(font, horizontalArrow, False)

# Add stretchy vertical and horizontal constructions for the vertical arrow.
mathfont.createSquareGlyph(font, verticalArrow)
mathfont.createStretchy(font, verticalArrow, True)
mathfont.createStretchy(font, verticalArrow, False)

mathfont.save(font)
