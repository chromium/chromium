#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace LabColorSpace {

static constexpr SkV3 rgbReferenceWhite = {1.0f, 1.0f, 1.0f};
static constexpr SkV3 labReferenceWhite = {100.0f, 0.0f, 0.0f};
static constexpr float epsilon = 0.0001;

class LabColorSpaceTest : public testing::Test {
 public:
  void AssertColorsEqual(const SkV3& color1, const SkV3& color2) {
    EXPECT_NEAR(color1.x, color2.x, epsilon);
    EXPECT_NEAR(color1.y, color2.y, epsilon);
    EXPECT_NEAR(color1.z, color2.z, epsilon);
  }
};

TEST_F(LabColorSpaceTest, XYZTranslation) {
  sRGBColorSpace colorSpace = sRGBColorSpace();

  // Check whether white transformation is correct
  SkV3 xyzWhite = colorSpace.ToXYZ(rgbReferenceWhite);
  AssertColorsEqual(xyzWhite, kIlluminantD50);

  SkV3 rgbWhite = colorSpace.FromXYZ(kIlluminantD50);
  AssertColorsEqual(rgbWhite, rgbReferenceWhite);

  // Check whether transforming sRGB to XYZ and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        SkV3 rgb = {r / 255.0f, g / 255.0f, b / 255.0f};
        SkV3 xyz = colorSpace.ToXYZ(rgb);
        AssertColorsEqual(rgb, colorSpace.FromXYZ(xyz));
      }
    }
  }
}

TEST_F(LabColorSpaceTest, LabTranslation) {
  RGBLABTransformer transformer = RGBLABTransformer();

  // Check whether white transformation is correct
  SkV3 labWhite = transformer.sRGBToLab(rgbReferenceWhite);
  AssertColorsEqual(labWhite, labReferenceWhite);

  SkV3 rgbWhite = transformer.LabToSRGB(labReferenceWhite);
  AssertColorsEqual(rgbWhite, rgbReferenceWhite);

  // Check whether transforming sRGB to Lab and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        SkV3 rgb = {r / 255.0f, g / 255.0f, b / 255.0f};
        SkV3 lab = transformer.sRGBToLab(rgb);
        AssertColorsEqual(rgb, transformer.LabToSRGB(lab));
      }
    }
  }
}

}  // namespace LabColorSpace
