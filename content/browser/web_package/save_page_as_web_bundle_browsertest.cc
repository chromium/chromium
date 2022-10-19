// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom.h"

namespace content {
namespace {

const char kOnePageSimplePath[] =
    "/web_bundle/save_page_as_web_bundle/one_page_simple.html";
const char kOnePageWithImgPath[] =
    "/web_bundle/save_page_as_web_bundle/one_page_with_img.html";
const char kImgPngPath[] = "/web_bundle/save_page_as_web_bundle/img.png";

uint64_t GetResourceCount(
    mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle>& snapshot) {
  uint64_t count_out = 0;
  base::RunLoop run_loop;
  snapshot->GetResourceCount(
      base::BindLambdaForTesting([&run_loop, &count_out](uint64_t count) {
        count_out = count;
        run_loop.Quit();
      }));
  run_loop.Run();
  return count_out;
}

data_decoder::mojom::SerializedResourceInfoPtr GetResourceInfo(
    mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle>& snapshot,
    uint64_t index) {
  data_decoder::mojom::SerializedResourceInfoPtr info_out;
  base::RunLoop run_loop;
  snapshot->GetResourceInfo(
      index, base::BindLambdaForTesting(
                 [&run_loop, &info_out](
                     data_decoder::mojom::SerializedResourceInfoPtr info) {
                   info_out = std::move(info);
                   run_loop.Quit();
                 }));
  run_loop.Run();
  return info_out;
}

absl::optional<mojo_base::BigBuffer> GetResourceBody(
    mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle>& snapshot,
    uint64_t index) {
  absl::optional<mojo_base::BigBuffer> data_out;
  base::RunLoop run_loop;
  snapshot->GetResourceBody(
      index,
      base::BindLambdaForTesting(
          [&run_loop, &data_out](absl::optional<mojo_base::BigBuffer> data) {
            data_out = std::move(data);
            run_loop.Quit();
          }));
  run_loop.Run();
  return data_out;
}

class MockWebBundler : public data_decoder::mojom::WebBundler {
 public:
  MockWebBundler() = default;
  ~MockWebBundler() override {
    if (file_.IsValid()) {
      base::ScopedAllowBlockingForTesting allow_blocking;
      file_.Close();
    }
  }

  MockWebBundler(const MockWebBundler&) = delete;
  MockWebBundler& operator=(const MockWebBundler&) = delete;

  void Bind(mojo::PendingReceiver<data_decoder::mojom::WebBundler> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void WaitUntilGenerateCalled() {
    if (callback_)
      return;
    base::RunLoop loop;
    generate_called_callback_ = loop.QuitClosure();
    loop.Run();
  }

  void ResetReceiver() { receiver_.reset(); }

  size_t GetSnapshotSize() { return snapshots_.size(); }

 private:
  // mojom::WebBundleParserFactory implementation.
  void Generate(
      std::vector<mojo::PendingRemote<
          data_decoder::mojom::ResourceSnapshotForWebBundle>> snapshots,
      base::File file,
      GenerateCallback callback) override {
    DCHECK(!callback_);
    snapshots_ = std::move(snapshots);
    file_ = std::move(file);
    callback_ = std::move(callback);
    if (generate_called_callback_)
      std::move(generate_called_callback_).Run();
  }

  std::vector<
      mojo::PendingRemote<data_decoder::mojom::ResourceSnapshotForWebBundle>>
      snapshots_;
  base::File file_;
  GenerateCallback callback_;
  base::OnceClosure generate_called_callback_;

  mojo::Receiver<data_decoder::mojom::WebBundler> receiver_{this};
};

}  // namespace

class SavePageAsWebBundleBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle>
  NavigateAndGetSnapshot(const GURL& url) {
    NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
    mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle> snapshot;
    static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame())
        ->GetAssociatedLocalFrame()
        ->GetResourceSnapshotForWebBundle(
            snapshot.BindNewPipeAndPassReceiver());
    return snapshot;
  }

  bool CreateSaveDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return save_dir_.CreateUniqueTempDir();
  }

  std::tuple<uint64_t, data_decoder::mojom::WebBundlerError> GenerateWebBundle(
      const base::FilePath& file_path) {
    uint64_t ret_file_size = 0;
    data_decoder::mojom::WebBundlerError ret_error =
        data_decoder::mojom::WebBundlerError::kOK;
    base::RunLoop run_loop;
    shell()->web_contents()->GenerateWebBundle(
        file_path, base::BindLambdaForTesting(
                       [&run_loop, &ret_file_size, &ret_error](
                           uint64_t file_size,
                           data_decoder::mojom::WebBundlerError error) {
                         ret_file_size = file_size;
                         ret_error = error;
                         run_loop.Quit();
                       }));
    run_loop.Run();
    return std::make_tuple(ret_file_size, ret_error);
  }

  base::ScopedTempDir save_dir_;
};

IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleBrowserTest, SnapshotOnePageSimple) {
  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  auto snapshot = NavigateAndGetSnapshot(page_url);
  ASSERT_EQ(1u, GetResourceCount(snapshot));

  auto info = GetResourceInfo(snapshot, 0);
  ASSERT_TRUE(info);
  EXPECT_EQ(page_url, info->url);
  EXPECT_EQ("text/html", info->mime_type);
  EXPECT_GT(info->size, 0lu);

  auto data = GetResourceBody(snapshot, 0);
  ASSERT_TRUE(data);
  EXPECT_EQ(info->size, data->size());

  EXPECT_EQ(
      "<html>"
      "<head>"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=UTF-8\">\n"
      "<title>Hello</title>\n"
      "</head>"
      "<body><h1>hello world</h1>\n</body></html>",
      std::string(reinterpret_cast<const char*>(data->data()), data->size()));

  // GetResourceInfo() API with an out-of-range index should return null.
  EXPECT_TRUE(GetResourceInfo(snapshot, 1).is_null());
  // GetResourceBody() API with an out-of-range index should return nullopt.
  EXPECT_FALSE(GetResourceBody(snapshot, 1).has_value());
}

IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleBrowserTest, SnapshotOnePageWithImg) {
  const auto page_url = embedded_test_server()->GetURL(kOnePageWithImgPath);
  const auto img_url = embedded_test_server()->GetURL(kImgPngPath);
  auto snapshot = NavigateAndGetSnapshot(page_url);
  ASSERT_EQ(2u, GetResourceCount(snapshot));

  // The first item of resources must be the page, as FrameSerializer pushes the
  // SerializedResource of the html content in front of the deque of
  // SerializedResources.
  auto page_info = GetResourceInfo(snapshot, 0);
  ASSERT_TRUE(page_info);
  EXPECT_EQ(page_url, page_info->url);
  EXPECT_EQ("text/html", page_info->mime_type);
  EXPECT_GT(page_info->size, 0lu);

  auto img_info = GetResourceInfo(snapshot, 1u);
  ASSERT_TRUE(img_info);
  EXPECT_EQ(img_url, img_info->url);
  EXPECT_EQ("image/png", img_info->mime_type);
  EXPECT_GT(img_info->size, 0lu);

  auto page_data = GetResourceBody(snapshot, 0);
  ASSERT_TRUE(page_data);
  EXPECT_EQ(page_info->size, page_data->size());
  EXPECT_EQ(base::StringPrintf(
                "<html>"
                "<head>"
                "<meta http-equiv=\"Content-Type\" content=\"text/html; "
                "charset=UTF-8\">\n"
                "<title>Hello</title>\n"
                "</head>"
                "<body>"
                "<img src=\"%s\">\n"
                "<h1>hello world</h1>\n</body></html>",
                img_url.spec().c_str()),
            std::string(reinterpret_cast<const char*>(page_data->data()),
                        page_data->size()));

  std::string img_file_data;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath src_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    ASSERT_TRUE(base::ReadFileToString(
        src_dir.Append(GetTestDataFilePath())
            .Append(FILE_PATH_LITERAL(
                "web_bundle/save_page_as_web_bundle/img.png")),
        &img_file_data));
  }
  auto img_data = GetResourceBody(snapshot, 1);
  EXPECT_EQ(img_file_data,
            std::string(reinterpret_cast<const char*>(img_data->data()),
                        img_data->size()));
}

// TODO(crbug.com/1040752): Implement sub frames support and add tests.
// TODO(crbug.com/1040752): Implement style sheet support and add tests.

IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleBrowserTest,
                       GenerateOnePageSimpleWebBundle) {
  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);
  ASSERT_TRUE(CreateSaveDir());
  const auto file_path =
      save_dir_.GetPath().Append(FILE_PATH_LITERAL("test.wbn"));
  const auto result = GenerateWebBundle(file_path);
  EXPECT_GT(std::get<0>(result), 0lu);
  EXPECT_EQ(std::get<1>(result), data_decoder::mojom::WebBundlerError::kOK);
}

IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleBrowserTest,
                       GenerateWebBundleInvalidFilePath) {
  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);
  ASSERT_TRUE(CreateSaveDir());
  const auto file_path = save_dir_.GetPath();
  // Generating Web Bundle file using the existing directory path name must
  // fail with kFileOpenFailed error.
  EXPECT_EQ(
      std::make_tuple(0, data_decoder::mojom::WebBundlerError::kFileOpenFailed),
      GenerateWebBundle(file_path));
}

IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleBrowserTest,
                       GenerateWebBundleConnectionError) {
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  MockWebBundler mock_web_bundler;
  in_process_data_decoder.service().SetWebBundlerBinderForTesting(
      base::BindRepeating(&MockWebBundler::Bind,
                          base::Unretained(&mock_web_bundler)));

  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);
  ASSERT_TRUE(CreateSaveDir());
  const auto file_path =
      save_dir_.GetPath().Append(FILE_PATH_LITERAL("test.wbn"));
  uint64_t result_file_size = 0ul;
  data_decoder::mojom::WebBundlerError result_error =
      data_decoder::mojom::WebBundlerError::kOK;

  base::RunLoop run_loop;
  shell()->web_contents()->GenerateWebBundle(
      file_path,
      base::BindLambdaForTesting(
          [&run_loop, &result_file_size, &result_error](
              uint64_t file_size, data_decoder::mojom::WebBundlerError error) {
            result_file_size = file_size;
            result_error = error;
            run_loop.Quit();
          }));
  mock_web_bundler.WaitUntilGenerateCalled();
  mock_web_bundler.ResetReceiver();
  run_loop.Run();
  // When the connection to the WebBundler in the data decoder service is
  // disconnected, the result must be kWebBundlerConnectionError.
  EXPECT_EQ(0ULL, result_file_size);
  EXPECT_EQ(data_decoder::mojom::WebBundlerError::kWebBundlerConnectionError,
            result_error);
}

class SavePageAsWebBundleFencedFrameBrowserTest
    : public SavePageAsWebBundleBrowserTest {
 public:
  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 protected:
  test::FencedFrameTestHelper fenced_frame_helper_;
};

// If fenced frames become savable, this test will need to be updated.
// See https://crbug.com/1321102
IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleFencedFrameBrowserTest,
                       SnapshotOnePageWithFencedFrame) {
  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);

  // Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  mojo::Remote<data_decoder::mojom::ResourceSnapshotForWebBundle> snapshot;
  static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame())
      ->GetAssociatedLocalFrame()
      ->GetResourceSnapshotForWebBundle(snapshot.BindNewPipeAndPassReceiver());

  ASSERT_EQ(1u, GetResourceCount(snapshot));

  auto data = GetResourceBody(snapshot, 0);
  ASSERT_TRUE(data);

  EXPECT_EQ(
      "<html>"
      "<head>"
      "<meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=UTF-8\">\n"
      "<title>Hello</title>\n"
      "</head>"
      "<body><h1>hello world</h1>\n"
      "<fencedframe mode=\"default\"></fencedframe></body></html>",
      std::string(reinterpret_cast<const char*>(data->data()), data->size()));
}

// If fenced frames become savable, this test will need to be updated.
// See https://crbug.com/1321102
IN_PROC_BROWSER_TEST_F(SavePageAsWebBundleFencedFrameBrowserTest,
                       IgnoreSnapshotFencedFrameInWebBundle) {
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  MockWebBundler mock_web_bundler;
  in_process_data_decoder.service().SetWebBundlerBinderForTesting(
      base::BindRepeating(&MockWebBundler::Bind,
                          base::Unretained(&mock_web_bundler)));

  const auto page_url = embedded_test_server()->GetURL(kOnePageSimplePath);
  NavigateToURLBlockUntilNavigationsComplete(shell(), page_url, 1);

  // Create a fenced frame.
  GURL fenced_frame_url =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), fenced_frame_url);

  ASSERT_TRUE(CreateSaveDir());
  const auto file_path =
      save_dir_.GetPath().Append(FILE_PATH_LITERAL("test.wbn"));
  shell()->web_contents()->GenerateWebBundle(file_path, base::DoNothing());
  mock_web_bundler.WaitUntilGenerateCalled();

  // Verify the absence of the fenced frame's document.
  EXPECT_EQ(1lu, mock_web_bundler.GetSnapshotSize());
}

}  // namespace content
