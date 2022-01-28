# Chromium's .icns files

## Sizes

`.icns` files contain multiple sizes of icons. The standard `.icns` files for
Chromium contain icons of the following sizes:

| Size    | Type              |
|---------|-------------------|
| 16×16   | `'is32'`/`'s8mk'` |
| 32×32   | `'il32'`/`'l8mk'` |
| 128×128 | `'it32'`/`'t8mk'` |
| 256×256 | `'ic08'` (PNG)    |
| 512×512 | `'ic09'` (PNG)    |

## Rationale

The rationale behind these choices is to avoid bugs in icon display. As noted in
a [bug](https://crbug.com/576173#c4), having @2x versions of the smaller icons
causes blockiness on retina Macs, and in fact, sometimes just having @2x
versions of icons would cause them to be preferred even when it doesn't make
sense.

At least through macOS 10.11, using the modern (`'icp4'`–`'icp6'`) types causes
scrambling of the icons in display. If the old types work, why mess with them?

## Source PNG files

Use whatever tools you want to create the PNG files, but please note that the
dimensions of the images in the PNG files must match exactly the size indicated
by their filename. This will be enforced by the `makeicns2` tool below.

## Construction

The tools for `.icns` construction can be found in `src/tools/mac/icons`.
Compile `makeicns2` before you begin by following the directions in its header
comment.

In addition, you will need `optipng` and `advpng`, which can be found in the
`optipng` and `advancecomp` packages, respectively, in your favorite port
manager.

To construct an `.icns` file:

1. Assemble a directory (`.iconset`) containing the five required sizes of icon,
in PNG format: 16×16, 32×32, 128×128, 256×256, and 512×512, named `16.png`,
`32.png`, `128.png`, `256.png`, and `512.png`, respectively.
1. Process the `.png` files with:
    1. `optipng -o7 -zm1-9`
    1. `advpng -z4 -i50`
    1. `png_fix.py`
1. Create the `.icns` file with the `makeicns2` you compiled:
`makeicns2 <name>.iconset`

## Deconstruction and other tools

In the `src/tools/mac/icons/additional_tools` directory there are several other
tools that you may find useful in your quest to craft the perfect icons. They
are:

- `makeicns`: This tool takes an `.iconset` directory of `.png` files and
creates an `.icns` file containing them. However, it cannot create the old-style
data/mask image pairs, which is why `makeicns2` is preferred.
- `unmakeicns`: This tool takes an `.icns` file and unpacks it into its
constituent images. For the old-style data/mask image pairs, this tool will
reconstitute the two into a more useful `.png` file.
- `unpackicon`: This tool is used by the `unmakeicns` tool. It takes the data
from an old-style data/mask image pair, which is encoded with a simple
PackBits-esque RLE scheme, and outputs the data uncompressed. It is available
separately should this be a capability you require.
- `makepng`: This tool is used by the `unmakeicns` tool. It reconstructs a
`.png` file from the uncompressed data of an old-style data/mask image pair. It
is available separately should this be a capability you require.
- `maketoc.py`: This tool can be used to both create a `TOC` section for an
`.icns` file, as well as verify one. The `makeicns2` tool automatically creates
a `TOC` section, but this tool's verification of these sections may be useful.

To compile these tools, run `make` in their containing directory. Note that
`libpng` is required.
