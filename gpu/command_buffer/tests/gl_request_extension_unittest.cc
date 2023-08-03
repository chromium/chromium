// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>

#include "base/containers/contains.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class RequestExtensionCHROMIUMTest
    : public testing::TestWithParam<ContextType> {
 protected:
  void SetUp() override {
    GLManager::Options options;
    options.context_type = GetParam();
    gl_.Initialize(options);
  }
  void TearDown() override { gl_.Destroy(); }
  bool IsApplicable() const { return gl_.IsInitialized(); }
  GLManager gl_;
};

TEST_P(RequestExtensionCHROMIUMTest, Basic) {
  if (!IsApplicable()) {
    return;
  }
  // Test to test that requesting an extension does not delete any extensions.
  // Also tests that WebGL2 and GLES3 contexts keep glGetString(GL_EXTENSIONS)
  // and glGetStringi(GL_EXTENSIONS, index) in sync.

  std::string requestable_extensions_string =
      reinterpret_cast<const char*>(glGetRequestableExtensionsCHROMIUM());
  std::vector<std::string> requestable =
      base::SplitString(requestable_extensions_string, base::kWhitespaceASCII,
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& to_request : requestable) {
    std::string extension_string =
        reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    extension_string += " ";
    size_t extensions_size_before_request =
        base::SplitString(extension_string, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)
            .size();

    if (base::Contains(extension_string, to_request + " ")) {
      // Somewhat counterintuitively, requestable extensions contain every
      // extension available.
      continue;
    }

    glRequestExtensionCHROMIUM(to_request.c_str());

    extension_string =
        reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    std::vector<std::string> extensions =
        base::SplitString(extension_string, base::kWhitespaceASCII,
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    std::set<std::string> extensions_from_string(extensions.begin(),
                                                 extensions.end());

    if (GetParam() == CONTEXT_TYPE_WEBGL2 ||
        GetParam() == CONTEXT_TYPE_OPENGLES3) {
      // Test that GetString(GL_EXTENSIONS) is consistent with
      // GetStringi(GL_EXTENSIONS, index)
      GLint num_extensions = 0;
      glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
      EXPECT_EQ(static_cast<size_t>(num_extensions), extensions.size());
      std::set<std::string> extensions_from_stringi;
      for (int i = 0; i < num_extensions; ++i) {
        extensions_from_stringi.insert(
            reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)));
      }
      EXPECT_EQ(extensions_from_string, extensions_from_stringi);
    }

    // Somewhat counterintuitively, requesting an extension that is offered
    // does not necessarily make it appear in the extensions list.
    // This is due to the fact that offered extensions list contains every
    // available extension, while WebGL2 filters extension list
    // based on what's in core WebGL2.

    // Test that RequestExtension does not erase any extensions.
    EXPECT_GE(extensions.size(), extensions_size_before_request);
  }
}

INSTANTIATE_TEST_SUITE_P(WithContextTypes,
                         RequestExtensionCHROMIUMTest,
                         ::testing::Values(CONTEXT_TYPE_WEBGL1,
                                           CONTEXT_TYPE_WEBGL2,
                                           CONTEXT_TYPE_OPENGLES2,
                                           CONTEXT_TYPE_OPENGLES3));
}
