# Chromium’s icons

## Asset catalogs for the app icon and document icon badge

Mac Chromium stores its app icon and document icon badge in an asset catalog.
Unlike iOS Chromium, which compiles an `.xcassets` directory into a `.car` file
at build time, Mac Chromium has the `.car` file pre-built and checked in. This
is done because (unlike on iOS) the asset catalog file only holds three items
and thus isn’t often changing, and because of internal technical constraints in
tooling. This may change in the future.

Chromium’s asset catalog contains three logical items:

1. A collection of assets (`Icon Image`, `MultiSized Image`, `PackedImage`)
   named `AppIcon` used for the app icon on macOS releases prior to macOS 26.
2. A collection of assets (`Color`, `Image`, `IconGroup`, `IconImageStack`,
   `Named Gradient`, `Vector`) named `AppIcon` used for the app icon on macOS 26
   and subsequent releases.
3. A collection of assets (`Icon Image`, `MultiSized Image`) named `Icon`
   used for badging documents using the
   `UTTypeIcons`/`UTTypeIconBadgeName`/`UTTypeIconText` `Info.plist` keys.

The sources for this catalog are an `.xcassets` directory as well as an `.icon`
Icon Composer document package. These sources, as well as the compiled `.car`
result, are checked into the `//chrome/app/theme` directory.

The current state of app icons (as of July 2025) is that Chrome compiles a
“split app icon” asset catalog: the `AppIcon` bitmaps used for macOS releases
prior to macOS 26 are those of the old, 2022-era app icon, while the `AppIcon`
vectors used for macOS 26 and subsequent releases are of the new, 2025-era Icon
Composer app icon. As users migrate to macOS 26 and subsequent releases, this
will likely be revisited.

### Compiling the asset catalog

To compile the asset catalog, invoke `//tools/mac/icons/compile_car.py`:

`python3 tools/mac/icons/compile_car.py chrome/app/theme/chromium/mac/Assets.xcassets`

(or the path to whichever `.xcassets` file you want to compile)

There is no need to specify the corresponding `.icon` file; its name will be
derived from the name of the `.xcassets` file specified. The script will compile
the two source files, and will put the files resulting from the asset catalog
compilation into the directory containing the source files that were processed.

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
