// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/web_transport_simple_test_server.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file is placed tentively in content/browser/loader.
// TODO(yhirano): Convert tests in this file to web platform tests when they
// have a WebTransport server.

namespace content {
namespace {

using base::ASCIIToUTF16;

class WebTransportBrowserTest : public ContentBrowserTest {
 public:
  WebTransportBrowserTest() { server_.Start(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    server_.SetUpCommandLine(command_line);
  }

  bool WaitForTitle(const std::u16string& expected_title,
                    const std::vector<std::u16string> additional_titles) {
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    for (const auto& title : additional_titles) {
      title_watcher.AlsoWaitForTitle(title);
    }
    std::u16string actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    return expected_title == actual_title;
  }

  bool WaitForTitle(const std::u16string& title) {
    return WaitForTitle(title, {});
  }

 protected:
  quic::test::QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  WebTransportSimpleTestServer server_;
};

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, CertificateFingerprint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const hashValue = new Uint8Array(32);
      // The connection fails because the fingerprint does not match.
      const transport = new WebTransport(
          'https://localhost:%d/echo', {
            serverCertificateHashes: [
              {
                algorithm: "sha-256",
                value: hashValue,
              },
            ],
          });

      let fulfilled = false;
      try {
        await transport.ready;
        fulfilled = true
      } catch {}

      if (fulfilled) {
        throw Error('ready should be rejected');
      }

      try {
        await transport.closed;
      } catch (e) {
        return;
      }
      throw Error('closed should be rejected');
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

// A test that aims to reproduce https://crbug.com/1369030 -- note that since
// the bug in question is a race condition, this test will probably be flaky if
// this is actually broken.
IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, EchoLargeBidirectionalStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('https://localhost:%d/echo');
      await transport.ready;

      const numBytes = 1024 * 1024;
      const numStreams = 5;
      for (let i = 0; i < numStreams; i++) {
        const stream = await transport.createBidirectionalStream();
        const writer = stream.writable.getWriter();
        await writer.write(new Uint8Array(numBytes));
        await writer.close();
        let response = await (new Response(stream.readable).arrayBuffer());
        if (response.byteLength != numBytes) {
          throw Error('Size mismatch, received size: '
                         + response.byteLength.toString());
        }
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

}  // namespace
}  // namespace content
