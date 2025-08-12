// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_tokenizer.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "ui/gl/gl_switches.h"

using base::CommandLine;

namespace content {

namespace {

const char kShapeDetectionTestHtml[] = "/media/shape_detection_test.html";

struct TestParameters {
  const std::string detector_name;
  const std::string image_path;
  const std::vector<std::vector<float>> expected_bounding_boxes;
} const kTestParameters[] = {
    {"FaceDetector", "/blank.jpg", {}},
    {"FaceDetector",
     "/single_face.jpg",
#if BUILDFLAG(IS_ANDROID)
     {{23, 20, 42, 42}}
#else
     {{23, 26, 42, 42}}
#endif
    },
};

std::ostream& operator<<(std::ostream& out,
                         const struct TestParameters& parameters) {
  out << parameters.detector_name << " running on: " << parameters.image_path;
  return out;
}

}  // namespace

// This class contains content_browsertests for Shape Detection API, which
// detect features (Faces, QR/Barcodes, etc) in still or moving images.
class ShapeDetectionBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<struct TestParameters> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable FaceDetector since it is still experimental.
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "FaceDetector");
  }

 protected:
  void RunDetectShapesOnImage(
      const std::string& detector_name,
      const std::string& image_path,
      const std::vector<std::vector<float>>& expected_bounding_boxes) {
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL html_url(
        embedded_test_server()->GetURL(kShapeDetectionTestHtml));
    const GURL image_url(embedded_test_server()->GetURL(image_path));
    EXPECT_TRUE(NavigateToURL(shell(), html_url));
    const std::string js_command = "detectShapesOnImageUrl('" + detector_name +
                                   "', '" + image_url.spec() + "')";
    std::string response_string = EvalJs(shell(), js_command).ExtractString();

    base::StringTokenizer outer_tokenizer(response_string, "#");
    std::vector<std::vector<float>> detected_bounding_boxes;
    while (outer_tokenizer.GetNext()) {
      std::string s = outer_tokenizer.token().c_str();
      std::vector<float> bounding_box;
      base::StringTokenizer inner_tokenizer(s, ",");
      while (inner_tokenizer.GetNext())
        bounding_box.push_back(atof(inner_tokenizer.token().c_str()));
      detected_bounding_boxes.push_back(bounding_box);
    }

    ASSERT_EQ(expected_bounding_boxes.size(), detected_bounding_boxes.size());
    for (size_t shape_id = 0; shape_id < detected_bounding_boxes.size();
         ++shape_id) {
      const auto expected_bounding_box = expected_bounding_boxes[shape_id];
      const auto detected_bounding_box = detected_bounding_boxes[shape_id];
      for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(expected_bounding_box[i], detected_bounding_box[i], 2)
            << ", index " << i;
      }
    }
  }
};

// TODO(crbug.com/41282827): Enable the test on other platforms.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC)
#define MAYBE_DetectShapesInImage DetectShapesInImage
#else
#define MAYBE_DetectShapesInImage DISABLED_DetectShapesInImage
#endif

IN_PROC_BROWSER_TEST_P(ShapeDetectionBrowserTest, MAYBE_DetectShapesInImage) {
  // Face detection needs GPU infrastructure.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseGpuInTests))
    return;

  RunDetectShapesOnImage(GetParam().detector_name, GetParam().image_path,
                         GetParam().expected_bounding_boxes);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ShapeDetectionBrowserTest,
                         testing::ValuesIn(kTestParameters));

}  // namespace content
