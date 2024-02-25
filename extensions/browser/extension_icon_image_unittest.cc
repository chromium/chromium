// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_image.h"

#include <vector>

#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "skia/ext/image_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

SkBitmap CreateBlankBitmapForScale(int size_dip,
                                   ui::ResourceScaleFactor scale_factor) {
  SkBitmap bitmap;
  const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
  bitmap.allocN32Pixels(static_cast<int>(size_dip * scale),
                        static_cast<int>(size_dip * scale));
  bitmap.eraseColor(SkColorSetARGB(0, 0, 0, 0));
  return bitmap;
}

SkBitmap EnsureBitmapSize(const SkBitmap& original, int size) {
  if (original.width() == size && original.height() == size)
    return original;

  SkBitmap resized = skia::ImageOperations::Resize(
      original, skia::ImageOperations::RESIZE_LANCZOS3, size, size);
  return resized;
}

// Used to test behavior including images defined by an image skia source.
// |GetImageForScale| simply returns image representation from the image given
// in the ctor.
class MockImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  explicit MockImageSkiaSource(const gfx::ImageSkia& image)
      : image_(image) {
  }
  ~MockImageSkiaSource() override {}

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    return image_.GetRepresentation(scale);
  }

 private:
  gfx::ImageSkia image_;
};

class ExtensionIconImageTest : public ExtensionsTest,
                               public IconImage::Observer {
 public:
  ExtensionIconImageTest()
      : image_loaded_count_(0), quit_in_image_loaded_(false) {}

  ExtensionIconImageTest(const ExtensionIconImageTest&) = delete;
  ExtensionIconImageTest& operator=(const ExtensionIconImageTest&) = delete;

  ~ExtensionIconImageTest() override {}

  void WaitForImageLoad() {
    base::RunLoop loop;
    quit_closure_ = loop.QuitWhenIdleClosure();
    quit_in_image_loaded_ = true;
    loop.Run();
    quit_in_image_loaded_ = false;
  }

  int ImageLoadedCount() {
    int result = image_loaded_count_;
    image_loaded_count_ = 0;
    return result;
  }

  scoped_refptr<Extension> CreateExtension(const char* name,
                                           ManifestLocation location) {
    // Create and load an extension.
    base::FilePath test_file;
    if (!base::PathService::Get(DIR_TEST_DATA, &test_file)) {
      EXPECT_FALSE(true);
      return nullptr;
    }
    test_file = test_file.AppendASCII(name);
    int error_code = 0;
    std::string error;
    JSONFileValueDeserializer deserializer(
        test_file.AppendASCII("manifest.json"));
    std::unique_ptr<base::Value> valid_value =
        deserializer.Deserialize(&error_code, &error);
    EXPECT_EQ(0, error_code) << error;
    if (error_code != 0)
      return nullptr;

    EXPECT_TRUE(valid_value);
    if (!valid_value)
      return nullptr;

    const base::Value::Dict* valid_dict = valid_value->GetIfDict();
    EXPECT_TRUE(valid_dict);
    if (!valid_dict)
      return nullptr;

    return Extension::Create(test_file, location, *valid_dict,
                             Extension::NO_FLAGS, &error);
  }

  // IconImage::Delegate overrides:
  void OnExtensionIconImageChanged(IconImage* image) override {
    image_loaded_count_++;
    if (quit_in_image_loaded_)
      std::move(quit_closure_).Run();
  }

  gfx::ImageSkia GetDefaultIcon() {
    return gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(16, 16), 1.0f));
  }

 private:
  int image_loaded_count_;
  bool quit_in_image_loaded_;
  base::OnceClosure quit_closure_;
};

}  // namespace

