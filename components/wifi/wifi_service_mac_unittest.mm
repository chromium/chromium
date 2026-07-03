// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi/wifi_service.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wifi {

namespace {

// Matches the notification name observed by WiFiServiceMac::SetEventObservers.
NSNotificationName const kCoreWLANSSIDChangedNotification =
    @"com.apple.coreWLAN.notification.ssid.legacy";

}  // namespace

using WiFiServiceMacTest = testing::Test;

// CoreWLAN posts SSID-change notifications from a background dispatch queue,
// so the observer block can run concurrently with the WiFiService being torn
// down on its worker sequence. Any task the block posts to the worker sequence
// may therefore run after the service has been destroyed and must not touch
// it.
TEST_F(WiFiServiceMacTest, NotificationDeliveredAfterServiceDestroyed) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<WiFiService> wifi_service(WiFiService::Create());
  wifi_service->Initialize(base::SequencedTaskRunner::GetCurrentDefault());
  wifi_service->SetEventObservers(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*networks_changed_observer=*/base::DoNothing(),
      /*network_list_changed_observer=*/base::DoNothing());

  // The observer is registered with `queue:nil`, so the block is invoked
  // synchronously on the posting thread and queues its work on the worker
  // sequence before destruction below.
  [NSNotificationCenter.defaultCenter
      postNotificationName:kCoreWLANSSIDChangedNotification
                    object:nil];

  wifi_service.reset();

  // Draining the queued work must not touch the destroyed service.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace wifi
