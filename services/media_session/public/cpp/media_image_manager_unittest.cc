// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_image_manager.h"

#include "base/hash/hash.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

namespace {

const int kMinSize = 10;
const int kIdealSize = 40;

}  // namespace

class MediaImageManagerTest : public testing::Test {
 public:
  MediaImageManagerTest() = default;

  void SetUp() override {
    manager_ = std::make_unique<MediaImageManager>(kMinSize, kIdealSize);
  }

  MediaImageManager* manager() const { return manager_.get(); }

 private:
  std::unique_ptr<MediaImageManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaImageManagerTest);
};

TEST_F(MediaImageManagerTest, CheckExpectedImageExtensionHashes) {
  const std::string extensions[] = {".png", ".jpeg", ".jpg",
                                    ".bmp", ".icon", ".gif"};

  for (const auto& extension : extensions) {
    // Uncomment this line to print the hashes if new ones need to be added.
    // printf("0x%x %s\n", base::PersistentHash(extension), extension.c_str());

    GURL url("https://www.example.com/test" + extension);
    EXPECT_TRUE(MediaImageManager::GetImageExtensionScore(url));
  }
}

TEST_F(MediaImageManagerTest, CheckExpectedImageTypeHashes) {
  const std::string types[] = {"image/bmp", "image/gif", "image/jpeg",
                               "image/png", "image/x-icon"};

  for (const auto& type : types) {
    base::string16 type16 = base::ASCIIToUTF16(type);

    // Uncomment these lines to print the hashes if new ones need to be added.
    // printf("0x%x %s\n",
    //        base::PersistentHash(type16.data(),
    //                             type16.size() * sizeof(base::char16)),
    //        type.c_str());

    EXPECT_TRUE(MediaImageManager::GetImageTypeScore(type16));
  }
}

TEST_F(MediaImageManagerTest, PickImageFromMimeType) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.type = base::ASCIIToUTF16("image/bmp");
  image1.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image1);

  MediaImage image2;
  image2.type = base::ASCIIToUTF16("image/png");
  image2.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image2);

  EXPECT_EQ(image2, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, PickImageFromExtension) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.src = GURL("https://www.example.com/test.bmp");
  image1.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image1);

  MediaImage image2;
  image2.src = GURL("https://www.example.com/test.PNG");
  image2.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image2);

  MediaImage image3;
  image3.src = GURL("https://www.example.com/test");
  image3.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image3);

  EXPECT_EQ(image2, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, IgnoreImageTooSmall) {
  std::vector<MediaImage> images;

  MediaImage image;
  image.sizes.push_back(gfx::Size(1, 1));
  images.push_back(image);

  EXPECT_FALSE(manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, PickImageUseDefaultScoreIfNoSize) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.src = GURL("https://www.example.com/test.bmp");
  image1.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image1);

  MediaImage image2;
  image2.src = GURL("https://www.example.com/test.PNG");
  images.push_back(image2);

  EXPECT_EQ(image1, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, PickImageCloserToIdeal) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image1);

  MediaImage image2;
  image2.sizes.push_back(gfx::Size(kMinSize, kMinSize));
  images.push_back(image2);

  EXPECT_EQ(image1, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, PickImageWithMultipleSizes) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.sizes.push_back(gfx::Size(kIdealSize - 5, kIdealSize - 5));
  images.push_back(image1);

  MediaImage image2;
  image2.sizes.push_back(gfx::Size(kMinSize, kMinSize));
  image2.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image2);

  EXPECT_EQ(image2, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, PickImageWithBetterAspectRatio) {
  std::vector<MediaImage> images;

  MediaImage image1;
  image1.sizes.push_back(gfx::Size(kIdealSize, kIdealSize));
  images.push_back(image1);

  MediaImage image2;
  image2.sizes.push_back(gfx::Size(kIdealSize, kMinSize));
  images.push_back(image2);

  EXPECT_EQ(image1, manager()->SelectImage(images));
}

TEST_F(MediaImageManagerTest, MinAndIdealAndImageSizeAreSame) {
  MediaImageManager manager(10, 10);

  std::vector<MediaImage> images;

  MediaImage image;
  image.sizes.push_back(gfx::Size(10, 10));
  images.push_back(image);

  EXPECT_TRUE(manager.SelectImage(images));
}

}  // namespace media_session
