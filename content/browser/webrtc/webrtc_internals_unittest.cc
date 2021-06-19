// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/values.h"
#include "content/browser/webrtc/webrtc_internals_connections_observer.h"
#include "content/browser/webrtc/webrtc_internals_ui_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const GlobalRenderFrameHostId kFrameId = {20, 30};
const int kLid = 40;
const int kPid = 123;
const char kConstraints[] = "c";
const char kRtcConfiguration[] = "r";
const char kUrl[] = "u";
const char* const kWakeLockConnectingValues[] = {"checking", "connected",
                                                 "completed"};
const char* const kWakeLockDisconnectingValues[] = {"disconnected", "closed",
                                                    "failed", "new"};
const char kAudioConstraint[] = "aaa";
const char kVideoConstraint[] = "vvv";

class MockWebRtcInternalsProxy : public WebRTCInternalsUIObserver {
 public:
  MockWebRtcInternalsProxy() = default;
  explicit MockWebRtcInternalsProxy(base::RunLoop* loop) : loop_(loop) {}

  const std::string& event_name() const { return event_name_; }

  base::Value* event_data() { return event_data_.get(); }

 private:
  void OnUpdate(const std::string& event_name,
                const base::Value* event_data) override {
    event_name_ = event_name;
    event_data_.reset(event_data ? event_data->DeepCopy() : nullptr);
    if (loop_)
      loop_->Quit();
  }

  std::string event_name_;
  std::unique_ptr<base::Value> event_data_;
  base::RunLoop* loop_{nullptr};
};

class MockWakeLock : public device::mojom::WakeLock {
 public:
  explicit MockWakeLock(mojo::PendingReceiver<device::mojom::WakeLock> receiver)
      : receiver_(this, std::move(receiver)), has_wakelock_(false) {}
  ~MockWakeLock() override {}

  // Implement device::mojom::WakeLock:
  void RequestWakeLock() override { has_wakelock_ = true; }
  void CancelWakeLock() override { has_wakelock_ = false; }
  void AddClient(
      mojo::PendingReceiver<device::mojom::WakeLock> receiver) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {}
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {}

  bool HasWakeLock() {
    base::RunLoop().RunUntilIdle();
    return has_wakelock_;
  }

 private:
  mojo::Receiver<device::mojom::WakeLock> receiver_;
  bool has_wakelock_;
};

class TestWebRtcConnectionsObserver
    : public WebRtcInternalsConnectionsObserver {
 public:
  TestWebRtcConnectionsObserver() = default;

  ~TestWebRtcConnectionsObserver() override = default;

  uint32_t latest_connections_count() const {
    return latest_connections_count_;
  }

 private:
  // content::WebRtcInternalsConnectionsObserver:
  void OnConnectionsCountChange(uint32_t count) override {
    latest_connections_count_ = count;
  }

  uint32_t latest_connections_count_ = 0u;
};

}  // namespace

// Derived class for testing only.  Allows the tests to have their own instance
// for testing and control the period for which WebRTCInternals will bulk up
// updates (changes down from 500ms to 1ms).
class WebRTCInternalsForTest : public WebRTCInternals {
 public:
  WebRTCInternalsForTest()
      : WebRTCInternals(1, true),
        mock_wake_lock_(wake_lock_.BindNewPipeAndPassReceiver()) {}

  ~WebRTCInternalsForTest() override {}

  bool HasWakeLock() { return mock_wake_lock_.HasWakeLock(); }

 private:
  MockWakeLock mock_wake_lock_;
};

class WebRtcInternalsTest : public testing::Test {
 protected:
  void VerifyString(const base::DictionaryValue& dict,
                    const std::string& key,
                    const std::string& expected) {
    const std::string* actual = dict.FindStringKey(key);
    ASSERT_TRUE(actual);
    EXPECT_EQ(expected, *actual);
  }

  void VerifyInt(const base::DictionaryValue& dict,
                 const std::string& key,
                 int expected) {
    absl::optional<int> actual = dict.FindIntKey(key);
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(expected, actual.value());
  }

  void VerifyList(const base::Value& dict,
                  base::StringPiece key,
                  const base::Value& expected) {
    ASSERT_TRUE(dict.is_dict());
    ASSERT_TRUE(expected.is_list());
    const base::Value* actual = dict.FindListKey(key);
    ASSERT_TRUE(actual);
    EXPECT_TRUE(expected.Equals(actual));
  }

