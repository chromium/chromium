// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum ColorScheme {
  "dark",
  "light"
};

dictionary IconVariant {
  // Optional DOMString path to an icon that should be used with any possible
  // size.
  DOMString _any;

  // Optional ColorScheme.
  sequence<ColorScheme> color_schemes;
};

// Use `iconVariants` for icon declarations with support for color schemes.
// In addition to these keys, a size key is allowed with a value of the file
// path to the image.
[generate_error_messages, nodoc, Namespace=iconVariants]
partial dictionary ExtensionManifest {
  sequence<IconVariant> icon_variants;
};
