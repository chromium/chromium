// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/ico/ico_rust_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateICODecoder() {
  return std::make_unique<ICOImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}
}  // namespace

TEST(ICOImageDecoderTests, trunctedIco) {
  const Vector<char> data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data.empty());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(base::span(data).first(data.size() / 2));
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());

  decoder->SetData(truncated_data.get(), true);
  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, errorInPngInIco) {
  const Vector<char> data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_FALSE(data.empty());

  // Modify the file to have a broken CRC in IHDR.
  constexpr size_t kCrcOffset = 22 + 29;
  constexpr size_t kCrcSize = 4;
  scoped_refptr<SharedBuffer> modified_data =
      SharedBuffer::Create(base::span(data).first(kCrcOffset));
  Vector<char> bad_crc(kCrcSize, 0);
  modified_data->Append(bad_crc);
  modified_data->Append(base::span(data).subspan(kCrcOffset + kCrcSize));

  auto decoder = CreateICODecoder();
  decoder->SetData(modified_data.get(), true);

  // ICOImageDecoder reports the frame count based on whether enough data has
  // been received according to the icon directory. So even though the
  // embedded PNG is broken, there is enough data to include it in the frame
  // count.
  EXPECT_EQ(1u, decoder->FrameCount());

  decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_TRUE(decoder->Failed());
}

TEST(ICOImageDecoderTests, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/png-in-ico.ico",
                       1u, kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/2entries.ico", 2u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/images/resources/greenbox-3frames.cur", 3u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder,
                       "/images/resources/icon-without-and-bitmap.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/1bit.ico", 1u,
                       kAnimationNone);
  TestByteByByteDecode(&CreateICODecoder, "/images/resources/bug653075.ico", 2u,
                       kAnimationNone);
}

TEST(ICOImageDecoderTests, NullData) {
  static constexpr size_t kSizeOfBadBlock = 6 + 16 + 1;

  Vector<char> ico_file_data = ReadFile("/images/resources/png-in-ico.ico");
  ASSERT_LT(kSizeOfBadBlock, ico_file_data.size());

  scoped_refptr<SharedBuffer> truncated_data =
      SharedBuffer::Create(base::span(ico_file_data).first(kSizeOfBadBlock));
  auto decoder = CreateICODecoder();

  decoder->SetData(truncated_data.get(), false);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());

  auto* frame = decoder->DecodeFrameBufferAtIndex(0);
  EXPECT_EQ(nullptr, frame);

  decoder->SetData(scoped_refptr<SegmentReader>(nullptr), false);
  decoder->ClearCacheExceptFrame(0);
  decoder->SetMemoryAllocator(nullptr);
  EXPECT_FALSE(decoder->Failed());
}

class ICOImageDecoderCorpusTest : public ImageDecoderBaseTest {
 public:
  ICOImageDecoderCorpusTest() : ImageDecoderBaseTest("ico") {}

 protected:
  std::unique_ptr<ImageDecoder> CreateImageDecoder() const override {
    return std::make_unique<ICOImageDecoder>(
        ImageDecoder::kAlphaPremultiplied, ColorBehavior::kTransformToSRGB,
        ImageDecoder::kNoDecodedImageByteLimit);
  }
};

TEST_F(ICOImageDecoderCorpusTest, Decoding) {
  TestDecoding();
}

TEST_F(ICOImageDecoderCorpusTest, ImageNonZeroFrameIndex) {
  // Test that the decoder decodes multiple sizes of icons which have them.
  // Load an icon that has both favicon-size and larger entries.
  base::FilePath multisize_icon_path(data_dir().AppendASCII("yahoo.ico"));

  // data_dir may not exist without src_internal checkouts.
  if (!base::PathExists(multisize_icon_path)) {
    return;
  }
  const base::FilePath md5_sum_path(GetMD5SumPath(multisize_icon_path).value() +
                                    FILE_PATH_LITERAL("2"));
  static const int kDesiredFrameIndex = 3;
  TestImageDecoder(multisize_icon_path, md5_sum_path, kDesiredFrameIndex);
}

namespace {

// Describes the type of expected difference between ICOImageDecoder (C++) and
// IcoRustImageDecoder.
enum class DifferenceType {
  kNone,       // Decoders should produce identical results.
  kAlphaType,  // Identical pixels, but the reported SkAlphaType differs.
  kPixelData   // Both decode successfully but produce different pixels.
};

// Documents whether (and why) the two decoders are expected to differ for a
// given input file.
struct ExpectedDifference {
  DifferenceType type = DifferenceType::kNone;
  std::string description;

