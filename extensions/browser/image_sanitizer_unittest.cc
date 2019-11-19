// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/image_sanitizer.h"

#include <map>
#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kBase64edValidPng[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd"
    "1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC";

constexpr char kBase64edInvalidPng[] =
    "Rw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd"
    "1PeAAAADElEQVQI12P4//8/AAX+Av7czFnnAAAAAElFTkSuQmCC";

class RefCountedSanitizerCallback
    : public base::RefCountedThreadSafe<RefCountedSanitizerCallback> {
 public:
  explicit RefCountedSanitizerCallback(bool* deleted) : deleted_(deleted) {}

  void ImageSanitizationDone(ImageSanitizer::Status status,
                             const base::FilePath& path) {
    if (done_callback_)
      std::move(done_callback_).Run();
  }
  void ImageSanitizerDecodedImage(const base::FilePath& path, SkBitmap image) {}

  void WaitForSanitizationDone() {
    ASSERT_FALSE(done_callback_);
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  friend class base::RefCountedThreadSafe<RefCountedSanitizerCallback>;
  ~RefCountedSanitizerCallback() {
    if (deleted_)
      *deleted_ = true;
  }

  base::OnceClosure done_callback_;
  bool* deleted_;
};

class ImageSanitizerTest : public testing::Test {
 public:
  ImageSanitizerTest() = default;

 protected:
  void CreateValidImage(const base::FilePath::StringPieceType& file_name) {
    ASSERT_TRUE(WriteBase64DataToFile(kBase64edValidPng, file_name));
  }

  void CreateInvalidImage(const base::FilePath::StringPieceType& file_name) {
    ASSERT_TRUE(WriteBase64DataToFile(kBase64edInvalidPng, file_name));
  }

  const base::FilePath& GetImagePath() const { return temp_dir_.GetPath(); }

  void WaitForSanitizationDone() {
    ASSERT_FALSE(done_callback_);
    base::RunLoop run_loop;
    done_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CreateAndStartSanitizer(
      const std::set<base::FilePath>& image_relative_paths) {
    CreateAndStartSanitizerWithCallbacks(
        image_relative_paths,
        base::BindRepeating(&ImageSanitizerTest::ImageSanitizerDecodedImage,
                            base::Unretained(this)),
        base::BindOnce(&ImageSanitizerTest::ImageSanitizationDone,
                       base::Unretained(this)));
  }

  void TestDontHoldOnToCallback(const base::FilePath& image_path) {
    bool callback_deleted = false;
    scoped_refptr<RefCountedSanitizerCallback> sanitizer_callback =
        base::MakeRefCounted<RefCountedSanitizerCallback>(&callback_deleted);
    CreateAndStartSanitizerWithCallbacks(
        {image_path},
        base::BindRepeating(
            &RefCountedSanitizerCallback::ImageSanitizerDecodedImage,
            sanitizer_callback),
        base::BindOnce(&RefCountedSanitizerCallback::ImageSanitizationDone,
                       sanitizer_callback));

    sanitizer_callback->WaitForSanitizationDone();
    sanitizer_callback = nullptr;

    // The sanitizer should have cleared its reference to |sanitizer_callback|.
    EXPECT_TRUE(callback_deleted);
  }

  ImageSanitizer::Status last_reported_status() const { return last_status_; }

  const base::FilePath& last_reported_path() const {
    return last_reported_path_;
  }

  std::map<base::FilePath, SkBitmap>* decoded_images() {
    return &decoded_images_;
  }

  void ClearSanitizer() { sanitizer_.reset(); }

  bool done_callback_called() const { return done_callback_called_; }

  bool decoded_image_callback_called() const {
    return decoded_image_callback_called_;
  }

  data_decoder::test::InProcessDataDecoder& in_process_data_decoder() {
    return in_process_data_decoder_;
  }

 private:
  void CreateAndStartSanitizerWithCallbacks(
      const std::set<base::FilePath>& image_relative_paths,
      ImageSanitizer::ImageDecodedCallback image_decoded_callback,
      ImageSanitizer::SanitizationDoneCallback done_callback) {
    sanitizer_ = ImageSanitizer::CreateAndStart(
        &data_decoder_, temp_dir_.GetPath(), image_relative_paths,
        std::move(image_decoded_callback), std::move(done_callback));
  }

  bool WriteBase64DataToFile(const std::string& base64_data,
                             const base::FilePath::StringPieceType& file_name) {
    std::string binary;
    if (!base::Base64Decode(base64_data, &binary))
      return false;

    base::FilePath path = temp_dir_.GetPath().Append(file_name);
    return base::WriteFile(path, binary.data(), binary.size());
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void ImageSanitizationDone(ImageSanitizer::Status status,
                             const base::FilePath& path) {
    done_callback_called_ = true;
    last_status_ = status;
    last_reported_path_ = path;
    if (done_callback_)
      std::move(done_callback_).Run();
  }

  void ImageSanitizerDecodedImage(const base::FilePath& path, SkBitmap image) {
    EXPECT_EQ(decoded_images()->find(path), decoded_images()->end());
    (*decoded_images())[path] = image;
    decoded_image_callback_called_ = true;
  }

  content::BrowserTaskEnvironment task_environment_;
  ImageSanitizer::Status last_status_ = ImageSanitizer::Status::kSuccess;
  base::FilePath last_reported_path_;
  base::OnceClosure done_callback_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  data_decoder::DataDecoder data_decoder_;
  std::unique_ptr<ImageSanitizer> sanitizer_;
  base::ScopedTempDir temp_dir_;
  std::map<base::FilePath, SkBitmap> decoded_images_;
  bool done_callback_called_ = false;
  bool decoded_image_callback_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(ImageSanitizerTest);
};

}  // namespace

TEST_F(ImageSanitizerTest, NoImagesProvided) {
  CreateAndStartSanitizer(std::set<base::FilePath>());
  WaitForSanitizationDone();
  EXPECT_TRUE(done_callback_called());
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kSuccess);
  EXPECT_FALSE(decoded_image_callback_called());
  EXPECT_TRUE(last_reported_path().empty());
}

TEST_F(ImageSanitizerTest, InvalidPathAbsolute) {
  base::FilePath normal_path(FILE_PATH_LITERAL("hello.png"));
#if defined(OS_WIN)
  base::FilePath absolute_path(FILE_PATH_LITERAL("c:\\Windows\\win32"));
#else
  base::FilePath absolute_path(FILE_PATH_LITERAL("/usr/bin/root"));
#endif
  CreateAndStartSanitizer({normal_path, absolute_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kImagePathError);
  EXPECT_EQ(last_reported_path(), absolute_path);
}

TEST_F(ImageSanitizerTest, InvalidPathReferenceParent) {
  base::FilePath good_path(FILE_PATH_LITERAL("hello.png"));
  base::FilePath bad_path(FILE_PATH_LITERAL("hello"));
  bad_path = bad_path.Append(base::FilePath::kParentDirectory)
                 .Append(base::FilePath::kParentDirectory)
                 .Append(base::FilePath::kParentDirectory)
                 .Append(FILE_PATH_LITERAL("usr"))
                 .Append(FILE_PATH_LITERAL("bin"));
  CreateAndStartSanitizer({good_path, bad_path});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kImagePathError);
  EXPECT_EQ(last_reported_path(), bad_path);
}

TEST_F(ImageSanitizerTest, ValidCase) {
  constexpr std::array<const base::FilePath::CharType* const, 10> kFileNames{
      {FILE_PATH_LITERAL("image0.png"), FILE_PATH_LITERAL("image1.png"),
       FILE_PATH_LITERAL("image2.png"), FILE_PATH_LITERAL("image3.png"),
       FILE_PATH_LITERAL("image4.png"), FILE_PATH_LITERAL("image5.png"),
       FILE_PATH_LITERAL("image6.png"), FILE_PATH_LITERAL("image7.png"),
       FILE_PATH_LITERAL("image8.png"), FILE_PATH_LITERAL("image9.png")}};
  std::set<base::FilePath> paths;
  for (const base::FilePath::CharType* file_name : kFileNames) {
    CreateValidImage(file_name);
    paths.insert(base::FilePath(file_name));
  }
  CreateAndStartSanitizer(paths);
  WaitForSanitizationDone();
  EXPECT_TRUE(done_callback_called());
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kSuccess);
  EXPECT_TRUE(last_reported_path().empty());
  // Make sure the image files are there and non empty, and that the
  // ImageSanitizerDecodedImage callback was invoked for every image.
  for (const auto& path : paths) {
    int64_t file_size = 0;
    base::FilePath full_path = GetImagePath().Append(path);
    EXPECT_TRUE(base::GetFileSize(full_path, &file_size));
    EXPECT_GT(file_size, 0);

    ASSERT_TRUE(base::Contains(*decoded_images(), path));
    EXPECT_FALSE((*decoded_images())[path].drawsNothing());
  }
  // No extra images should have been reported.
  EXPECT_EQ(decoded_images()->size(), 10U);
}

