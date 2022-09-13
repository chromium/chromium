// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "fuchsia_web/webengine/test/frame_for_test.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace {

// Value returned by echoheader if header is not present in the request.
constexpr const char kHeaderNotPresent[] = "None";

// Client Hint header names defined by the spec.
constexpr const char kRoundTripTimeCH[] = "rtt";
constexpr const char kDeviceMemoryCH[] = "sec-ch-device-memory";
constexpr const char kUserAgentCH[] = "sec-ch-ua";
constexpr const char kFullVersionListCH[] = "sec-ch-ua-full-version-list";
constexpr const char kArchCH[] = "sec-ch-ua-arch";
constexpr const char kBitnessCH[] = "sec-ch-ua-bitness";
constexpr const char kPlatformCH[] = "sec-ch-ua-platform";

// |str| is interpreted as a decimal number or integer.
void ExpectStringIsNonNegativeNumber(std::string& str) {
  double str_double;
  EXPECT_TRUE(base::StringToDouble(str, &str_double));
  EXPECT_GE(str_double, 0);
}

}  // namespace

// TODO(crbug.com/1356277): Client Hints temporarily disabled as it is causing
// several apps to fail. Re-enable Client Hints tests after breakage is fixed.
class DISABLED_ClientHintsTest : public FrameImplTestBaseWithServer {
 public:
  DISABLED_ClientHintsTest() = default;
  ~DISABLED_ClientHintsTest() override = default;
  DISABLED_ClientHintsTest(const DISABLED_ClientHintsTest&) = delete;
  DISABLED_ClientHintsTest& operator=(const DISABLED_ClientHintsTest&) = delete;

  void SetUpOnMainThread() override {
    FrameImplTestBaseWithServer::SetUpOnMainThread();
    frame_for_test_ =
        FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
  }

