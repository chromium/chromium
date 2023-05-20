
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

void WaitForHistogram(const std::string& histogram_name) {
  // Need the polling of histogram because ScopedHistogramSampleObserver doesn't
  // support cross process metrics.
  while (!base::StatisticsRecorder::FindHistogram(histogram_name)) {
    content::FetchHistogramsFromChildProcesses();
    base::PlatformThread::Sleep(base::Milliseconds(5));
  }
}

}  // namespace

// Tests end to end functionality of "compression dictionary transport" feature.
class SharedDictionaryBrowserTest : public ContentBrowserTest {
 public:
  SharedDictionaryBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kCompressionDictionaryTransport,
                              blink::features::
                                  kCompressionDictionaryTransportBackend},
        /*disabled_features=*/{});
  }
  SharedDictionaryBrowserTest(const SharedDictionaryBrowserTest&) = delete;
  SharedDictionaryBrowserTest& operator=(const SharedDictionaryBrowserTest&) =
      delete;
  // ContentBrowserTest implementation:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  int64_t GetTestDataFileSize(const std::string& name) {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    int64_t file_size = 0;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      CHECK(base::GetFileSize(
          file_path.Append(GetTestDataFilePath()).AppendASCII(name),
          &file_size));
    }
    return file_size;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SharedDictionaryBrowserTest, LinkRelDictionary) {
  GURL url = embedded_test_server()->GetURL("/shared_dictionary/blank.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(), R"(
    (async ()=>{
      const link = document.createElement('link');
      link.rel = 'dictionary';
      link.href = './test.dict';
      document.body.appendChild(link);
    })();
  )"));

  WaitForHistogram("Net.SharedDictionaryWriterInMemory.DictionarySize");
  histogram_tester.ExpectBucketCount(
      "Net.SharedDictionaryWriterInMemory.DictionarySize",
      GetTestDataFileSize("shared_dictionary/test.dict"),
      /*expected_count=*/1);
}

}  // namespace content
