// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test_utils.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "components/devtools/simple_devtools_protocol_client/simple_devtools_protocol_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/headless_web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

namespace {

std::string GetString(const base::Value::Dict& dict,
                      base::StringPiece root,
                      base::StringPiece key) {
  const std::string path = base::StrCat({root, key});
  const std::string* return_string = dict.FindStringByDottedPath(path);
  CHECK(return_string) << "Missing value for '" << path << "' in:\n"
                       << dict.DebugString();
  return *return_string;
}

int GetInt(const base::Value::Dict& dict,
           base::StringPiece root,
           base::StringPiece key) {
  const std::string path = base::StrCat({root, key});
  absl::optional<int> return_int = dict.FindIntByDottedPath(path);
  CHECK(return_int) << "Missing value for '" << path << "' in:\n"
                    << dict.DebugString();
  return *return_int;
}

bool GetBool(const base::Value::Dict& dict,
             base::StringPiece root,
             base::StringPiece key) {
  const std::string path = base::StrCat({root, key});
  absl::optional<bool> return_bool = dict.FindBoolByDottedPath(path);
  CHECK(return_bool) << "Missing value for '" << path << "' in:\n"
                     << dict.DebugString();
  return *return_bool;
}

bool Has(const base::Value::Dict& dict,
         base::StringPiece root,
         base::StringPiece key) {
  const std::string path = base::StrCat({root, key});
  return dict.FindByDottedPath(path) != nullptr;
}

}  // namespace

bool ResultError(const base::Value::Dict& result,
                 int* code,
                 std::string* message) {
  absl::optional<int> error_code = result.FindIntByDottedPath("error.code");
  if (code && error_code)
    *code = *error_code;

  const std::string* error_message =
      result.FindStringByDottedPath("error.message");
  if (message && error_message)
    *message = *error_message;

  return error_code || error_message;
}

std::string ResultString(const base::Value::Dict& result,
                         base::StringPiece key) {
  return GetString(result, "result.", key);
}

int ResultInt(const base::Value::Dict& result, base::StringPiece key) {
  return GetInt(result, "result.", key);
}

bool ResultBool(const base::Value::Dict& result, base::StringPiece key) {
  return GetBool(result, "result.", key);
}

bool ResultHas(const base::Value::Dict& result, base::StringPiece key) {
  return Has(result, "result.", key);
}

std::string ParamsString(const base::Value::Dict& params,
                         base::StringPiece key) {
  return GetString(params, "params.", key);
}

int ParamsInt(const base::Value::Dict& params, base::StringPiece key) {
  return GetInt(params, "params.", key);
}

bool ParamsBool(const base::Value::Dict& params, base::StringPiece key) {
  return GetBool(params, "params.", key);
}

bool ParamHas(const base::Value::Dict& result, base::StringPiece key) {
  return Has(result, "params.", key);
}

}  // namespace headless
