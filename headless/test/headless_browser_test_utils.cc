// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test_utils.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

using simple_devtools_protocol_client::SimpleDevToolsProtocolClient;

namespace headless {

base::Value::Dict SendCommandSync(SimpleDevToolsProtocolClient& devtools_client,
                                  const std::string& command) {
  return SendCommandSync(devtools_client, command, base::Value::Dict());
}

base::Value::Dict SendCommandSync(
    simple_devtools_protocol_client::SimpleDevToolsProtocolClient&
        devtools_client,
    const std::string& command,
    base::Value::Dict params) {
  base::Value::Dict command_result;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  devtools_client.SendCommand(
      command, std::move(params),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::Value::Dict* command_result,
             base::Value::Dict result) {
            *command_result = std::move(result);
            run_loop->Quit();
          },
          base::Unretained(&run_loop), base::Unretained(&command_result)));
  run_loop.Run();

  return command_result;
}

base::Value::Dict EvaluateScript(HeadlessWebContents* web_contents,
                                 const std::string& script) {
  SimpleDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(
      HeadlessWebContentsImpl::From(web_contents)->web_contents());

  base::Value::Dict result = SendCommandSync(
      devtools_client, "Runtime.evaluate", Param("expression", script));

  devtools_client.DetachClient();

  return result;
}

bool WaitForLoad(HeadlessWebContents* web_contents, net::Error* error) {
  content::WebContents* content_web_contents =
      HeadlessWebContentsImpl::From(web_contents)->web_contents();

  content::TestNavigationObserver observer(content_web_contents, 1);
  observer.Wait();

  if (error)
    *error = observer.last_net_error_code();

  return observer.last_navigation_succeeded();
}

void WaitForLoadAndGainFocus(HeadlessWebContents* web_contents) {
  content::WebContents* content_web_contents =
      HeadlessWebContentsImpl::From(web_contents)->web_contents();

  // To finish loading and to gain focus are two independent events. Which one
  // is issued first is undefined. The following code is waiting on both, in any
  // order.
  content::TestNavigationObserver load_observer(content_web_contents, 1);
  content::FrameFocusedObserver focus_observer(
      content_web_contents->GetPrimaryMainFrame());
  load_observer.Wait();
  focus_observer.Wait();
}

///////////////////////////////////////////////////////////////////////
// base::Value::Dict helpers.

std::string DictString(const base::Value::Dict& dict, std::string_view path) {
  const std::string* result = dict.FindStringByDottedPath(path);
  CHECK(result) << "Missing value for '" << path << "' in:\n"
                << dict.DebugString();
  return *result;
}

int DictInt(const base::Value::Dict& dict, std::string_view path) {
  std::optional<int> result = dict.FindIntByDottedPath(path);
  CHECK(result) << "Missing value for '" << path << "' in:\n"
                << dict.DebugString();
  return *result;
}

bool DictBool(const base::Value::Dict& dict, std::string_view path) {
  std::optional<bool> result = dict.FindBoolByDottedPath(path);
  CHECK(result) << "Missing value for '" << path << "' in:\n"
                << dict.DebugString();
  return *result;
}

bool DictHas(const base::Value::Dict& dict, std::string_view path) {
  return dict.FindByDottedPath(path) != nullptr;
}

///////////////////////////////////////////////////////////////////////
// GMock matchers

namespace {
// Cannot use Value::DebugString here due to newlines.
std::string ToJSON(const base::ValueView& value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

class DictHasPathValueMatcher
    : public testing::MatcherInterface<const base::Value::Dict&> {
 public:
  DictHasPathValueMatcher(const std::string& path, base::Value expected_value)
      : path_(path), expected_value_(std::move(expected_value)) {}

  DictHasPathValueMatcher& operator=(const DictHasPathValueMatcher& other) =
      delete;

  ~DictHasPathValueMatcher() override = default;

  bool MatchAndExplain(const base::Value::Dict& dict,
                       testing::MatchResultListener* listener) const override {
    const base::Value* dict_value = dict.FindByDottedPath(path_);
    if (!dict_value) {
      *listener << "Dictionary '" << ToJSON(dict) << "' does not have path '"
                << path_ << "'";
      return false;
    }
    if (*dict_value != expected_value_) {
      *listener << "Dictionary path value '" << path_ << "' is '"
                << ToJSON(*dict_value) << "', expected '"
                << ToJSON(expected_value_) << "'";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "has path '" << path_ << "' with value '" << ToJSON(expected_value_)
        << "'";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not have path '" << path_ << "' with value '"
        << ToJSON(expected_value_) << "'";
  }

 private:
  const std::string path_;
  const base::Value expected_value_;
};

class DictHasKeyMatcher
    : public testing::MatcherInterface<const base::Value::Dict&> {
 public:
  explicit DictHasKeyMatcher(const std::string& key) : key_(key) {}

  DictHasKeyMatcher& operator=(const DictHasKeyMatcher& other) = delete;

  ~DictHasKeyMatcher() override = default;

  bool MatchAndExplain(const base::Value::Dict& dict,
                       testing::MatchResultListener* listener) const override {
    const base::Value* dict_value = dict.Find(key_);
    if (!dict_value) {
      *listener << "Dictionary '" << ToJSON(dict) << "' does not have key '"
                << key_ << "'";
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "has key '" << key_ << "'";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not have key '" << key_ << "'";
  }

 private:
  const std::string key_;
};

}  // namespace

testing::Matcher<const base::Value::Dict&> DictHasPathValue(
    const std::string& path,
    base::Value expected_value) {
  return testing::MakeMatcher(
      new DictHasPathValueMatcher(path, std::move(expected_value)));
}

testing::Matcher<const base::Value::Dict&> DictHasKey(const std::string& key) {
  return testing::MakeMatcher(new DictHasKeyMatcher(key));
}

}  // namespace headless
