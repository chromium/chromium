#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace LabColorSpace {

using blink::FloatPoint3D;

static constexpr FloatPoint3D rgbReferenceWhite =
    FloatPoint3D(1.0f, 1.0f, 1.0f);
static constexpr FloatPoint3D labReferenceWhite =
    FloatPoint3D(100.0f, 0.0f, 0.0f);
static constexpr float epsilon = 0.0001;

class LabColorSpaceTest : public testing::Test {
 public:
  void AssertColorsEqual(const FloatPoint3D& color1,
                         const FloatPoint3D& color2) {
    EXPECT_NEAR(color1.X(), color2.X(), epsilon);
    EXPECT_NEAR(color1.Y(), color2.Y(), epsilon);
    EXPECT_NEAR(color1.Z(), color2.Z(), epsilon);
  }
};

TEST_F(LabColorSpaceTest, XYZTranslation) {
  sRGBColorSpace colorSpace = sRGBColorSpace();

  // Check whether white transformation is correct
  FloatPoint3D xyzWhite = colorSpace.toXYZ(rgbReferenceWhite);
  AssertColorsEqual(xyzWhite, kIlluminantD50);

  FloatPoint3D rgbWhite = colorSpace.fromXYZ(kIlluminantD50);
  AssertColorsEqual(rgbWhite, rgbReferenceWhite);

  // Check whether transforming sRGB to XYZ and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        FloatPoint3D rgb = FloatPoint3D(r / 255.0f, g / 255.0f, b / 255.0f);
        FloatPoint3D xyz = colorSpace.toXYZ(rgb);
        AssertColorsEqual(rgb, colorSpace.fromXYZ(xyz));
      }
    }
  }
}

TEST_F(LabColorSpaceTest, LabTranslation) {
  RGBLABTransformer transformer = RGBLABTransformer();

  // Check whether white transformation is correct
  FloatPoint3D labWhite = transformer.sRGBToLab(rgbReferenceWhite);
  AssertColorsEqual(labWhite, labReferenceWhite);

  FloatPoint3D rgbWhite = transformer.LabToSRGB(labReferenceWhite);
  AssertColorsEqual(rgbWhite, rgbReferenceWhite);

  // Check whether transforming sRGB to Lab and back gives the same RGB values
  // for some random colors with different r, g, b components.
  for (unsigned r = 0; r <= 255; r += 40) {
    for (unsigned g = 0; r <= 255; r += 50) {
      for (unsigned b = 0; r <= 255; r += 60) {
        FloatPoint3D rgb = FloatPoint3D(r / 255.0f, g / 255.0f, b / 255.0f);
        FloatPoint3D lab = transformer.sRGBToLab(rgb);
        AssertColorsEqual(rgb, transformer.LabToSRGB(lab));
      }
    }
  }
}

}  // namespace LabColorSpace
