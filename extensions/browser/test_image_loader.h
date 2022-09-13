// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_IMAGE_LOADER_H_
#define EXTENSIONS_BROWSER_TEST_IMAGE_LOADER_H_

#include "base/run_loop.h"
#include "ui/gfx/image/image.h"

namespace extensions {

class Extension;

// Helper class for synchronously loading an extension image resource.
class TestImageLoader {
 public:
  TestImageLoader();

  TestImageLoader(const TestImageLoader&) = delete;
  TestImageLoader& operator=(const TestImageLoader&) = delete;

  ~TestImageLoader();

  // Loads an image to be used in test from |extension|.
  // The image will be loaded from the relative path |image_path|.
  static SkBitmap LoadAndGetExtensionBitmap(const Extension* extension,
                                            const std::string& image_path,
                                            int size);

 private:
  void OnImageLoaded(const gfx::Image& image);

  SkBitmap LoadAndGetBitmap(const Extension* extension,
                            const std::string& path,
                            int size);

  gfx::Image image_;
  base::OnceClosure loader_message_loop_quit_;
  bool waiting_ = false;
  bool image_loaded_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_IMAGE_LOADER_H_
