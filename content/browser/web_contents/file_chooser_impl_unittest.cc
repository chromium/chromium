// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/file_chooser_impl.h"

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

class MockWebContentsDelegateForFileChooser : public WebContentsDelegate {
 public:
  // Mock the method to inspect the parameters it receives.
  MOCK_METHOD(void,
              RunFileChooser,
              (RenderFrameHost* render_frame_host,
               scoped_refptr<FileSelectListener> listener,
               const blink::mojom::FileChooserParams& params),
              (override));
};

class FileChooserImplTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    mock_web_contents_delegate_ =
        std::make_unique<MockWebContentsDelegateForFileChooser>();
    auto test_web_contents =
        TestWebContents::Create(browser_context(), nullptr);
    test_web_contents->SetDelegate(mock_web_contents_delegate_.get());
    SetContents(std::move(test_web_contents));

    // Navigate to page, otherwise OpenFileChooser() returns early.
    NavigateAndCommit(GURL(url::kAboutBlankURL));
  }

  void TearDown() override {
    mock_web_contents_delegate_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<MockWebContentsDelegateForFileChooser>
      mock_web_contents_delegate_;
};

TEST_F(FileChooserImplTest, DefaultFileNameClearedWhenModeIsNotSave) {
  FileChooserImpl* file_chooser_impl =
      FileChooserImpl::CreateForTesting(
          static_cast<RenderFrameHostImpl*>(main_rfh()))
          .first;

  auto params = blink::mojom::FileChooserParams::New();
  params->mode = blink::mojom::FileChooserParams::Mode::kOpen;
  const base::FilePath kInitialFile =
      base::FilePath(FILE_PATH_LITERAL("file.txt"));
  params->default_file_name = kInitialFile;

  blink::mojom::FileChooserParamsPtr captured_params;
  EXPECT_CALL(*mock_web_contents_delegate_, RunFileChooser(_, _, _))
      .WillOnce(
          [&](RenderFrameHost* rfh, scoped_refptr<FileSelectListener> listener,
              const blink::mojom::FileChooserParams& passed_params) {
            // Capture the arguments for later inspection.
            captured_params = passed_params.Clone();

            // Avoid logging error on destruction in test.
            static_cast<FileChooserImpl::FileSelectListenerImpl*>(
                listener.get())
                ->SetListenerFunctionCalledTrueForTesting();
          });

  file_chooser_impl->OpenFileChooser(std::move(params), base::DoNothing());

  // Verify the default file name was cleared.
  ASSERT_TRUE(captured_params);
  EXPECT_EQ(captured_params->default_file_name, base::FilePath());
}

TEST_F(FileChooserImplTest, DefaultFileNamePreservedWhenModeIsSave) {
  FileChooserImpl* file_chooser_impl =
      FileChooserImpl::CreateForTesting(
          static_cast<RenderFrameHostImpl*>(main_rfh()))
          .first;

  auto params = blink::mojom::FileChooserParams::New();
  params->mode = blink::mojom::FileChooserParams::Mode::kSave;
  const base::FilePath kInitialFile =
      base::FilePath(FILE_PATH_LITERAL("file.txt"));
  params->default_file_name = kInitialFile;

  blink::mojom::FileChooserParamsPtr captured_params;
  EXPECT_CALL(*mock_web_contents_delegate_, RunFileChooser(_, _, _))
      .WillOnce(
          [&](RenderFrameHost* rfh, scoped_refptr<FileSelectListener> listener,
              const blink::mojom::FileChooserParams& passed_params) {
            // Capture the arguments for later inspection.
            captured_params = passed_params.Clone();

            // Avoid logging error on destruction in test.
            static_cast<FileChooserImpl::FileSelectListenerImpl*>(
                listener.get())
                ->SetListenerFunctionCalledTrueForTesting();
          });

  file_chooser_impl->OpenFileChooser(std::move(params), base::DoNothing());

  // Verify the default file name was preserved.
  ASSERT_TRUE(captured_params);
  EXPECT_EQ(captured_params->default_file_name, kInitialFile);
}

}  // namespace content
