pdf_cg test expectations
---
The PNG files in this directory are the CoreGraphics rendering outputs for PDFs
in //pdf/test/data/. They are generated from raw bitmaps using
gfx::PNGCodec::Encode() using the |gfx::PNGCodec::FORMAT_BGRA| format. The PNGs
are further optimized with optipng. Each PNG file is named after the
PdfMetafileCgTest instance that uses it.