TEST_F(ImageSanitizerTest, MissingImage) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("image.png");
  constexpr base::FilePath::CharType kNonExistingName[] =
      FILE_PATH_LITERAL("i_don_t_exist.png");
  CreateValidImage(kGoodPngName);
  base::FilePath good_png(kGoodPngName);
  base::FilePath bad_png(kNonExistingName);
  CreateAndStartSanitizer({good_png, bad_png});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kFileReadError);
  EXPECT_EQ(last_reported_path(), bad_png);
}

TEST_F(ImageSanitizerTest, InvalidImage) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("good.png");
  constexpr base::FilePath::CharType kBadPngName[] =
      FILE_PATH_LITERAL("bad.png");
  CreateValidImage(kGoodPngName);
  CreateInvalidImage(kBadPngName);
  base::FilePath good_png(kGoodPngName);
  base::FilePath bad_png(kBadPngName);
  CreateAndStartSanitizer({good_png, bad_png});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kDecodingError);
  EXPECT_EQ(last_reported_path(), bad_png);
}

TEST_F(ImageSanitizerTest, NoCallbackAfterDelete) {
  constexpr base::FilePath::CharType kBadPngName[] =
      FILE_PATH_LITERAL("bad.png");
  CreateInvalidImage(kBadPngName);
  base::FilePath bad_png(kBadPngName);
  CreateAndStartSanitizer({bad_png});
  // Delete the sanitizer before we have received the callback.
  ClearSanitizer();
  // Wait a bit and ensure no callback has been called.
  base::RunLoop run_loop;
  base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                        base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();
  EXPECT_FALSE(done_callback_called());
  EXPECT_FALSE(decoded_image_callback_called());
}

