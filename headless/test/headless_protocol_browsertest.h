// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_
#define HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_browser_test.h"

namespace headless {

class HeadlessProtocolBrowserTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessDevToolsClient::RawProtocolListener,
      public runtime::ExperimentalObserver {
 public:
  HeadlessProtocolBrowserTest();

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  virtual std::vector<std::string> GetPageUrlExtraParams();

 private:
  // HeadlessWebContentsObserver implementation.
  void RunDevTooledTest() override;
  void BindingCreated(std::unique_ptr<headless::runtime::AddBindingResult>);

  // runtime::Observer implementation.
  void OnBindingCalled(const runtime::BindingCalledParams& params) override;

  // HeadlessDevToolsClient::RawProtocolListener
  bool OnProtocolMessage(base::span<const uint8_t> json_message,
                         const base::DictionaryValue& parsed_message) override;

  void SendMessageToJS(base::StringPiece message);
  void FinishTest();

 protected:
  bool test_finished_ = false;
  std::string test_folder_;
  std::string script_name_;
};

}  // namespace headless

#endif  // HEADLESS_TEST_HEADLESS_PROTOCOL_BROWSERTEST_H_
