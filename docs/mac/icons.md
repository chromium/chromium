# Chromium’s icons

## Asset catalogs for the app icon and document icon badge

Mac Chromium stores its app icon and document icon badge in an asset catalog.
Unlike iOS Chromium, which compiles an `.xcassets` directory into a `.car` file
at build time, Mac Chromium has the `.car` file pre-built and checked in. This
is done because (unlike on iOS) the asset catalog file only holds two icons and
thus isn’t often changing, and because of internal technical constraints in
tooling. This may change in the future.

Chromium’s asset catalog has two multi-size image sets: one named `AppIcon` used
for the app icon, and one named `Icon` used for badging documents using the
`UTTypeIcons`/`UTTypeIconBadgeName`/`UTTypeIconText` `Info.plist` keys. The
asset catalog `.xcassets` source, as well as the compiled `.car` result, are
checked into the `//chrome/app/theme` directory.

### Compiling the asset catalog

To compile the asset catalog, invoke `//tools/mac/icons/compile_car.py`:

`python3 tools/mac/icons/compile_car.py chrome/app/theme/chromium/mac/Assets.xcassets`

(or the path to whichever `.xcassets` file you want to compile)

The script will put the files resulting from the asset catalog compilation into
the directory containing the `.xcassets` file that was processed.

## `.icns` files

While the app icon and document icon badge are stored in an asset catalog,
`.icns` files are still required in certain circumstances (e.g. `.dmg` icons).
When such icons are still required, this is the Chromium convention for their
construction.

### Sizes

`.icns` files contain multiple sizes of icons. Standard `.icns` files for
Chromium contain icons of the following sizes:

| Size    | Type              |
|---------|-------------------|
| 16×16   | `'is32'`/`'s8mk'` |
| 32×32   | `'il32'`/`'l8mk'` |
| 128×128 | `'it32'`/`'t8mk'` |
| 256×256 | `'ic08'` (PNG)    |
| 512×512 | `'ic09'` (PNG)    |

### Rationale

The rationale behind these choices is to avoid bugs in icon display. As noted in
a [bug](https://crbug.com/40451709#comment5), having @2x versions of the smaller
icons causes blockiness on retina Macs, and in fact, sometimes just having @2x
versions of icons would cause them to be preferred even when it doesn't make
sense.

At least through macOS 10.11, using the modern (`'icp4'`–`'icp6'`) types causes
scrambling of the icons in display. If the old types work, why mess with them?

### Source PNG files

Use whatever tools you want to create the PNG files, but please note that the
dimensions of the images in the PNG files must match exactly the size indicated
by their filename. This will be enforced by the `makeicns2` tool below.

### Construction

The tools for `.icns` construction can be found in `//tools/mac/icons`.
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

## Historical tools

Tools historically used for `.icns` file construction and analysis can be found
in `//tools/mac/icons/additional_tools`.