TEST_F(ExtensionIconImageTest, Basic) {
  ui::test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {ui::k100Percent, ui::k200Percent});
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 =
      TestImageLoader::LoadAndGetExtensionBitmap(extension.get(), "16.png", 16);
  ASSERT_FALSE(bitmap_16.empty());

  // There is no image of size 32 defined in the extension manifest, so we
  // should expect manifest image of size 48 resized to size 32.
  SkBitmap bitmap_48_resized_to_32 =
      TestImageLoader::LoadAndGetExtensionBitmap(extension.get(), "48.png", 32);
  ASSERT_FALSE(bitmap_48_resized_to_32.empty());

  IconImage image(browser_context(),
                  extension.get(),
                  IconsInfo::GetIcons(extension.get()),
                  16,
                  default_icon,
                  this);

  // No representations in |image_| yet.
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_skia().image_reps();
  ASSERT_EQ(0u, image_reps.size());

  // Gets representation for a scale factor.
  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  // Before the image representation is loaded, image should contain blank
  // image representation.
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(representation.GetBitmap(),
                           CreateBlankBitmapForScale(16, ui::k100Percent)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);

  // We should get the right representation now.
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.GetBitmap(), bitmap_16));
  EXPECT_EQ(16, representation.pixel_width());

  // Gets representation for an additional scale factor.
  representation = image.image_skia().GetRepresentation(2.0f);

  EXPECT_TRUE(
      gfx::BitmapsAreEqual(representation.GetBitmap(),
                           CreateBlankBitmapForScale(16, ui::k200Percent)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(2u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(2.0f);

  // Image should have been resized.
  EXPECT_EQ(32, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.GetBitmap(),
                                   bitmap_48_resized_to_32));
}

// There is no resource with either exact or bigger size, but there is a smaller
// resource.
TEST_F(ExtensionIconImageTest, FallbackToSmallerWhenNoBigger) {
  ui::test::ScopedSetSupportedResourceScaleFactors scoped_supported(
      {ui::k100Percent, ui::k200Percent});
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_48 =
      TestImageLoader::LoadAndGetExtensionBitmap(extension.get(), "48.png", 48);
  ASSERT_FALSE(bitmap_48.empty());

  IconImage image(browser_context(),
                  extension.get(),
                  IconsInfo::GetIcons(extension.get()),
                  32,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(2.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(2.0f);

  // We should have loaded the biggest smaller resource resized to the actual
  // size.
  EXPECT_EQ(2.0f, representation.scale());
  EXPECT_EQ(64, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.GetBitmap(),
                                   EnsureBitmapSize(bitmap_48, 64)));
}

// There is no resource with exact size, but there is a smaller and a bigger
// one. The bigger resource should be loaded.
TEST_F(ExtensionIconImageTest, FallbackToBigger) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_24 =
      TestImageLoader::LoadAndGetExtensionBitmap(extension.get(), "24.png", 24);
  ASSERT_FALSE(bitmap_24.empty());

  IconImage image(browser_context(),
                  extension.get(),
                  IconsInfo::GetIcons(extension.get()),
                  17,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);

  // We should have loaded the smallest bigger (resized) resource.
  EXPECT_EQ(1.0f, representation.scale());
  EXPECT_EQ(17, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.GetBitmap(),
                                   EnsureBitmapSize(bitmap_24, 17)));
}

// If resource set is empty, |GetRepresentation| should synchronously return
// default icon, without notifying observer of image change.
TEST_F(ExtensionIconImageTest, NoResources) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  ExtensionIconSet empty_icon_set;
  gfx::ImageSkia default_icon = GetDefaultIcon();

  const int kRequestedSize = 24;
  IconImage image(browser_context(),
                  extension.get(),
                  empty_icon_set,
                  kRequestedSize,
                  default_icon,
                  this);

  // Default icon is loaded asynchronously.
  image.image_skia().GetRepresentation(1.0f);
  base::RunLoop().RunUntilIdle();
  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      EnsureBitmapSize(default_icon.GetRepresentation(1.0f).GetBitmap(),
                       kRequestedSize)));

  EXPECT_EQ(1, ImageLoadedCount());
  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      EnsureBitmapSize(default_icon.GetRepresentation(1.0f).GetBitmap(),
                       kRequestedSize)));
}

// If resource set is invalid, image load should be done asynchronously and
// the observer should be notified when it's done. |GetRepresentation| should
// return the default icon representation once image load is done.
TEST_F(ExtensionIconImageTest, InvalidResource) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  const int kInvalidIconSize = 24;
  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(kInvalidIconSize, "invalid.png");

  gfx::ImageSkia default_icon = GetDefaultIcon();

  IconImage image(browser_context(),
                  extension.get(),
                  invalid_icon_set,
                  kInvalidIconSize,
                  default_icon,
                  this);

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      CreateBlankBitmapForScale(kInvalidIconSize, ui::k100Percent)));

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      EnsureBitmapSize(default_icon.GetRepresentation(1.0f).GetBitmap(),
                       kInvalidIconSize)));
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns synchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(
      std::make_unique<MockImageSkiaSource>(default_icon), default_icon.size());

  ExtensionIconSet empty_icon_set;

  const int kRequestedSize = 128;
  IconImage image(browser_context(),
                  extension.get(),
                  empty_icon_set,
                  kRequestedSize,
                  lazy_default_icon,
                  this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(1.0f));

  // Default icon is loaded asynchronously.
  image.image_skia().GetRepresentation(1.0f);
  base::RunLoop().RunUntilIdle();
  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  // The resouce set is empty, so we should get the result right away.
  EXPECT_TRUE(lazy_default_icon.HasRepresentation(1.0f));
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      EnsureBitmapSize(default_icon.GetRepresentation(1.0f).GetBitmap(),
                       kRequestedSize)));

  // We should have a default icon representation.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());
}

