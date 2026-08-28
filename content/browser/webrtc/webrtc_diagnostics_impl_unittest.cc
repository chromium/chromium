// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_diagnostics_impl.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kClientA[] = "client-a";
constexpr char kClientB[] = "client-b";

class MockWebRtcDiagnosticsObserver : public WebRtcDiagnostics::Observer {
 public:
  MockWebRtcDiagnosticsObserver() = default;
  ~MockWebRtcDiagnosticsObserver() override = default;

  int truncated_count() const { return truncated_count_; }
  const std::vector<std::string>& added_ids() const { return added_ids_; }
  const std::vector<std::string>& removed_ids() const { return removed_ids_; }
  const std::vector<std::string>& stopped_clients() const {
    return stopped_clients_;
  }

  void OnPeerConnectionAdded(std::string_view id,
                             const base::Value& data) override {
    added_ids_.emplace_back(id);
  }
  void OnPeerConnectionRemoved(std::string_view id) override {
    removed_ids_.emplace_back(id);
  }
  void OnSnapshotTruncated(int dropped_log,
                           int dropped_stats,
                           int dropped_media) override {
    truncated_count_++;
  }
  void OnCaptureStopped(std::string_view stopped_client_id) override {
    stopped_clients_.emplace_back(stopped_client_id);
  }

 private:
  int truncated_count_ = 0;
  std::vector<std::string> added_ids_;
  std::vector<std::string> removed_ids_;
  std::vector<std::string> stopped_clients_;
};

class WebRtcDiagnosticsImplTest : public testing::Test {
 protected:
  WebRtcDiagnosticsImpl* diagnostics() {
    return WebRtcDiagnosticsImpl::GetInstance();
  }

  void TearDown() override {
    diagnostics()->ResetForTesting();
    testing::Test::TearDown();
  }

  // Runs GetSnapshot() and returns the snapshot. `ok` receives the
  // synchronous return value; the snapshot is left empty when it is false.
  base::Value Snapshot(BrowserContext* context,
                       std::string_view client_id,
                       const std::vector<url::Origin>& origins,
                       bool* ok) {
    base::RunLoop run_loop;
    base::Value snapshot;
    *ok = diagnostics()->GetSnapshot(
        context, client_id, origins,
        base::BindOnce(
            [](base::RunLoop* loop, base::Value* target, base::Value result) {
              *target = std::move(result);
              loop->Quit();
            },
            &run_loop, &snapshot));
    if (*ok) {
      run_loop.Run();
    }
    return snapshot;
  }

  base::Value PeerConnectionEntry(int rid, int lid, const char* url) {
    base::DictValue pc;
    pc.Set("rid", rid);
    pc.Set("lid", lid);
    if (url) {
      pc.Set("url", url);
    }
    return base::Value(std::move(pc));
  }

