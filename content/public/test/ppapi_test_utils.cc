// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/ppapi_test_utils.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_tcp_socket_message_filter.h"
#include "content/browser/renderer_host/pepper/pepper_udp_socket_message_filter.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "ppapi/shared_impl/ppapi_switches.h"

using CharType = base::FilePath::CharType;
using StringType = base::FilePath::StringType;

namespace ppapi {

namespace {

struct PluginInfo {
  PluginInfo(const StringType& library_name,
             const StringType& extra_params,
             const StringType& mime_type)
      : library_name(library_name),
        extra_params(extra_params),
        mime_type(mime_type) {}

  StringType library_name;
  // Typically a string of the form #name#description#version
  StringType extra_params;
  StringType mime_type;
};

bool RegisterPlugins(base::CommandLine* command_line,
                     const std::vector<PluginInfo>& plugins) {
  base::FilePath plugin_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &plugin_dir))
    return false;

  StringType args;
  for (const auto& plugin : plugins) {
    if (!args.empty())
      args += FILE_PATH_LITERAL(",");
    base::FilePath plugin_path = plugin_dir.Append(plugin.library_name);
    if (!base::PathExists(plugin_path))
      return false;
    args += plugin_path.value();
    args += plugin.extra_params;
    args += FILE_PATH_LITERAL(";");
    args += plugin.mime_type;
  }
  command_line->AppendSwitchNative(switches::kRegisterPepperPlugins, args);
  command_line->AppendSwitch(switches::kAllowCommandLinePlugins);
  return true;
}

bool RegisterPluginWithDefaultMimeType(
    base::CommandLine* command_line,
    const base::FilePath::StringType& library_name,
    const base::FilePath::StringType& extra_registration_parameters) {
  std::vector<PluginInfo> plugins;
  plugins.push_back(PluginInfo(library_name, extra_registration_parameters,
                               FILE_PATH_LITERAL("application/x-ppapi-tests")));
  return RegisterPlugins(command_line, plugins);
}

}  // namespace

bool RegisterTestPlugin(base::CommandLine* command_line) {
  return RegisterTestPluginWithExtraParameters(command_line,
                                               FILE_PATH_LITERAL(""));
}

bool RegisterTestPluginWithExtraParameters(
    base::CommandLine* command_line,
    const base::FilePath::StringType& extra_registration_parameters) {
#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType plugin_library = L"ppapi_tests.dll";
#elif BUILDFLAG(IS_MAC)
  base::FilePath::StringType plugin_library = "ppapi_tests.plugin";
#elif BUILDFLAG(IS_POSIX)
  base::FilePath::StringType plugin_library = "libppapi_tests.so";
#endif
  return RegisterPluginWithDefaultMimeType(command_line, plugin_library,
                                           extra_registration_parameters);
}

bool RegisterBlinkTestPlugin(base::CommandLine* command_line) {
#if BUILDFLAG(IS_WIN)
  static const CharType kPluginLibrary[] = L"blink_test_plugin.dll";
  static const CharType kDeprecatedPluginLibrary[] =
      L"blink_deprecated_test_plugin.dll";
#elif BUILDFLAG(IS_MAC)
  static const CharType kPluginLibrary[] = "blink_test_plugin.plugin";
  static const CharType kDeprecatedPluginLibrary[] =
      "blink_deprecated_test_plugin.plugin";
#elif BUILDFLAG(IS_POSIX)
  static const CharType kPluginLibrary[] = "libblink_test_plugin.so";
  static const CharType kDeprecatedPluginLibrary[] =
      "libblink_deprecated_test_plugin.so";
#endif
  static const CharType kExtraParameters[] =
      FILE_PATH_LITERAL("#Blink Test Plugin#Interesting description.#0.8");
  static const CharType kDeprecatedExtraParameters[] =
      FILE_PATH_LITERAL("#Blink Deprecated Test Plugin#Description#0.1");

  std::vector<PluginInfo> plugins;
  plugins.push_back(PluginInfo(
      kPluginLibrary, kExtraParameters,
      FILE_PATH_LITERAL("application/x-blink-test-plugin#blinktestplugin")));
  plugins.push_back(
      PluginInfo(kDeprecatedPluginLibrary, kDeprecatedExtraParameters,
                 FILE_PATH_LITERAL("application/"
                                   "x-blink-deprecated-test-plugin#"
                                   "blinkdeprecatedtestplugin")));
  return RegisterPlugins(command_line, plugins);
}

void SetPepperTCPNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  content::PepperTCPServerSocketMessageFilter::SetNetworkContextForTesting(
      network_context);
  content::PepperTCPSocketMessageFilter::SetNetworkContextForTesting(
      network_context);
}

void SetPepperUDPSocketCallackForTesting(
    const CreateUDPSocketCallback* create_udp_socket_callback) {
  content::PepperUDPSocketMessageFilter::SetCreateUDPSocketCallbackForTesting(
      create_udp_socket_callback);
}

}  // namespace ppapi
