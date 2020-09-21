#include "third_party/blink/renderer/platform/graphics/dark_mode_lab_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace lab {

static constexpr SkV3 kSRGBReferenceWhite = {1.0f, 1.0f, 1.0f};
static constexpr SkV3 kLABReferenceWhite = {100.0f, 0.0f, 0.0f};
static constexpr float kEpsilon = 0.0001;

class DarkModeLABColorSpaceTest : public testing::Test {
 public:
  void AssertColorsEqual(const SkV3& color1, const SkV3& color2) {
    EXPECT_NEAR(color1.x, color2.x, kEpsilon);
    EXPECT_NEAR(color1.y, color2.y, kEpsilon);
    EXPECT_NEAR(color1.z, color2.z, kEpsilon);
  }
};

TEST_F(DarkModeLABColorSpaceTest, XYZTranslation) {
  DarkModeSRGBColorSpace color_space = DarkModeSRGBColorSpace();

  // Check whether white transformation is correct.
  SkV3 xyz_white = color_space.ToXYZ(kSRGBReferenceWhite);
  AssertColorsEqual(xyz_white, kIlluminantD50);

  SkV3 rgb_white = color_space.FromXYZ(kIlluminantD50);
  AssertColorsEqual(rgb_white, kSRGBReferenceWhite);

  // Check whether transforming sRGB to XYZ and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        SkV3 rgb = {r / 255.0f, g / 255.0f, b / 255.0f};
        SkV3 xyz = color_space.ToXYZ(rgb);
        AssertColorsEqual(rgb, color_space.FromXYZ(xyz));
      }
    }
  }
}

TEST_F(DarkModeLABColorSpaceTest, LABTranslation) {
  DarkModeSRGBLABTransformer transformer = DarkModeSRGBLABTransformer();

  // Check whether white transformation is correct.
  SkV3 lab_white = transformer.SRGBToLAB(kSRGBReferenceWhite);
  AssertColorsEqual(lab_white, kLABReferenceWhite);

  SkV3 rgb_white = transformer.LABToSRGB(kLABReferenceWhite);
  AssertColorsEqual(rgb_white, kSRGBReferenceWhite);

  // Check whether transforming sRGB to Lab and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        SkV3 rgb = {r / 255.0f, g / 255.0f, b / 255.0f};
        SkV3 lab = transformer.SRGBToLAB(rgb);
        AssertColorsEqual(rgb, transformer.LABToSRGB(lab));
      }
    }
  }
}

}  // namespace lab

}  // namespace blink
