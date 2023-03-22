# PDF thumbnail test expectations

The PNG files in this directory are the thumbnail rendering outputs for PDFs in
`//pdf/test/data/`. They are generated from raw bitmaps using
`gfx::PNGCodec::Encode()` using the `gfx::PNGCodec::FORMAT_RGBA` format. The
PNGs are further optimized with `optipng`.

Each PNG file is named using the PDF file name and zero-based page number, and
is located in a directory corresponding to the device to pixel ratio. For
example, the file located at `2.0x/variable_page_sizes_expected.pdf.3.png` is
the thumbnail rendering of the fourth page of `variable_page_sizes.pdf` with a
device to pixel ratio of 2.0.

Also some PNG file names contain extra renderer type and device information.
For example, the file "variable_page_sizes_arm64_expected_skia.pdf.3.png"
is generated with Skia renderer on a macOS device with ARM64 CPU. If the
renderer type and the device information are not included, by default the PNG
files are generated with AGG renderer using a device with non-ARM64 CPU.
