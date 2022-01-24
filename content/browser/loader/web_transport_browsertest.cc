// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
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
#include "net/third_party/quiche/src/quic/platform/api/quic_test.h"
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
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  WebTransportSimpleTestServer server_;
};

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, Echo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('https://localhost:%d/echo');

      const writer = transport.datagrams.writable.getWriter();
      const reader = transport.datagrams.readable.getReader();

      const data = new Uint8Array([65, 66, 67]);
      const id = setInterval(() => {
        writer.write(data);
      }, 10);

      const {done, value} = await reader.read();
      clearInterval(id);
      if (done) {
        throw Error('Got an unexpected DONE signal');
      }
      if (value.length !== 3 ||
          value[0] !== 65 || value[1] !== 66 || value[2] !== 67) {
        throw Error('Got invalid data');
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, EchoViaWebTransport) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('https://localhost:%d/echo');

      const writer = transport.datagrams.writable.getWriter();
      const reader = transport.datagrams.readable.getReader();

      const data = new Uint8Array([65, 66, 67]);
      const id = setInterval(() => {
        writer.write(data);
      }, 10);

      const {done, value} = await reader.read();
      clearInterval(id);
      if (done) {
        throw Error('Got an unexpected DONE signal');
      }
      if (value.length !== 3 ||
          value[0] !== 65 || value[1] !== 66 || value[2] !== 67) {
        throw Error('Got invalid data');
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, NonexistentResource) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      // The client indication fails because there is no resource /X
      // on the server.
      const transport = new WebTransport('https://localhost:%d/X');

      try {
        await transport.ready;
      } catch (e) {
        return;
      }
      throw Error('ready should be rejected');
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, CreateSendStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('https://localhost:%d/echo');

      await transport.ready;

      const sendStream = await transport.createUnidirectionalStream();
      const writer = sendStream.getWriter();
      await writer.write(new Uint8Array([65, 66, 67]));
      await writer.close();
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

// ReceiveStream is flaky: crbug.com/1140193
// TODO(vasilvv): change from QuicTransport to WebTransport when re-enabling.
#define MAYBE_ReceiveStream DISABLED_ReceiveStream
IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, MAYBE_ReceiveStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('https://localhost:%d/echo');

      await transport.ready;

      const data = [65, 66, 67];

      const sendStream = await transport.createSendStream();
      const writer = sendStream.writable.getWriter();
      await writer.write(new Uint8Array(data));
      await writer.close();

      const receiveStreamReader = transport.receiveStreams().getReader();
      const {value: receiveStream, done: streamsDone} =
          await receiveStreamReader.read();
      if (streamsDone) {
        throw new Error('should not be done');
      }
      const reader = receiveStream.readable.getReader();
      const {value: u8array, done: arraysDone} = await reader.read();
      if (arraysDone) {
        throw new Error('receiveStream should not be done');
      }
      const actual = Array.from(u8array);
      if (JSON.stringify(actual) !== JSON.stringify(data)) {
        throw new Error('arrays do not match');
      }
      const {done: finalDone} = await reader.read();
      if (!finalDone) {
        throw new Error('receiveStream should be done');
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

// This is flaky on all platforms. https://crbug.com/1254667
// TODO(ricea): Fix it and re-enable.
IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, BidirectionalStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('https://localhost:%d/echo');

      await transport.ready;

      const data = [65, 66, 67];

      const bidiStream = await transport.createBidirectionalStream();
      const writer = bidiStream.writable.getWriter();
      await writer.write(new Uint8Array(data));

      const reader = bidiStream.readable.getReader();

      const {value, done: done1} = await reader.read();
      if (done1) {
        throw new Error('reading should not be done');
      }
      const actual = Array.from(value);
      if (JSON.stringify(actual) !== JSON.stringify(data)) {
        throw new Error('arrays do not match');
      }

      await writer.close();

      const {done: done2} = await reader.read();
      if (!done2) {
        throw new Error('receiveStream should be done');
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

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

IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest, ReceiveBidirectionalStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport(
        'https://localhost:%d/echo');

      await transport.ready;

      const streams = transport.incomingBidirectionalStreams;
      const reader = streams.getReader();
      const {value, done} = await reader.read();
      if (done) {
        throw new Error('bidirectional streams should not be closed');
      }
      await testBidiStream(value);
    }

    async function testBidiStream(bidiStream) {
      // Consume the initial "hello" that is sent by the server.
      const writer = bidiStream.writable.getWriter();
      const reader = bidiStream.readable.getReader();
      await writer.write(new TextEncoder().encode('hello'));
      const {value: valueAsBinary, done: done0} = await reader.read();
      if (done0) {
        throw new Error('at least one read should happen');
      }
      const valueAsString = new TextDecoder().decode(valueAsBinary);
      if (valueAsString !== 'hello') {
        throw new Error(`expected 'hello', got '${valueAsString}'`);
      }
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                         server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(u"PASS", {u"FAIL"}));
}

// TODO(vasilvv): re-add /receive-bidirectional and re-enable the test.
IN_PROC_BROWSER_TEST_F(WebTransportBrowserTest,
                       DISABLED_ReceiveBidirectionalStreamOld) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(u"Title Of Awesomeness"));

  ASSERT_TRUE(
      ExecJs(shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport(
        'https://localhost:%d/receive-bidirectional');

      await transport.ready;

      // Trigger the server to create a bidirectional stream.
      const sendStream = await transport.createSendStream();
      // Need to actually write some data to inform the server.
      sendStream.writable.getWriter().write(new Uint8Array([1]));

      const streams = transport.receiveBidirectionalStreams();
      const reader = streams.getReader();
      const {value, done} = await reader.read();
      if (done) {
        throw new Error('bidirectional streams should not be closed');
      }
      await testBidiStream(value);
    }

    async function testBidiStream(bidiStream) {
      // Consume the initial "hello" that is sent by the server.
      const reader = bidiStream.readable.getReader();
      const {value: valueAsBinary, done: done0} = await reader.read();
      if (done0) {
        throw new Error('at least one read should happen');
      }
      const valueAsString = new TextDecoder().decode(valueAsBinary);
      if (valueAsString !== 'hello') {
        throw new Error(`expected 'hello', got '${valueAsString}'`);
      }

      const data = [65, 66, 67];

      const writer = bidiStream.writable.getWriter();
      await writer.write(new Uint8Array(data));

      const {value, done: done1} = await reader.read();
      if (done1) {
        throw new Error('reading should not be done');
      }
      const actual = Array.from(value);
      if (JSON.stringify(actual) !== JSON.stringify(data)) {
        throw new Error('arrays do not match');
      }

      await writer.close();

      const {done: done2} = await reader.read();
      if (!done2) {
        throw new Error('receiveStream should be done');
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