  void VerifyGetUserMediaData(base::Value* actual_data,
                              GlobalRenderFrameHostId frame_id,
                              int pid,
                              const std::string& origin,
                              const std::string& audio,
                              const std::string& video) {
    ASSERT_TRUE(actual_data->is_dict());
    const base::DictionaryValue& dict =
        base::Value::AsDictionaryValue(*actual_data);

    VerifyInt(dict, "rid", frame_id.child_id);
    VerifyInt(dict, "pid", pid);
    VerifyString(dict, "origin", origin);
    VerifyString(dict, "audio", audio);
    VerifyString(dict, "video", video);
  }

  BrowserTaskEnvironment task_environment_;
};

TEST_F(WebRtcInternalsTest, AddRemoveObserver) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);

  webrtc_internals.RemoveObserver(&observer);
  // The observer should not get notified of this activity.
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);

  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  EXPECT_EQ("", observer.event_name());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, EnsureNoLogWhenNoObserver) {
  base::RunLoop loop;
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid, "update_type",
                                           "update_value");
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  // Make sure we don't have a log entry since there was no observer.
  MockWebRtcInternalsProxy observer;
  webrtc_internals.UpdateObserver(&observer);
  EXPECT_EQ("update-all-peer-connections", observer.event_name());

  base::ListValue* list = nullptr;
  ASSERT_TRUE(observer.event_data()->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());
  base::DictionaryValue* dict = nullptr;
  ASSERT_TRUE((*list->GetList().begin()).GetAsDictionary(&dict));
  base::ListValue* log = nullptr;
  ASSERT_FALSE(dict->GetList("log", &log));

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, EnsureLogIsRemovedWhenObserverIsRemoved) {
  base::RunLoop loop;
  WebRTCInternalsForTest webrtc_internals;
  MockWebRtcInternalsProxy observer;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid, "update_type",
                                           "update_value");
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();

  // Make sure we have a log entry since there was an observer.
  webrtc_internals.UpdateObserver(&observer);
  EXPECT_EQ("update-all-peer-connections", observer.event_name());

  base::ListValue* list = nullptr;
  ASSERT_TRUE(observer.event_data()->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());
  base::DictionaryValue* dict = nullptr;
  ASSERT_TRUE((*list->GetList().begin()).GetAsDictionary(&dict));
  base::ListValue* log = nullptr;
  ASSERT_TRUE(dict->GetList("log", &log));

  // Make sure we the log entry was removed when the last observer was removed.
  webrtc_internals.RemoveObserver(&observer);
  webrtc_internals.UpdateObserver(&observer);
  EXPECT_EQ("update-all-peer-connections", observer.event_name());

  ASSERT_TRUE(observer.event_data()->GetAsList(&list));
  EXPECT_EQ(1U, list->GetSize());
  ASSERT_TRUE((*list->GetList().begin()).GetAsDictionary(&dict));
  ASSERT_FALSE(dict->GetList("log", &log));

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, SendAddPeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);

  loop.Run();

  ASSERT_EQ("add-peer-connection", observer.event_name());

  ASSERT_TRUE(observer.event_data()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*observer.event_data());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);
  VerifyInt(dict, "pid", kPid);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "rtcConfiguration", kRtcConfiguration);
  VerifyString(dict, "constraints", kConstraints);

  webrtc_internals.RemoveObserver(&observer);
  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, SendRemovePeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);

  loop.Run();

  ASSERT_EQ("remove-peer-connection", observer.event_name());

  ASSERT_TRUE(observer.event_data()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*observer.event_data());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);

  webrtc_internals.RemoveObserver(&observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, SendUpdatePeerConnectionUpdate) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);

  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";
  webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid, update_type,
                                           update_value);

  loop.Run();

  ASSERT_EQ("update-peer-connection", observer.event_name());

  ASSERT_TRUE(observer.event_data()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*observer.event_data());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);
  VerifyString(dict, "type", update_type);
  VerifyString(dict, "value", update_value);

  const std::string* time = dict.FindStringKey("time");
  ASSERT_TRUE(time);
  EXPECT_FALSE(time->empty());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  webrtc_internals.RemoveObserver(&observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, AddGetUserMedia) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;

  // Add one observer before "getUserMedia".
  webrtc_internals.AddObserver(&observer);

  webrtc_internals.OnGetUserMedia(kFrameId, kPid, kUrl, true, true,
                                  kAudioConstraint, kVideoConstraint);

  loop.Run();

  ASSERT_EQ("add-get-user-media", observer.event_name());
  VerifyGetUserMediaData(observer.event_data(), kFrameId, kPid, kUrl,
                         kAudioConstraint, kVideoConstraint);

  webrtc_internals.RemoveObserver(&observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, SendAllUpdateWithGetUserMedia) {
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.OnGetUserMedia(kFrameId, kPid, kUrl, true, true,
                                  kAudioConstraint, kVideoConstraint);

  MockWebRtcInternalsProxy observer;
  // Add one observer after "getUserMedia".
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.UpdateObserver(&observer);

  EXPECT_EQ("add-get-user-media", observer.event_name());
  VerifyGetUserMediaData(observer.event_data(), kFrameId, kPid, kUrl,
                         kAudioConstraint, kVideoConstraint);

  webrtc_internals.RemoveObserver(&observer);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, SendAllUpdatesWithPeerConnectionUpdate) {
  const std::string update_type = "fakeType";
  const std::string update_value = "fakeValue";

  WebRTCInternalsForTest webrtc_internals;

  MockWebRtcInternalsProxy observer;
  webrtc_internals.AddObserver(&observer);

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid, update_type,
                                           update_value);

  webrtc_internals.UpdateObserver(&observer);

  EXPECT_EQ("update-all-peer-connections", observer.event_name());
  ASSERT_TRUE(observer.event_data());

  ASSERT_TRUE(observer.event_data()->is_list());
  base::Value::ConstListView list = observer.event_data()->GetList();
  EXPECT_EQ(1U, list.size());

  ASSERT_TRUE(list.begin()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*list.begin());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);
  VerifyInt(dict, "pid", kPid);
  VerifyString(dict, "url", kUrl);
  VerifyString(dict, "rtcConfiguration", kRtcConfiguration);
  VerifyString(dict, "constraints", kConstraints);

  const base::Value* log_value = dict.FindListKey("log");
  ASSERT_TRUE(log_value);
  base::Value::ConstListView log = log_value->GetList();
  EXPECT_EQ(1U, log.size());

  ASSERT_TRUE(log.begin()->is_dict());
  const base::DictionaryValue& inner_dict =
      base::Value::AsDictionaryValue(*log.begin());
  VerifyString(inner_dict, "type", update_type);
  VerifyString(inner_dict, "value", update_value);

  const std::string* time = inner_dict.FindStringKey("time");
  ASSERT_TRUE(time);
  EXPECT_FALSE(time->empty());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, OnAddStandardStats) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);

  base::Value list(base::Value::Type::LIST);
  list.Append("xxx");
  list.Append("yyy");
  webrtc_internals.OnAddStandardStats(kFrameId, kLid, list.Clone());

  loop.Run();

  EXPECT_EQ("add-standard-stats", observer.event_name());
  ASSERT_TRUE(observer.event_data());

  ASSERT_TRUE(observer.event_data()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*observer.event_data());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);
  VerifyList(dict, "reports", list);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, OnAddLegacyStats) {
  base::RunLoop loop;
  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddObserver(&observer);
  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);

  base::Value list(base::Value::Type::LIST);
  list.Append("xxx");
  list.Append("yyy");
  webrtc_internals.OnAddLegacyStats(kFrameId, kLid, list.Clone());

  loop.Run();

  EXPECT_EQ("add-legacy-stats", observer.event_name());
  ASSERT_TRUE(observer.event_data());

  ASSERT_TRUE(observer.event_data()->is_dict());
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(*observer.event_data());

  VerifyInt(dict, "rid", kFrameId.child_id);
  VerifyInt(dict, "lid", kLid);
  VerifyList(dict, "reports", list);

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, AudioDebugRecordingsFileSelectionCanceled) {
  base::RunLoop loop;

  MockWebRtcInternalsProxy observer(&loop);
  WebRTCInternalsForTest webrtc_internals;

  webrtc_internals.AddObserver(&observer);
  webrtc_internals.FileSelectionCanceled(nullptr);

  loop.Run();

  EXPECT_EQ("audio-debug-recordings-file-selection-cancelled",
            observer.event_name());
  EXPECT_EQ(nullptr, observer.event_data());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockCreateRemove) {
  WebRTCInternalsForTest webrtc_internals;
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockConnecting) {
  for (const char* value : kWakeLockConnectingValues) {
    WebRTCInternalsForTest webrtc_internals;
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                           kRtcConfiguration, kConstraints);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid,
                                             "iceConnectionStateChange", value);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
    EXPECT_TRUE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());
  }

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockConnectingSequence) {
  WebRTCInternalsForTest webrtc_internals;
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  // A sequence of connecting messages should not increase the number of
  // connected connections beyond 1.
  for (const char* value : kWakeLockConnectingValues) {
    webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid,
                                             "iceConnectionStateChange", value);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
    EXPECT_TRUE(webrtc_internals.HasWakeLock());
  }

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockDisconnecting) {
  for (const char* value : kWakeLockDisconnectingValues) {
    WebRTCInternalsForTest webrtc_internals;
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                           kRtcConfiguration, kConstraints);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionUpdated(
        kFrameId, kLid, "iceConnectionStateChange", "connected");
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
    EXPECT_TRUE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid,
                                             "iceConnectionStateChange", value);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());

    webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());
  }

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockDisconnectingSequence) {
  WebRTCInternalsForTest webrtc_internals;
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  // A sequence of disconnecting messages should not decrease the number of
  // connected connections below zero.
  for (const char* value : kWakeLockDisconnectingValues) {
    webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLid,
                                             "iceConnectionStateChange", value);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());
  }

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockReconnect) {
  WebRTCInternalsForTest webrtc_internals;
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "disconnected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, WakeLockMultplePeerConnections) {
  const int kLids[] = {71, 72, 73};

  WebRTCInternalsForTest webrtc_internals;
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  for (const int lid : kLids) {
    webrtc_internals.OnPeerConnectionAdded(kFrameId, lid, kPid, kUrl,
                                           kRtcConfiguration, kConstraints);
    EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
    EXPECT_FALSE(webrtc_internals.HasWakeLock());
  }

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLids[0], "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLids[1], "iceConnectionStateChange", "completed");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 2);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLids[2], "iceConnectionStateChange", "checking");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 3);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  // A duplicate message should not alter the number of connected connections.
  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLids[2], "iceConnectionStateChange", "checking");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 3);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLids[0], "iceConnectionStateChange", "closed");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 2);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionUpdated(kFrameId, kLids[1], "stop",
                                           std::string());
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLids[0]);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLids[1]);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_TRUE(webrtc_internals.HasWakeLock());

  // Remove the remaining open peer connection.
  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLids[2]);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_FALSE(webrtc_internals.HasWakeLock());

  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcInternalsTest, TestWebRtcConnectionsObserver) {
  TestWebRtcConnectionsObserver observer;

  WebRTCInternalsForTest webrtc_internals;
  webrtc_internals.AddConnectionsObserver(&observer);
  EXPECT_EQ(0u, observer.latest_connections_count());
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);

  webrtc_internals.OnPeerConnectionAdded(kFrameId, kLid, kPid, kUrl,
                                         kRtcConfiguration, kConstraints);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_EQ(0u, observer.latest_connections_count());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_EQ(1u, observer.latest_connections_count());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "disconnected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_EQ(0u, observer.latest_connections_count());

  webrtc_internals.OnPeerConnectionUpdated(
      kFrameId, kLid, "iceConnectionStateChange", "connected");
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 1);
  EXPECT_EQ(1u, observer.latest_connections_count());

  webrtc_internals.OnPeerConnectionRemoved(kFrameId, kLid);
  EXPECT_EQ(webrtc_internals.num_connected_connections(), 0);
  EXPECT_EQ(0u, observer.latest_connections_count());

  base::RunLoop().RunUntilIdle();
}

// TODO(eladalon): Add tests that WebRtcEventLogger::Enable/Disable is
// correctly called. https://crbug.com/775415

}  // namespace content
