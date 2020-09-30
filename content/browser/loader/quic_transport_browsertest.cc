// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/ip_endpoint.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

// This file is placed tentively in content/browser/loader.
// TODO(yhirano): Convert tests in this file to web platform tests when they
// have a QuicTransport server.

namespace content {
namespace {

using base::ASCIIToUTF16;

class QuicTransportSimpleServerWithThread final {
 public:
  explicit QuicTransportSimpleServerWithThread(
      const std::vector<url::Origin>& origins)
      : origins_(origins) {}

  ~QuicTransportSimpleServerWithThread() {
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<net::QuicTransportSimpleServer> server) {},
            std::move(server_)));

    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_for_thread_join;
    io_thread_.reset();
  }

  void Start() {
    CHECK(!io_thread_);

    io_thread_ = std::make_unique<base::Thread>("QuicTransport server");
    base::Thread::Options thread_options;
    thread_options.message_pump_type = base::MessagePumpType::IO;
    CHECK(io_thread_->StartWithOptions(thread_options));
    CHECK(io_thread_->WaitUntilThreadStarted());

    base::WaitableEvent event;
    net::IPEndPoint server_address;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          server_ = std::make_unique<net::QuicTransportSimpleServer>(
              /*port=*/0, origins_,
              quic::test::crypto_test_utils::ProofSourceForTesting());
          const auto result = server_->Start();
          CHECK_EQ(EXIT_SUCCESS, result);
          server_address = server_->server_address();
          event.Signal();
        }));
    event.Wait();
    server_address_ = server_address;
  }

  const net::IPEndPoint& server_address() const { return server_address_; }

 private:
  const std::vector<url::Origin> origins_;
  net::IPEndPoint server_address_;

  std::unique_ptr<net::QuicTransportSimpleServer> server_;
  std::unique_ptr<base::Thread> io_thread_;
};

class QuicTransportBrowserTest : public ContentBrowserTest {
 public:
  QuicTransportBrowserTest() : server_({}) {
    quic::QuicEnableVersion(quic::DefaultVersionForQuicTransport());
    server_.Start();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(
        switches::kOriginToForceQuicOn,
        base::StringPrintf("localhost:%d", server_.server_address().port()));
    command_line->AppendSwitch(switches::kEnableQuic);
    command_line->AppendSwitchASCII(
        switches::kQuicVersion,
        quic::AlpnForVersion(quic::DefaultVersionForQuicTransport()));
    // The value is calculated from net/data/ssl/certificates/quic-chain.pem.
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList,
        "I+ryIVl5ksb8KijTneC3y7z1wBFn5x35O5is9g5n/KM=");
  }

  bool WaitForTitle(const base::string16& expected_title,
                    const std::vector<base::string16> additional_titles) {
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    for (const auto& title : additional_titles) {
      title_watcher.AlsoWaitForTitle(title);
    }
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
    return expected_title == actual_title;
  }

  bool WaitForTitle(const base::string16& title) {
    return WaitForTitle(title, {});
  }

 protected:
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  QuicTransportSimpleServerWithThread server_;
};

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, Echo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('quic-transport://localhost:%d/echo');

      const writer = transport.sendDatagrams().getWriter();
      const reader = transport.receiveDatagrams().getReader();

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, EchoViaWebTransport) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new WebTransport('quic-transport://localhost:%d/echo');

      const writer = transport.datagramWritable.getWriter();
      const reader = transport.datagramReadable.getReader();

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, ClientIndicationFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      // The client indication fails because there is no resource /X
      // on the server.
      const transport = new QuicTransport('quic-transport:localhost:%d/X');

      // Client indication is NOT part of handshake.
      await transport.ready;

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, CreateSendStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('quic-transport://localhost:%d/echo');

      await transport.ready;

      const sendStream = await transport.createSendStream();
      const writer = sendStream.writable.getWriter();
      await writer.write(new Uint8Array([65, 66, 67]));
      await writer.close();
    }

    run().then(() => { document.title = 'PASS'; },
               (e) => { console.log(e); document.title = 'FAIL'; });
)JS",
                                  server_.server_address().port())));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

// Flaky on many platforms (see crbug/1064434).
IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, DISABLED_ReceiveStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('quic-transport://localhost:%d/echo');

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, BidirectionalStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport('quic-transport://localhost:%d/echo');

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, CertificateFingerprint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      // The connection fails because the fingerprint does not match.
      const transport = new QuicTransport(
          'quic-transport://localhost:%d/echo', {
            serverCertificateFingerprints: [
              {
                algorithm: "sha-256",
                value: "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:" +
                       "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00",
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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

IN_PROC_BROWSER_TEST_F(QuicTransportBrowserTest, ReceiveBidirectionalStream) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("Title Of Awesomeness")));

  ASSERT_TRUE(ExecuteScript(
      shell(), base::StringPrintf(R"JS(
    async function run() {
      const transport = new QuicTransport(
        'quic-transport://localhost:%d/receive-bidirectional');

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

  ASSERT_TRUE(WaitForTitle(ASCIIToUTF16("PASS"), {ASCIIToUTF16("FAIL")}));
}

}  // namespace
}  // namespace content
