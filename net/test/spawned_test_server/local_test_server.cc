// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/local_test_server.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/test/python_utils.h"
#include "url/gurl.h"

namespace net {

namespace {

bool AppendArgumentFromJSONValue(const std::string& key,
                                 const base::Value& value_node,
                                 base::CommandLine* command_line) {
  std::string argument_name = "--" + key;
  switch (value_node.type()) {
    case base::Value::Type::NONE:
      command_line->AppendArg(argument_name);
      break;
    case base::Value::Type::INTEGER: {
      command_line->AppendArg(argument_name + "=" +
                              base::NumberToString(value_node.GetInt()));
      break;
    }
    case base::Value::Type::STRING: {
      if (!value_node.is_string())
        return false;
      const std::string value = value_node.GetString();
      if (value.empty())
        return false;
      command_line->AppendArg(argument_name + "=" + value);
      break;
    }
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::LIST:
    case base::Value::Type::DICT:
    case base::Value::Type::BINARY:
    default:
      NOTREACHED_IN_MIGRATION() << "improper json type";
      return false;
  }
  return true;
}

}  // namespace

LocalTestServer::LocalTestServer(Type type, const base::FilePath& document_root)
    : BaseTestServer(type) {
  if (!Init(document_root))
    NOTREACHED_IN_MIGRATION();
}

LocalTestServer::LocalTestServer(Type type,
                                 const SSLOptions& ssl_options,
                                 const base::FilePath& document_root)
    : BaseTestServer(type, ssl_options) {
  if (!Init(document_root))
    NOTREACHED_IN_MIGRATION();
}

LocalTestServer::~LocalTestServer() {
  Stop();
}

bool LocalTestServer::GetTestServerPath(base::FilePath* testserver_path) const {
  base::FilePath testserver_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &testserver_dir)) {
    LOG(ERROR) << "Failed to get DIR_SRC_TEST_DATA_ROOT";
    return false;
  }
  testserver_dir = testserver_dir.Append(FILE_PATH_LITERAL("net"))
                       .Append(FILE_PATH_LITERAL("tools"))
                       .Append(FILE_PATH_LITERAL("testserver"));
  *testserver_path = testserver_dir.Append(FILE_PATH_LITERAL("testserver.py"));
  return true;
}

bool LocalTestServer::StartInBackground() {
  DCHECK(!started());

  base::ScopedAllowBlockingForTesting allow_blocking;

  // Get path to Python server script.
  base::FilePath testserver_path;
  if (!GetTestServerPath(&testserver_path)) {
    LOG(ERROR) << "Could not get test server path.";
    return false;
  }

  std::optional<std::vector<base::FilePath>> python_path = GetPythonPath();
  if (!python_path) {
    LOG(ERROR) << "Could not get Python path.";
    return false;
  }

  if (!LaunchPython(testserver_path, *python_path)) {
    LOG(ERROR) << "Could not launch Python with path " << testserver_path;
    return false;
  }

  return true;
}

bool LocalTestServer::BlockUntilStarted() {
  if (!WaitToStart()) {
    Stop();
    return false;
  }

  return SetupWhenServerStarted();
}

bool LocalTestServer::Stop() {
  CleanUpWhenStoppingServer();

  if (!process_.IsValid())
    return true;

  // First check if the process has already terminated.
  bool ret = process_.WaitForExitWithTimeout(base::TimeDelta(), nullptr);
  if (!ret) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait_process;
    ret = process_.Terminate(1, true);
  }

  if (ret)
    process_.Close();
  else
    VLOG(1) << "Kill failed?";

  return ret;
}

bool LocalTestServer::Init(const base::FilePath& document_root) {
  if (document_root.IsAbsolute())
    return false;

  // At this point, the port that the test server will listen on is unknown.
  // The test server will listen on an ephemeral port, and write the port
  // number out over a pipe that this TestServer object will read from. Once
  // that is complete, the host port pair will contain the actual port.
  DCHECK(!GetPort());

  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir)) {
    return false;
  }
  SetResourcePath(src_dir.Append(document_root),
                  src_dir.AppendASCII("net")
                      .AppendASCII("data")
                      .AppendASCII("ssl")
                      .AppendASCII("certificates"));
  return true;
}

std::optional<std::vector<base::FilePath>> LocalTestServer::GetPythonPath()
    const {
  base::FilePath third_party_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &third_party_dir)) {
    LOG(ERROR) << "Failed to get DIR_SRC_TEST_DATA_ROOT";
    return std::nullopt;
  }
  third_party_dir = third_party_dir.AppendASCII("third_party");

  std::vector<base::FilePath> ret = {
      third_party_dir.AppendASCII("pywebsocket3").AppendASCII("src"),
  };

  return ret;
}

bool LocalTestServer::AddCommandLineArguments(
    base::CommandLine* command_line) const {
  std::optional<base::Value::Dict> arguments_dict = GenerateArguments();
  if (!arguments_dict)
    return false;

  // Serialize the argument dictionary into CommandLine.
  for (auto it = arguments_dict->begin(); it != arguments_dict->end(); ++it) {
    const base::Value& value = it->second;
    const std::string& key = it->first;

    // Add arguments from a list.
    if (value.is_list()) {
      if (value.GetList().empty())
        return false;
      for (const auto& entry : value.GetList()) {
        if (!AppendArgumentFromJSONValue(key, entry, command_line))
          return false;
      }
    } else if (!AppendArgumentFromJSONValue(key, value, command_line)) {
      return false;
    }
  }

  // Append the appropriate server type argument.
  switch (type()) {
    case TYPE_WS:
    case TYPE_WSS:
      command_line->AppendArg("--websocket");
      break;
    case TYPE_BASIC_AUTH_PROXY:
      command_line->AppendArg("--basic-auth-proxy");
      break;
    case TYPE_PROXY:
      command_line->AppendArg("--proxy");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  return true;
}

}  // namespace net