  bool HasDifference() const { return type != DifferenceType::kNone; }
  // kPixelData implies the alpha type may differ too, since the whole pixel
  // comparison is skipped.
  bool AllowsAlphaTypeDifference() const {
    return type == DifferenceType::kAlphaType ||
           type == DifferenceType::kPixelData;
  }
  bool AllowsPixelDifference() const {
    return type == DifferenceType::kPixelData;
  }
};

ExpectedDifference NoDifference() {
  return ExpectedDifference{};
}

// Documents an expected difference limited to the reported SkAlphaType: the two
// decoders produce identical pixels, but disagree on the alpha type metadata
// (e.g. C++ reports kOpaque while Rust reports kUnpremul). Pixel data is still
// compared strictly.
ExpectedDifference AlphaTypeDiff(std::string desc) {
  return ExpectedDifference{DifferenceType::kAlphaType, std::move(desc)};
}

// Documents an expected pixel-level difference between the two decoders for a
// particular input. Currently unused, but kept available so that files where
// the Rust and C++ decoders legitimately diverge can be annotated with the
// reason (mirroring the BMP comparison test).
[[maybe_unused]] ExpectedDifference PixelDiff(std::string desc) {
  return ExpectedDifference{DifferenceType::kPixelData, std::move(desc)};
}

std::unique_ptr<ImageDecoder> CreateCppICODecoder() {
  return std::make_unique<ICOImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreateRustICODecoder() {
  return std::make_unique<IcoRustImageDecoder>(
      ImageDecoder::kAlphaNotPremultiplied, ColorBehavior::kTransformToSRGB,
      ImageDecoder::kNoDecodedImageByteLimit);
}

// Returns true if every pixel in `bitmap` is fully opaque (alpha == 255),
// regardless of the bitmap's reported SkAlphaType.
bool IsFullyOpaque(const SkBitmap& bitmap) {
  for (int y = 0; y < bitmap.height(); ++y) {
    for (int x = 0; x < bitmap.width(); ++x) {
      if (SkColorGetA(bitmap.getColor(x, y)) != SK_AlphaOPAQUE) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

// Compares ICOImageDecoder (C++) against IcoRustImageDecoder for a single ICO
// or CUR file, verifying they produce identical output unless a difference is
// explicitly expected.
//
// Test parameter is a tuple of:
// - the resource path of the file under test.
// - an ExpectedDifference documenting the type/reason of any known difference.
class ICODecoderComparisonTest
    : public testing::TestWithParam<
          std::tuple<const char*, ExpectedDifference>> {};

TEST_P(ICODecoderComparisonTest, CompareDecoders) {
  const auto& [ico_path, expected_diff] = GetParam();

  scoped_refptr<SharedBuffer> data = ReadFileToSharedBuffer(ico_path);
  ASSERT_NE(data.get(), nullptr) << "Unable to load: " << ico_path;
  ASSERT_FALSE(data->empty());

  // If there's an expected difference, log it for documentation.
  if (expected_diff.HasDifference()) {
    SCOPED_TRACE("Expected difference: " + expected_diff.description);
  }

  std::unique_ptr<ImageDecoder> cpp_decoder = CreateCppICODecoder();
  std::unique_ptr<ImageDecoder> rust_decoder = CreateRustICODecoder();

  cpp_decoder->SetData(data.get(), true);
  rust_decoder->SetData(data.get(), true);

  // Compare size availability.
  bool cpp_size_available = cpp_decoder->IsSizeAvailable();
  bool rust_size_available = rust_decoder->IsSizeAvailable();
  EXPECT_EQ(cpp_size_available, rust_size_available)
      << ico_path << ": IsSizeAvailable mismatch"
      << " (ICOImageDecoder: " << cpp_size_available
      << ", IcoRustImageDecoder: " << rust_size_available << ")";
  if (!cpp_size_available || !rust_size_available) {
    return;
  }

  // Compare dimensions.
  EXPECT_EQ(cpp_decoder->Size().width(), rust_decoder->Size().width())
      << ico_path << ": Width mismatch";
  EXPECT_EQ(cpp_decoder->Size().height(), rust_decoder->Size().height())
      << ico_path << ": Height mismatch";

  // Decode frame 0 with both decoders.
  ImageFrame* cpp_frame = cpp_decoder->DecodeFrameBufferAtIndex(0);
  ImageFrame* rust_frame = rust_decoder->DecodeFrameBufferAtIndex(0);

  // Compare failure states after decoding.
  bool cpp_failed = cpp_decoder->Failed();
  bool rust_failed = rust_decoder->Failed();
  EXPECT_EQ(cpp_failed, rust_failed)
      << ico_path << ": Failed state mismatch after decode"
      << " (ICOImageDecoder: " << cpp_failed
      << ", IcoRustImageDecoder: " << rust_failed << ")";
  if (cpp_failed || rust_failed) {
    return;
  }

  // Compare frame validity.
  if (!cpp_frame || !rust_frame) {
    EXPECT_EQ(cpp_frame == nullptr, rust_frame == nullptr)
        << ico_path << ": Frame null mismatch";
    return;
  }

  // Compare frame status.
  EXPECT_EQ(cpp_frame->GetStatus(), rust_frame->GetStatus())
      << ico_path << ": Frame status mismatch";
  if (cpp_frame->GetStatus() != ImageFrame::kFrameComplete ||
      rust_frame->GetStatus() != ImageFrame::kFrameComplete) {
    return;
  }

  const SkBitmap& cpp_bitmap = cpp_frame->Bitmap();
  const SkBitmap& rust_bitmap = rust_frame->Bitmap();

  // Compare bitmap metadata - strict unless a pixel difference is expected.
  if (!expected_diff.AllowsPixelDifference()) {
    EXPECT_EQ(cpp_bitmap.width(), rust_bitmap.width())
        << ico_path << ": Bitmap width mismatch";
    EXPECT_EQ(cpp_bitmap.height(), rust_bitmap.height())
        << ico_path << ": Bitmap height mismatch";
    EXPECT_EQ(cpp_bitmap.colorType(), rust_bitmap.colorType())
        << ico_path << ": Bitmap color type mismatch";
    // The alpha type is metadata; some inputs legitimately differ here while
    // still producing identical pixels (see kAlphaType).
    if (!expected_diff.AllowsAlphaTypeDifference()) {
      EXPECT_EQ(cpp_bitmap.alphaType(), rust_bitmap.alphaType())
          << ico_path << ": Bitmap alpha type mismatch";
    } else {
      // When an alpha-type difference is tolerated, prove it is purely
      // cosmetic: both outputs must be fully opaque (every pixel alpha == 255),
      // so kOpaque vs kUnpremul renders identically and cannot introduce a
      // different (e.g. black) background.
      EXPECT_TRUE(IsFullyOpaque(cpp_bitmap))
          << ico_path << ": ICOImageDecoder output is not fully opaque, so the "
          << "tolerated alpha-type difference is NOT cosmetic";
      EXPECT_TRUE(IsFullyOpaque(rust_bitmap))
          << ico_path
          << ": IcoRustImageDecoder output is not fully opaque, so the "
          << "tolerated alpha-type difference is NOT cosmetic";
    }
  }

  // Compare pixel data - strict unless a pixel difference is expected.
  if (!expected_diff.AllowsPixelDifference() &&
      cpp_bitmap.width() == rust_bitmap.width() &&
      cpp_bitmap.height() == rust_bitmap.height() &&
      cpp_bitmap.colorType() == rust_bitmap.colorType()) {
    // Compare pixels via SkBitmap::getColor(), which performs bounds-checked
    // access and returns a normalized (unpremultiplied) SkColor. This keeps the
    // comparison agnostic to the underlying N32 byte order and avoids raw
    // pointer/span access.
    for (int y = 0; y < cpp_bitmap.height(); ++y) {
      for (int x = 0; x < cpp_bitmap.width(); ++x) {
        SkColor cpp_color = cpp_bitmap.getColor(x, y);
        SkColor rust_color = rust_bitmap.getColor(x, y);
        if (cpp_color != rust_color) {
          ADD_FAILURE() << ico_path << ": Pixel data mismatch at (x=" << x
                        << ", y=" << y << "). ICOImageDecoder value: 0x"
                        << std::hex << cpp_color
                        << ", IcoRustImageDecoder value: 0x" << rust_color;
          // Report only the first differing pixel to avoid log spam.
          return;
        }
      }
    }
  }
}

// Generates a readable, unique test name from the file's stem.
std::string ICOComparisonTestName(
    const testing::TestParamInfo<std::tuple<const char*, ExpectedDifference>>&
        info) {
  std::string name = std::get<0>(info.param);
  if (size_t slash = name.find_last_of('/'); slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  if (size_t dot = name.find_last_of('.'); dot != std::string::npos) {
    name = name.substr(0, dot);
  }
  // Replace any characters that are invalid in a test name with underscores.
  for (char& c : name) {
    if (!IsAsciiAlphanumeric(c) && c != '_') {
      c = '_';
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(
    ICOComparison,
    ICODecoderComparisonTest,
    testing::Values(
        // Single-image BMP-in-ICO files at various bit depths. For BMP entries
        // the C++ decoder inspects the AND mask and reports kOpaque when no
        // pixel is actually transparent, whereas the Rust codec conservatively
        // reports kUnpremul. The decoded pixels are identical.
        std::make_tuple(
            "/images/resources/1bit.ico",
            AlphaTypeDiff("BMP-in-ICO: C++ reports kOpaque, Rust kUnpremul")),
        std::make_tuple(
            "/images/resources/8bit.ico",
            AlphaTypeDiff("BMP-in-ICO: C++ reports kOpaque, Rust kUnpremul")),
        std::make_tuple(
            "/images/resources/32bit.ico",
            AlphaTypeDiff("BMP-in-ICO: C++ reports kOpaque, Rust kUnpremul")),
        // Single-image PNG-in-ICO.
        std::make_tuple("/images/resources/png-in-ico.ico", NoDifference()),
        // ICO whose AND (transparency) mask bitmap is omitted.
        std::make_tuple(
            "/images/resources/icon-without-and-bitmap.ico",
            AlphaTypeDiff("BMP-in-ICO: C++ reports kOpaque, Rust kUnpremul"))),
    ICOComparisonTestName);

}  // namespace blink