 protected:
  // Sets Client Hints for embedded test server to request from the content
  // embedder. Sends "Accept-CH" response header with |hint_types|, a
  // comma-separated list of Client Hint types.
  void SetClientHintsForTestServerToRequest(const std::string& hint_types) {
    GURL url = embedded_test_server()->GetURL(
        std::string("/set-header?Accept-CH: ") + hint_types);
    LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                             url.spec());
    frame_for_test_.navigation_listener().RunUntilUrlEquals(url);
  }

  // Gets the value of |header| returned by WebEngine on a navigation.
  // Loads "/echoheader" which echoes the given |header|. The server responds to
  // this navigation request with the header value. Returns the header value,
  // which is read by JavaScript. Returns kHeaderNotPresent if header was not
  // sent during the request.
  std::string GetNavRequestHeaderValue(const std::string& header) {
    GURL url =
        embedded_test_server()->GetURL(std::string("/echoheader?") + header);
    LoadUrlAndExpectResponse(frame_for_test_.GetNavigationController(), {},
                             url.spec());
    frame_for_test_.navigation_listener().RunUntilUrlEquals(url);

    absl::optional<base::Value> value =
        ExecuteJavaScript(frame_for_test_.get(), "document.body.innerText;");
    return value->GetString();
  }

  // Gets the value of |header| returned by WebEngine on a XMLHttpRequest.
  // Loads "/echoheader" which echoes the given |header|. The server responds to
  // the XMLHttpRequest with the header value, which is saved in a JavaScript
  // Promise. Returns the value of Promise, and returns kHeaderNotPresent if
  // header is not sent during the request. Requires a loaded page first. Call
  // TestServerRequestsClientHints or GetNavRequestHeaderValue first to have a
  // loaded page.
  std::string GetXHRRequestHeaderValue(const std::string& header) {
    constexpr char kScript[] = R"(
      new Promise(function (resolve, reject) {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", "/echoheader?" + $1);
        xhr.onload = () => {
          resolve(xhr.response);
        };
        xhr.send();
      })
    )";
    FrameImpl* frame_impl =
        context_impl()->GetFrameImplForTest(&frame_for_test_.ptr());
    content::WebContents* web_contents = frame_impl->web_contents_for_test();
    return content::EvalJs(web_contents, content::JsReplace(kScript, header))
        .ExtractString();
  }

  // Fetches value of Client Hint |hint_type| for both navigation and
  // subresource requests, and calls |verify_callback| with the value.
  void GetAndVerifyClientHint(
      const std::string& hint_type,
      base::RepeatingCallback<void(std::string&)> verify_callback) {
    std::string result = GetNavRequestHeaderValue(hint_type);
    verify_callback.Run(result);
    result = GetXHRRequestHeaderValue(hint_type);
    verify_callback.Run(result);
  }

  FrameForTest frame_for_test_;
};

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, NumericalClientHints) {
  SetClientHintsForTestServerToRequest(std::string(kRoundTripTimeCH) + "," +
                                       std::string(kDeviceMemoryCH));
  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, InvalidClientHint) {
  // Check browser handles requests for an invalid Client Hint.
  SetClientHintsForTestServerToRequest("not-a-client-hint");
  GetAndVerifyClientHint("not-a-client-hint",
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

// Low-entropy User Agent Client Hints are sent by default without the origin
// needing to request them. For a list of low-entropy Client Hints, see
// https://wicg.github.io/client-hints-infrastructure/#low-entropy-hint-table/
IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest,
                       LowEntropyClientHintsAreSentByDefault) {
  GetAndVerifyClientHint(
      kUserAgentCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(str.find("Chromium") != std::string::npos);
        EXPECT_TRUE(str.find(version_info::GetMajorVersionNumber()) !=
                    std::string::npos);
      }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest,
                       LowEntropyClientHintsAreSentWhenRequested) {
  SetClientHintsForTestServerToRequest(kUserAgentCH);
  GetAndVerifyClientHint(
      kUserAgentCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(str.find("Chromium") != std::string::npos);
        EXPECT_TRUE(str.find(version_info::GetMajorVersionNumber()) !=
                    std::string::npos);
      }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest,
                       HighEntropyClientHintsAreNotSentByDefault) {
  GetAndVerifyClientHint(kFullVersionListCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest,
                       HighEntropyClientHintsAreSentWhenRequested) {
  SetClientHintsForTestServerToRequest(kFullVersionListCH);
  GetAndVerifyClientHint(
      kFullVersionListCH, base::BindRepeating([](std::string& str) {
        EXPECT_TRUE(str.find("Chromium") != std::string::npos);
        EXPECT_TRUE(str.find(version_info::GetVersionNumber()) !=
                    std::string::npos);
      }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, ArchitectureIsArmOrX86) {
  SetClientHintsForTestServerToRequest(kArchCH);
  GetAndVerifyClientHint(kArchCH, base::BindRepeating([](std::string& str) {
#if defined(ARCH_CPU_X86_64)
                           EXPECT_EQ(str, "\"x86\"");
#elif defined(ARCH_CPU_ARM64)
                           EXPECT_EQ(str, "\"arm\"");
#else
#error Unsupported CPU architecture
#endif
                         }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, BitnessIs64) {
  SetClientHintsForTestServerToRequest(kBitnessCH);
  GetAndVerifyClientHint(kBitnessCH, base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, "\"64\"");
                         }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, PlatformIsFuchsia) {
  // Platform is a low-entropy Client Hint, so no need for test server to
  // request it.
  GetAndVerifyClientHint(kPlatformCH, base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, "\"Fuchsia\"");
                         }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest, RemoveClientHint) {
  SetClientHintsForTestServerToRequest(std::string(kRoundTripTimeCH) + "," +
                                       std::string(kDeviceMemoryCH));
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Remove device memory from list of requested Client Hints. Removed hints
  // should no longer be sent.
  SetClientHintsForTestServerToRequest(kRoundTripTimeCH);
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}

IN_PROC_BROWSER_TEST_F(DISABLED_ClientHintsTest,
                       AdditionalClientHintsAreAlwaysSent) {
  SetClientHintsForTestServerToRequest(kRoundTripTimeCH);

  // Enable device memory as an additional Client Hint.
  auto* client_hints_delegate =
      context_impl()->browser_context()->GetClientHintsControllerDelegate();
  client_hints_delegate->SetAdditionalClientHints(
      {network::mojom::WebClientHintsType::kDeviceMemory});

  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // The additional Client Hint is sent without needing to be requested.
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Remove all additional Client Hints.
  client_hints_delegate->ClearAdditionalClientHints();

  // Request a different URL because the frame would not reload the page with
  // the same URL.
  GetAndVerifyClientHint(kRoundTripTimeCH,
                         base::BindRepeating(&ExpectStringIsNonNegativeNumber));

  // Removed additional Client Hint is no longer sent.
  GetAndVerifyClientHint(kDeviceMemoryCH,
                         base::BindRepeating([](std::string& str) {
                           EXPECT_EQ(str, kHeaderNotPresent);
                         }));
}
