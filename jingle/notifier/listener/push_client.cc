// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/push_client.h"

#include <cstddef>

#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "jingle/notifier/listener/non_blocking_push_client.h"
#include "jingle/notifier/listener/xmpp_push_client.h"

namespace notifier {

PushClient::~PushClient() {}

namespace {

std::unique_ptr<PushClient> CreateXmppPushClient(
    const NotifierOptions& notifier_options) {
  return std::unique_ptr<PushClient>(new XmppPushClient(notifier_options));
}

}  // namespace

std::unique_ptr<PushClient> PushClient::CreateDefault(
    const NotifierOptions& notifier_options) {
  return std::make_unique<NonBlockingPushClient>(
      notifier_options.network_config.task_runner,
      base::BindOnce(&CreateXmppPushClient, notifier_options));
}

std::unique_ptr<PushClient> PushClient::CreateDefaultOnIOThread(
    const NotifierOptions& notifier_options) {
  CHECK(notifier_options.network_config.task_runner->BelongsToCurrentThread());
  return CreateXmppPushClient(notifier_options);
}

}  // namespace notifier