// Ensures the sanitizer does not keep a reference to the callbacks to prevent
// memory leaks. (it's typical to have a ref counted object A own an
// ImageSanitizer which is given callbacks bound to A, creating a circular
// reference)
TEST_F(ImageSanitizerTest, DontHoldOnToCallbacksOnFailure) {
  constexpr base::FilePath::CharType kBadPngName[] =
      FILE_PATH_LITERAL("bad.png");
  CreateInvalidImage(kBadPngName);
  TestDontHoldOnToCallback(base::FilePath(kBadPngName));
}

TEST_F(ImageSanitizerTest, DontHoldOnToCallbacksOnSuccess) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("good.png");
  CreateValidImage(kGoodPngName);
  TestDontHoldOnToCallback(base::FilePath(kGoodPngName));
}

// Tests that the callback is invoked if the data decoder service crashes.
TEST_F(ImageSanitizerTest, DataDecoderServiceCrashes) {
  constexpr base::FilePath::CharType kGoodPngName[] =
      FILE_PATH_LITERAL("good.png");
  in_process_data_decoder().service().SimulateImageDecoderCrashForTesting(true);
  CreateValidImage(kGoodPngName);
  base::FilePath good_png(kGoodPngName);
  CreateAndStartSanitizer({good_png});
  WaitForSanitizationDone();
  EXPECT_EQ(last_reported_status(), ImageSanitizer::Status::kServiceError);
}

}  // namespace extensions
