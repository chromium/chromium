// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_image_loader.h"

#include "base/functional/bind.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace extensions {

TestImageLoader::TestImageLoader() = default;

TestImageLoader::~TestImageLoader() = default;

// static
SkBitmap TestImageLoader::LoadAndGetExtensionBitmap(
    const Extension* extension,
    const std::string& image_path,
    int size) {
  TestImageLoader image_loader;
  return image_loader.LoadAndGetBitmap(extension, image_path, size);
}

void TestImageLoader::OnImageLoaded(const gfx::Image& image) {
  image_ = image;
  image_loaded_ = true;
  if (waiting_) {
    std::move(loader_message_loop_quit_).Run();
  }
}

SkBitmap TestImageLoader::LoadAndGetBitmap(const Extension* extension,
                                           const std::string& path,
                                           int size) {
  image_loaded_ = false;

  ImageLoader image_loader;
  image_loader.LoadImageAsync(
      extension, extension->GetResource(path), gfx::Size(size, size),
      base::BindOnce(&TestImageLoader::OnImageLoaded, base::Unretained(this)));

  // If |image_| still hasn't been loaded (i.e. it is being loaded
  // asynchronously), wait for it.
  if (!image_loaded_) {
    waiting_ = true;
    base::RunLoop run_loop;
    loader_message_loop_quit_ = run_loop.QuitClosure();
    run_loop.Run();
    waiting_ = false;
  }

  DCHECK(image_loaded_);

  return image_.IsEmpty() ? SkBitmap() : *image_.ToSkBitmap();
}

}  // namespace extensions
