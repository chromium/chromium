// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/test/fake_plugin_service.h"

namespace content {

FakePluginService::FakePluginService() {
}

FakePluginService::~FakePluginService() {
}

void FakePluginService::Init() {
}

bool FakePluginService::GetPluginInfoArray(
    const GURL& url,
    const std::string& mime_type,
    bool allow_wildcard,
    std::vector<WebPluginInfo>* plugins,
    std::vector<std::string>* actual_mime_types) {
  return false;
}

bool FakePluginService::GetPluginInfo(int render_process_id,
                                      int render_frame_id,
                                      const GURL& url,
                                      const url::Origin& main_frame_origin,
                                      const std::string& mime_type,
                                      bool allow_wildcard,
                                      bool* is_stale,
                                      WebPluginInfo* info,
                                      std::string* actual_mime_type) {
  *is_stale = false;
  return false;
}

bool FakePluginService::GetPluginInfoByPath(const base::FilePath& plugin_path,
                                            WebPluginInfo* info) {
  return false;
}

base::string16 FakePluginService::GetPluginDisplayNameByPath(
    const base::FilePath& path) {
  return base::string16();
}

void FakePluginService::GetPlugins(GetPluginsCallback callback) {}

const PepperPluginInfo* FakePluginService::GetRegisteredPpapiPluginInfo(
    const base::FilePath& plugin_path) {
  return nullptr;
}

void FakePluginService::SetFilter(PluginServiceFilter* filter) {
}

PluginServiceFilter* FakePluginService::GetFilter() {
  return nullptr;
}

bool FakePluginService::IsPluginUnstable(const base::FilePath& path) {
  return false;
}

void FakePluginService::RefreshPlugins() {
}

void FakePluginService::RegisterInternalPlugin(
    const WebPluginInfo& info,
    bool add_at_beginning) {
}

void FakePluginService::UnregisterInternalPlugin(const base::FilePath& path) {
}

void FakePluginService::GetInternalPlugins(
    std::vector<WebPluginInfo>* plugins) {
}

bool FakePluginService::PpapiDevChannelSupported(
    BrowserContext* browser_context,
    const GURL& document_url) {
  return false;
}

int FakePluginService::CountPpapiPluginProcessesForProfile(
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory) {
  return 0;
}

}  // namespace content
