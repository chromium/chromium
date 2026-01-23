// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_net_log_observer.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log.h"
#include "net/log/net_log_util.h"

namespace {

// TODO(crbug.com/40257546): This should be updated to pass a
// base::DictValue instead of a std::unique_ptr.
std::unique_ptr<base::DictValue> GetWebEngineConstants() {
  base::DictValue constants_dict = net::GetNetConstants();

  base::DictValue dict;
  dict.Set("name", "WebEngine");
  dict.Set("command_line",
           base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  constants_dict.Set("clientInfo", std::move(dict));

  return std::make_unique<base::DictValue>(std::move(constants_dict));
}

}  // namespace

WebEngineNetLogObserver::WebEngineNetLogObserver(
    const base::FilePath& log_path) {
  if (!log_path.empty()) {
    net::NetLogCaptureMode capture_mode = net::NetLogCaptureMode::kDefault;
    file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
        log_path, capture_mode, GetWebEngineConstants());
    file_net_log_observer_->StartObserving(net::NetLog::Get());
  }
}

WebEngineNetLogObserver::~WebEngineNetLogObserver() {
  if (file_net_log_observer_)
    file_net_log_observer_->StopObserving(nullptr, base::OnceClosure());
}