  base::Value MediaEntry(int rid, int request_id, const char* origin) {
    base::DictValue media;
    media.Set("rid", rid);
    media.Set("request_id", request_id);
    if (origin) {
      media.Set("origin", origin);
    }
    return base::Value(std::move(media));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Two profiles running the same client must get independent sessions. Before
// per-context state this returned kAlreadyCapturing for the second profile,
// which then silently inherited the first profile's origin filter.
TEST_F(WebRtcDiagnosticsImplTest, SameClientIdInTwoProfilesIsTwoSessions) {
  TestBrowserContext context_a;
  TestBrowserContext context_b;

  EXPECT_EQ(diagnostics()->StartCaptureForClient(
                &context_a, kClientA,
                {url::Origin::Create(GURL("https://a.example"))}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  EXPECT_EQ(diagnostics()->StartCaptureForClient(
                &context_b, kClientA,
                {url::Origin::Create(GURL("https://b.example"))}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  EXPECT_TRUE(diagnostics()->IsCapturingForClient(&context_a, kClientA));
  EXPECT_TRUE(diagnostics()->IsCapturingForClient(&context_b, kClientA));

  // Each session keeps its own filter.
  auto filter_a =
      diagnostics()->GetFilterOriginsForClient(&context_a, kClientA);
  auto filter_b =
      diagnostics()->GetFilterOriginsForClient(&context_b, kClientA);
  ASSERT_TRUE(filter_a);
  ASSERT_TRUE(filter_b);
  ASSERT_EQ(filter_a->size(), 1u);
  ASSERT_EQ(filter_b->size(), 1u);
  EXPECT_EQ(filter_a->front().host(), "a.example");
  EXPECT_EQ(filter_b->front().host(), "b.example");

  // Stopping in one profile must not stop the other.
  EXPECT_EQ(diagnostics()->StopCaptureForClient(&context_a, kClientA),
            WebRtcDiagnostics::StopCaptureResult::kSuccess);
  EXPECT_FALSE(diagnostics()->IsCapturingForClient(&context_a, kClientA));
  EXPECT_TRUE(diagnostics()->IsCapturingForClient(&context_b, kClientA));
}

// Starting twice in the same profile is still rejected.
TEST_F(WebRtcDiagnosticsImplTest, SecondStartInSameProfileIsAlreadyCapturing) {
  TestBrowserContext context;

  EXPECT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  EXPECT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kAlreadyCapturing);
}

TEST_F(WebRtcDiagnosticsImplTest, StopWithoutSessionIsNotCapturing) {
  TestBrowserContext context;
  EXPECT_EQ(diagnostics()->StopCaptureForClient(&context, kClientA),
            WebRtcDiagnostics::StopCaptureResult::kNotCapturing);
}

// An observer only ever hears about its own profile's sessions.
TEST_F(WebRtcDiagnosticsImplTest, CaptureStoppedDoesNotCrossProfiles) {
  TestBrowserContext context_a;
  TestBrowserContext context_b;
  MockWebRtcDiagnosticsObserver observer_a;
  MockWebRtcDiagnosticsObserver observer_b;
  diagnostics()->AddObserver(&context_a, &observer_a);
  diagnostics()->AddObserver(&context_b, &observer_b);

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_b, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  diagnostics()->StopCaptureForClient(&context_a, kClientA);

  EXPECT_EQ(observer_a.stopped_clients().size(), 1u);
  EXPECT_TRUE(observer_b.stopped_clients().empty());

  diagnostics()->RemoveObserver(&context_a, &observer_a);
  diagnostics()->RemoveObserver(&context_b, &observer_b);
}

// A snapshot is only served to a client that actually has a session, so one
// extension cannot read what another caused to be captured.
TEST_F(WebRtcDiagnosticsImplTest, SnapshotRequiresAnActiveSession) {
  TestBrowserContext context;
  bool ok = false;

  Snapshot(&context, kClientA, {}, &ok);
  EXPECT_FALSE(ok);

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  Snapshot(&context, kClientA, {}, &ok);
  EXPECT_TRUE(ok);

  // A different client in the same profile still has no session of its own.
  Snapshot(&context, kClientB, {}, &ok);
  EXPECT_FALSE(ok);
}

// The request's filter narrows the session's filter and can never widen it.
TEST_F(WebRtcDiagnosticsImplTest, RequestFilterCannotWidenSessionFilter) {
  TestBrowserContext context;
  MockRenderProcessHost rph(&context);
  const int rid = rph.GetDeprecatedID();

  ASSERT_EQ(diagnostics()->StartCaptureForClient(
                &context, kClientA,
                {url::Origin::Create(GURL("https://allowed.example"))}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::ListValue pcs;
  pcs.Append(PeerConnectionEntry(rid, 1, "https://allowed.example/call"));
  pcs.Append(PeerConnectionEntry(rid, 2, "https://other.example/call"));
  base::Value update(std::move(pcs));
  diagnostics()->OnUpdate("update-all-peer-connections", &update);

  // Unfiltered request: still limited to the session's own origin.
  bool ok = false;
  base::Value snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  const base::DictValue* seen = snapshot.GetDict().FindDict("PeerConnections");
  ASSERT_TRUE(seen);
  EXPECT_EQ(seen->size(), 1u);

  // Explicitly asking for the origin outside the session yields nothing.
  snapshot =
      Snapshot(&context, kClientA,
               {url::Origin::Create(GURL("https://other.example"))}, &ok);
  ASSERT_TRUE(ok);
  seen = snapshot.GetDict().FindDict("PeerConnections");
  ASSERT_TRUE(seen);
  EXPECT_TRUE(seen->empty());
}

// Closing a peer connection must drop its cached data, not just its metadata.
// Retaining it grew the cache without bound and kept SDP and ICE candidates
// for calls that had already ended.
TEST_F(WebRtcDiagnosticsImplTest, RemovePeerConnectionDropsItsData) {
  TestBrowserContext context;
  MockRenderProcessHost rph(&context);
  const int rid = rph.GetDeprecatedID();

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::Value added = PeerConnectionEntry(rid, 7, "https://example.com/call");
  diagnostics()->OnUpdate("add-peer-connection", &added);

  bool ok = false;
  base::Value snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  const base::DictValue* seen = snapshot.GetDict().FindDict("PeerConnections");
  ASSERT_TRUE(seen);
  EXPECT_EQ(seen->size(), 1u);

  base::DictValue removal;
  removal.Set("rid", rid);
  removal.Set("lid", 7);
  base::Value removed(std::move(removal));
  diagnostics()->OnUpdate("remove-peer-connection", &removed);

  snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  seen = snapshot.GetDict().FindDict("PeerConnections");
  ASSERT_TRUE(seen);
  EXPECT_TRUE(seen->empty());
}

// Replaying the same getUserMedia entry must not duplicate it, otherwise a
// context that starts capturing while calls are in progress inflates its own
// cache and its dropped-entry counters.
TEST_F(WebRtcDiagnosticsImplTest, RepeatedAddMediaUpsertsRatherThanDuplicates) {
  TestBrowserContext context;
  MockRenderProcessHost rph(&context);
  const int rid = rph.GetDeprecatedID();

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::Value media = MediaEntry(rid, 1, "https://example.com");
  for (int i = 0; i < 5; ++i) {
    diagnostics()->OnUpdate("add-media", &media);
  }

  bool ok = false;
  base::Value snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  const base::ListValue* seen = snapshot.GetDict().FindList("getUserMedia");
  ASSERT_TRUE(seen);
  EXPECT_EQ(seen->size(), 1u);
}

// update-media carries only the fields that changed, so it must merge onto the
// entry add-media already created rather than append a second one.
TEST_F(WebRtcDiagnosticsImplTest, UpdateMediaMergesIntoExistingEntry) {
  TestBrowserContext context;
  MockRenderProcessHost rph(&context);
  const int rid = rph.GetDeprecatedID();

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::Value media = MediaEntry(rid, 1, "https://example.com");
  diagnostics()->OnUpdate("add-media", &media);

  base::DictValue update;
  update.Set("rid", rid);
  update.Set("request_id", 1);
  update.Set("audio", "audio-constraints");
  update.Set("video", "video-constraints");
  update.Set("audio_track_info", "audio-track");
  update.Set("video_track_info", "video-track");
  update.Set("error", "NotAllowedError");
  update.Set("error_message", "permission denied");
  base::Value update_value(std::move(update));
  diagnostics()->OnUpdate("update-media", &update_value);

  bool ok = false;
  base::Value snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  const base::ListValue* seen = snapshot.GetDict().FindList("getUserMedia");
  ASSERT_TRUE(seen);
  ASSERT_EQ(seen->size(), 1u);

  const base::DictValue* entry = (*seen)[0].GetIfDict();
  ASSERT_TRUE(entry);
  // The original fields survive and the update's fields are merged in.
  EXPECT_EQ(entry->FindInt("request_id"), 1);
  EXPECT_EQ(*entry->FindString("origin"), "https://example.com");
  EXPECT_EQ(*entry->FindString("audio"), "audio-constraints");
  EXPECT_EQ(*entry->FindString("video"), "video-constraints");
  EXPECT_EQ(*entry->FindString("audio_track_info"), "audio-track");
  EXPECT_EQ(*entry->FindString("video_track_info"), "video-track");
  EXPECT_EQ(*entry->FindString("error"), "NotAllowedError");
  EXPECT_EQ(*entry->FindString("error_message"), "permission denied");
}

// Data belonging to one profile must never appear in another's snapshot, and
// an off-the-record profile is just another profile for this purpose.
TEST_F(WebRtcDiagnosticsImplTest, SnapshotIsScopedToItsBrowserContext) {
  TestBrowserContext context_a;
  MockRenderProcessHost rph_a(&context_a);
  TestBrowserContext context_b;
  MockRenderProcessHost rph_b(&context_b);
  TestBrowserContext otr_context;
  otr_context.set_is_off_the_record(true);
  MockRenderProcessHost rph_otr(&otr_context);

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_b, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&otr_context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::ListValue pcs;
  pcs.Append(PeerConnectionEntry(rph_a.GetDeprecatedID(), 1,
                                 "https://a.example/call"));
  base::Value update(std::move(pcs));
  diagnostics()->OnUpdate("update-all-peer-connections", &update);

  base::Value media_b =
      MediaEntry(rph_b.GetDeprecatedID(), 1, "https://b.example");
  diagnostics()->OnUpdate("add-media", &media_b);
  base::Value media_otr =
      MediaEntry(rph_otr.GetDeprecatedID(), 2, "https://otr.example");
  diagnostics()->OnUpdate("add-media", &media_otr);

  bool ok = false;
  base::Value snapshot_a = Snapshot(&context_a, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_FALSE(snapshot_a.GetDict().FindDict("PeerConnections")->empty());
  EXPECT_TRUE(snapshot_a.GetDict().FindList("getUserMedia")->empty());

  base::Value snapshot_b = Snapshot(&context_b, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(snapshot_b.GetDict().FindDict("PeerConnections")->empty());
  EXPECT_EQ(snapshot_b.GetDict().FindList("getUserMedia")->size(), 1u);

  base::Value snapshot_otr = Snapshot(&otr_context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(snapshot_otr.GetDict().FindList("getUserMedia")->size(), 1u);
}

// An entry whose rid does not resolve to a live renderer cannot be attributed
// to a profile, so it must be dropped rather than shown to an arbitrary one.
TEST_F(WebRtcDiagnosticsImplTest, UnattributableEntryIsDropped) {
  TestBrowserContext context;
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  // No MockRenderProcessHost exists with this id.
  base::Value media = MediaEntry(999999, 1, "https://example.com");
  diagnostics()->OnUpdate("add-media", &media);

  bool ok = false;
  base::Value snapshot = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(snapshot.GetDict().FindList("getUserMedia")->empty());
}

// A peer connection with no usable origin is dropped from a filtered read and
// kept by an unfiltered one.
TEST_F(WebRtcDiagnosticsImplTest, OriginlessPeerConnectionRespectsFilter) {
  TestBrowserContext context;
  MockRenderProcessHost rph(&context);
  const int rid = rph.GetDeprecatedID();

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::ListValue pcs;
  pcs.Append(PeerConnectionEntry(rid, 1, /*url=*/nullptr));
  base::Value update(std::move(pcs));
  diagnostics()->OnUpdate("update-all-peer-connections", &update);

  bool ok = false;
  base::Value unfiltered = Snapshot(&context, kClientA, {}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_FALSE(unfiltered.GetDict().FindDict("PeerConnections")->empty());

  base::Value filtered =
      Snapshot(&context, kClientA,
               {url::Origin::Create(GURL("http://example.com"))}, &ok);
  ASSERT_TRUE(ok);
  EXPECT_TRUE(filtered.GetDict().FindDict("PeerConnections")->empty());
}

TEST_F(WebRtcDiagnosticsImplTest, StartRejectsTooManyOrigins) {
  TestBrowserContext context;
  std::vector<url::Origin> origins;
  for (size_t i = 0; i <= WebRtcDiagnostics::kMaxFilterOrigins; ++i) {
    origins.push_back(url::Origin::Create(GURL("https://example.com")));
  }
  EXPECT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, origins),
            WebRtcDiagnostics::StartCaptureResult::kTooManyOrigins);
}

TEST_F(WebRtcDiagnosticsImplTest, GetSnapshotRejectsTooManyOrigins) {
  TestBrowserContext context;
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  std::vector<url::Origin> origins;
  for (size_t i = 0; i <= WebRtcDiagnostics::kMaxFilterOrigins; ++i) {
    origins.push_back(url::Origin::Create(GURL("https://example.com")));
  }

  EXPECT_FALSE(diagnostics()->GetSnapshot(
      &context, kClientA, origins, base::BindOnce([](base::Value result) {
        ADD_FAILURE() << "Callback should not be invoked";
      })));
}

TEST_F(WebRtcDiagnosticsImplTest, StartRejectsOpaqueOrigin) {
  TestBrowserContext context;
  EXPECT_EQ(diagnostics()->StartCaptureForClient(
                &context, kClientA, {url::Origin::Create(GURL("about:blank"))}),
            WebRtcDiagnostics::StartCaptureResult::kInvalidOrigin);
}

// Truncation is reported per profile, rate limited per profile, and counts
// only that profile's own dropped entries.
TEST_F(WebRtcDiagnosticsImplTest, SnapshotTruncationIsPerProfile) {
  TestBrowserContext context_a;
  MockRenderProcessHost rph_a(&context_a);
  TestBrowserContext context_b;
  MockRenderProcessHost rph_b(&context_b);

  MockWebRtcDiagnosticsObserver observer_a;
  MockWebRtcDiagnosticsObserver observer_b;
  diagnostics()->AddObserver(&context_a, &observer_a);
  diagnostics()->AddObserver(&context_b, &observer_b);

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_b, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  // Distinct request_ids, so each is a new entry rather than an upsert.
  for (int i = 0; i < 2500; ++i) {
    base::Value media =
        MediaEntry(rph_a.GetDeprecatedID(), i, "https://a.example");
    diagnostics()->OnUpdate("add-media", &media);
  }

  EXPECT_EQ(observer_a.truncated_count(), 1);
  EXPECT_EQ(observer_b.truncated_count(), 0);

  // Rate limited to one notification per five seconds, per profile.
  task_environment_.FastForwardBy(base::Seconds(5));
  base::Value media =
      MediaEntry(rph_a.GetDeprecatedID(), 99999, "https://a.example");
  diagnostics()->OnUpdate("add-media", &media);
  EXPECT_EQ(observer_a.truncated_count(), 2);
  EXPECT_EQ(observer_b.truncated_count(), 0);

  diagnostics()->RemoveObserver(&context_a, &observer_a);
  diagnostics()->RemoveObserver(&context_b, &observer_b);
}

// Peer connection events reach the observer of the profile they belong to.
TEST_F(WebRtcDiagnosticsImplTest, PeerConnectionEventsAreScopedToProfile) {
  TestBrowserContext context_a;
  MockRenderProcessHost rph_a(&context_a);
  TestBrowserContext context_b;
  MockWebRtcDiagnosticsObserver observer_a;
  MockWebRtcDiagnosticsObserver observer_b;
  diagnostics()->AddObserver(&context_a, &observer_a);
  diagnostics()->AddObserver(&context_b, &observer_b);

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_b, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  base::Value added =
      PeerConnectionEntry(rph_a.GetDeprecatedID(), 3, "https://a.example/call");
  diagnostics()->OnUpdate("add-peer-connection", &added);

  EXPECT_EQ(observer_a.added_ids().size(), 1u);
  EXPECT_TRUE(observer_b.added_ids().empty());

  base::DictValue removal;
  removal.Set("rid", rph_a.GetDeprecatedID());
  removal.Set("lid", 3);
  base::Value removed(std::move(removal));
  diagnostics()->OnUpdate("remove-peer-connection", &removed);

  EXPECT_EQ(observer_a.removed_ids().size(), 1u);
  EXPECT_TRUE(observer_b.removed_ids().empty());

  diagnostics()->RemoveObserver(&context_a, &observer_a);
  diagnostics()->RemoveObserver(&context_b, &observer_b);
}

TEST_F(WebRtcDiagnosticsImplTest, GetCapturingClientsIsPerProfile) {
  TestBrowserContext context_a;
  TestBrowserContext context_b;

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_a, kClientB, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context_b, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  EXPECT_EQ(diagnostics()->GetCapturingClients(&context_a).size(), 2u);
  EXPECT_EQ(diagnostics()->GetCapturingClients(&context_b).size(), 1u);

  TestBrowserContext context_c;
  EXPECT_TRUE(diagnostics()->GetCapturingClients(&context_c).empty());
}

// The filter lookup distinguishes "no session" from "unfiltered session", so
// a missing session can never be read as "match every origin".
TEST_F(WebRtcDiagnosticsImplTest, FilterLookupDistinguishesMissingFromEmpty) {
  TestBrowserContext context;

  EXPECT_FALSE(diagnostics()->GetFilterOriginsForClient(&context, kClientA));

  ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
            WebRtcDiagnostics::StartCaptureResult::kSuccess);

  auto filter = diagnostics()->GetFilterOriginsForClient(&context, kClientA);
  ASSERT_TRUE(filter);
  EXPECT_TRUE(filter->empty());
}

// A profile's state is destroyed with the profile, so nothing survives to be
// inherited by a later profile that happens to reuse its address.
TEST_F(WebRtcDiagnosticsImplTest, StateDiesWithTheBrowserContext) {
  {
    TestBrowserContext context;
    ASSERT_EQ(diagnostics()->StartCaptureForClient(&context, kClientA, {}),
              WebRtcDiagnostics::StartCaptureResult::kSuccess);
    EXPECT_TRUE(diagnostics()->IsCapturingForClient(&context, kClientA));
  }

  TestBrowserContext replacement;
  EXPECT_FALSE(diagnostics()->IsCapturingForClient(&replacement, kClientA));
  EXPECT_TRUE(diagnostics()->GetCapturingClients(&replacement).empty());
}

}  // namespace

}  // namespace content