// Test that IconImage works with lazily (but synchronously) created default
// icon when IconImage returns asynchronously.
TEST_F(ExtensionIconImageTest, LazyDefaultIcon_AsyncIconImage) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();
  gfx::ImageSkia lazy_default_icon(
      std::make_unique<MockImageSkiaSource>(default_icon), default_icon.size());

  const int kInvalidIconSize = 24;
  ExtensionIconSet invalid_icon_set;
  invalid_icon_set.Add(kInvalidIconSize, "invalid.png");

  IconImage image(browser_context(),
                  extension.get(),
                  invalid_icon_set,
                  kInvalidIconSize,
                  lazy_default_icon,
                  this);

  ASSERT_FALSE(lazy_default_icon.HasRepresentation(1.0f));

  gfx::ImageSkiaRep representation = image.image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  // We should have default icon representation now.
  ASSERT_EQ(1u, image.image_skia().image_reps().size());

  EXPECT_TRUE(lazy_default_icon.HasRepresentation(1.0f));

  representation = image.image_skia().GetRepresentation(1.0f);
  EXPECT_TRUE(gfx::BitmapsAreEqual(
      representation.GetBitmap(),
      EnsureBitmapSize(default_icon.GetRepresentation(1.0f).GetBitmap(),
                       kInvalidIconSize)));
}

// Tests behavior of image created by IconImage after IconImage host goes
// away. The image should still return loaded representations. If requested
// representation was not loaded while IconImage host was around, transparent
// representations should be returned.
TEST_F(ExtensionIconImageTest, IconImageDestruction) {
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);

  gfx::ImageSkia default_icon = GetDefaultIcon();

  // Load images we expect to find as representations in icon_image, so we
  // can later use them to validate icon_image.
  SkBitmap bitmap_16 =
      TestImageLoader::LoadAndGetExtensionBitmap(extension.get(), "16.png", 16);
  ASSERT_FALSE(bitmap_16.empty());

  std::unique_ptr<IconImage> image(new IconImage(
      browser_context(), extension.get(), IconsInfo::GetIcons(extension.get()),
      16, default_icon, this));

  // Load an image representation.
  gfx::ImageSkiaRep representation =
      image->image_skia().GetRepresentation(1.0f);

  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  ASSERT_EQ(1u, image->image_skia().image_reps().size());

  // Stash loaded image skia, and destroy |image|.
  gfx::ImageSkia image_skia = image->image_skia();
  image.reset();
  extension = nullptr;

  // Image skia should still be able to get previously loaded representation.
  representation = image_skia.GetRepresentation(1.0f);

  EXPECT_EQ(1.0f, representation.scale());
  EXPECT_EQ(16, representation.pixel_width());
  EXPECT_TRUE(gfx::BitmapsAreEqual(representation.GetBitmap(), bitmap_16));

  // When requesting another representation, we should not crash and return some
  // image of the size. It could be blank or a rescale from the existing 1.0f
  // icon.
  representation = image_skia.GetRepresentation(2.0f);

  EXPECT_EQ(16, representation.GetWidth());
  EXPECT_EQ(16, representation.GetHeight());
  EXPECT_EQ(2.0f, representation.scale());
}

// Test that new representations added to the image of an IconImageSkia are
// cached for future use.
TEST_F(ExtensionIconImageTest, ImageCachesNewRepresentations) {
  // Load up an extension and create an icon image.
  scoped_refptr<Extension> extension(CreateExtension(
      "extension_icon_image", ManifestLocation::kInvalidLocation));
  ASSERT_TRUE(extension.get() != nullptr);
  gfx::ImageSkia default_icon = GetDefaultIcon();
  std::unique_ptr<IconImage> icon_image(new IconImage(
      browser_context(), extension.get(), IconsInfo::GetIcons(extension.get()),
      16, default_icon, this));

  // Load an blank image representation.
  EXPECT_EQ(0, ImageLoadedCount());
  icon_image->image_skia().GetRepresentation(1.0f);
  EXPECT_EQ(0, ImageLoadedCount());
  WaitForImageLoad();
  EXPECT_EQ(1, ImageLoadedCount());
  icon_image->image_skia().GetRepresentation(1.0f);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, ImageLoadedCount());
  icon_image->image_skia().GetRepresentation(1.0f);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, ImageLoadedCount());
}

}  // namespace extensions
