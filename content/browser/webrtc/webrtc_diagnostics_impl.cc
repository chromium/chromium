// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_diagnostics_impl.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/media/peer_connection_tracker_host.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const size_t kMaxMediaEntries = 1000;
const size_t kMaxLogEntries = 1000;
const size_t kMaxStatsEntries = 1000;

// Matches the polling interval used by chrome://webrtc-internals.
constexpr base::TimeDelta kStatsPollInterval = base::Seconds(3);

// Dictionary keys for peer connection data.
constexpr std::string_view kRidKey = "rid";
constexpr std::string_view kLidKey = "lid";
constexpr std::string_view kUrlKey = "url";
constexpr std::string_view kTypeKey = "type";
constexpr std::string_view kValueKey = "value";
constexpr std::string_view kTimeKey = "time";
constexpr std::string_view kTimestampKey = "timestamp";
constexpr std::string_view kReportsKey = "reports";
constexpr std::string_view kStatsKey = "stats";
constexpr std::string_view kLogKey = "log";
constexpr std::string_view kOriginKey = "origin";
constexpr std::string_view kRequestIdKey = "request_id";
constexpr std::string_view kAudioKey = "audio";
constexpr std::string_view kVideoKey = "video";
constexpr std::string_view kAudioTrackInfoKey = "audio_track_info";
constexpr std::string_view kVideoTrackInfoKey = "video_track_info";
constexpr std::string_view kErrorKey = "error";
constexpr std::string_view kErrorMessageKey = "error_message";

// Event names emitted by WebRTCInternals. Declared as constants because they
// are compared against strings produced in webrtc_internals.cc, where a typo
// would be a silent no-op rather than a compile error.
constexpr std::string_view kUpdateAllPeerConnections =
    "update-all-peer-connections";
constexpr std::string_view kAddMedia = "add-media";
constexpr std::string_view kUpdateMedia = "update-media";
constexpr std::string_view kAddPeerConnection = "add-peer-connection";
constexpr std::string_view kRemovePeerConnection = "remove-peer-connection";
constexpr std::string_view kUpdatePeerConnection = "update-peer-connection";
constexpr std::string_view kAddStandardStats = "add-standard-stats";

// Keys of the snapshot returned by GetSnapshot().
constexpr std::string_view kGetUserMediaOutKey = "getUserMedia";
constexpr std::string_view kPeerConnectionsOutKey = "PeerConnections";
constexpr std::string_view kUserAgentOutKey = "UserAgent";
constexpr std::string_view kUserAgentDataOutKey = "UserAgentData";

std::string MakePeerConnectionId(int rid, int lid) {
  return base::StringPrintf("%d-%d", rid, lid);
}

// True if `origin` matches any entry of `origins`. An empty `origins` means
// unfiltered and therefore matches everything.
bool MatchesFilter(const std::vector<url::Origin>& origins,
                   const url::Origin& origin) {
  if (origins.empty()) {
    return true;
  }
  return std::ranges::any_of(origins, [&origin](const url::Origin& filter) {
    return origin.IsSameOriginWith(filter);
  });
}

// True if the string stored under `key` parses to an origin matching
// `origins`. An entry with no usable origin never matches a non-empty filter.
bool EntryMatchesFilter(const base::DictValue& entry,
                        std::string_view key,
                        const std::vector<url::Origin>& origins) {
  if (origins.empty()) {
    return true;
  }
  const std::string* origin_str = entry.FindString(key);
  if (!origin_str) {
    return false;
  }
  return MatchesFilter(origins, url::Origin::Create(GURL(*origin_str)));
}

}  // namespace

// static
const void* WebRtcDiagnosticsImpl::PerContext::UserDataKey() {
  static const char kKey = 0;
  return &kKey;
}

WebRtcDiagnosticsImpl::PerContext::PerContext(WebRtcDiagnosticsImpl* owner,
                                              BrowserContext* context)
    : owner_(owner), context_(context) {}

WebRtcDiagnosticsImpl::PerContext::~PerContext() {
  owner_->OnPerContextDestroyed(this);
}

// static
WebRtcDiagnosticsImpl::PerContext*
WebRtcDiagnosticsImpl::PerContext::GetOrCreate(WebRtcDiagnosticsImpl* owner,
                                               BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context) {
    return nullptr;
  }
  if (PerContext* existing = GetIfExists(context)) {
    return existing;
  }
  // Never resurrect state on a profile that is already going away: the entry
  // would be destroyed immediately afterwards, and any observer registered
  // against it would outlive it.
  if (context->ShutdownStarted()) {
    return nullptr;
  }
  auto state = std::make_unique<PerContext>(owner, context);
  PerContext* raw = state.get();
  context->SetUserData(UserDataKey(), std::move(state));
  owner->contexts_.insert(raw);
  return raw;
}

// static
WebRtcDiagnosticsImpl::PerContext*
WebRtcDiagnosticsImpl::PerContext::GetIfExists(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context) {
    return nullptr;
  }
  return static_cast<PerContext*>(context->GetUserData(UserDataKey()));
}

// static
WebRtcDiagnosticsImpl* WebRtcDiagnosticsImpl::GetInstance() {
  static base::NoDestructor<WebRtcDiagnosticsImpl> instance;
  return instance.get();
}

WebRtcDiagnosticsImpl::WebRtcDiagnosticsImpl() = default;
WebRtcDiagnosticsImpl::~WebRtcDiagnosticsImpl() = default;

WebRtcDiagnosticsImpl::PeerConnectionInfo::PeerConnectionInfo() = default;
WebRtcDiagnosticsImpl::PeerConnectionInfo::PeerConnectionInfo(
    std::optional<url::Origin> origin,
    ChildProcessId render_process_id)
    : origin(origin), render_process_id(render_process_id) {}
WebRtcDiagnosticsImpl::PeerConnectionInfo::PeerConnectionInfo(
    const PeerConnectionInfo&) = default;
WebRtcDiagnosticsImpl::PeerConnectionInfo&
WebRtcDiagnosticsImpl::PeerConnectionInfo::operator=(
    const PeerConnectionInfo&) = default;
WebRtcDiagnosticsImpl::PeerConnectionInfo::~PeerConnectionInfo() = default;

void WebRtcDiagnosticsImpl::OnPerContextDestroyed(PerContext* state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  contexts_.erase(state);
  // The profile is gone, so it can no longer be capturing. Recompute the
  // WebRTCInternals registration and the stats timer without it.
  UpdateInternalsRegistration(nullptr);
  UpdateStatsTimer();
}

WebRtcDiagnostics::StartCaptureResult
WebRtcDiagnosticsImpl::StartCaptureForClient(BrowserContext* context,
                                             std::string_view client_id,
                                             std::vector<url::Origin> origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Prevent abuse by capping origins list length to kMaxFilterOrigins.
  if (origins.size() > WebRtcDiagnostics::kMaxFilterOrigins) {
    return StartCaptureResult::kTooManyOrigins;
  }

  for (const auto& origin : origins) {
    if (origin.opaque()) {
      return StartCaptureResult::kInvalidOrigin;
    }
  }

  PerContext* state = PerContext::GetOrCreate(this, context);
  if (!state) {
    return StartCaptureResult::kUnavailable;
  }

  if (state->clients_.find(client_id) != state->clients_.end()) {
    return StartCaptureResult::kAlreadyCapturing;
  }

  const bool context_was_idle = !state->has_clients();
  state->clients_.emplace(client_id, std::move(origins));

  if (context_was_idle) {
    // Start from a clean cache for this profile so a session never sees data
    // captured before it began.
    state->get_user_media_requests_.clear();
    state->peer_connection_data_.clear();
    state->pc_metadata_.clear();
    state->dropped_log_entries_ = 0;
    state->dropped_stats_entries_ = 0;
    state->dropped_media_entries_ = 0;
    state->last_truncation_notification_ = base::TimeTicks();
  }

  UpdateInternalsRegistration(context_was_idle ? state : nullptr);
  UpdateStatsTimer();
  return StartCaptureResult::kSuccess;
}

WebRtcDiagnostics::StopCaptureResult
WebRtcDiagnosticsImpl::StopCaptureForClient(BrowserContext* context,
                                            std::string_view client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return StopCaptureResult::kNotCapturing;
  }

  auto it = state->clients_.find(client_id);
  if (it == state->clients_.end()) {
    return StopCaptureResult::kNotCapturing;
  }

  // Copy the id: erasing the entry invalidates the string_view that callers
  // may have obtained from the map.
  const std::string stopped_client_id = it->first;
  state->clients_.erase(it);

  for (auto& observer : state->observers_) {
    observer.OnCaptureStopped(stopped_client_id);
  }

  if (!state->has_clients()) {
    // Nothing can read this profile's cache once its last session ends:
    // GetSnapshot rejects any client that is no longer in `clients`.
    state->get_user_media_requests_.clear();
    state->peer_connection_data_.clear();
    state->pc_metadata_.clear();
    state->dropped_log_entries_ = 0;
    state->dropped_stats_entries_ = 0;
    state->dropped_media_entries_ = 0;
    state->last_truncation_notification_ = base::TimeTicks();
  }

  UpdateInternalsRegistration(nullptr);
  UpdateStatsTimer();
  return StopCaptureResult::kSuccess;
}

void WebRtcDiagnosticsImpl::UpdateInternalsRegistration(
    PerContext* newly_capturing) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (!webrtc_internals) {
    return;
  }

  const bool want = std::ranges::any_of(
      contexts_, [](PerContext* state) { return state->has_clients(); });

  if (want != registered_with_internals_) {
    registered_with_internals_ = want;
    if (want) {
      webrtc_internals->AddObserver(this);
    } else {
      webrtc_internals->RemoveObserver(this);
      return;
    }
  }

  // Replay the state WebRTCInternals already holds so that a session started
  // while calls are in progress sees them. This is needed for every context
  // that has just gained its first client, not only for the first context to
  // register, and it is safe to repeat because ingest is idempotent.
  if (newly_capturing && registered_with_internals_) {
    webrtc_internals->UpdateObserver(this);
  }
}

void WebRtcDiagnosticsImpl::UpdateStatsTimer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool want = std::ranges::any_of(contexts_, [](PerContext* state) {
    return !state->pc_metadata_.empty();
  });
  if (want && !stats_timer_.IsRunning()) {
    stats_timer_.Start(FROM_HERE, kStatsPollInterval, this,
                       &WebRtcDiagnosticsImpl::RequestStats);
  } else if (!want && stats_timer_.IsRunning()) {
    stats_timer_.Stop();
  }
}

void WebRtcDiagnosticsImpl::RequestStats() {
  for (auto* host : PeerConnectionTrackerHost::GetAllHosts()) {
    host->GetStandardStats();
  }
}

WebRtcDiagnosticsImpl::PerContext* WebRtcDiagnosticsImpl::FindPerContextState(
    const base::DictValue& entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::optional<int> rid = entry.FindInt(kRidKey);
  if (!rid) {
    return nullptr;
  }
  RenderProcessHost* rph = RenderProcessHost::FromID(*rid);
  if (!rph) {
    return nullptr;
  }
  // Only contexts that already have state are considered: an event for a
  // profile that has never started a capture is not worth allocating for.
  return PerContext::GetIfExists(rph->GetBrowserContext());
}

void WebRtcDiagnosticsImpl::RebuildMetadataFor(PerContext* state,
                                               const base::DictValue& pc_dict) {
  std::optional<int> rid = pc_dict.FindInt(kRidKey);
  std::optional<int> lid = pc_dict.FindInt(kLidKey);
  const std::string* url_str = pc_dict.FindString(kUrlKey);
  if (rid && lid) {
    std::optional<url::Origin> origin;
    if (url_str) {
      origin = url::Origin::Create(GURL(*url_str));
    }
    state->pc_metadata_[MakePeerConnectionId(*rid, *lid)] =
        PeerConnectionInfo(origin, ChildProcessId::FromUnsafeValue(*rid));
  }
}

bool WebRtcDiagnosticsImpl::GetSnapshot(
    BrowserContext* context,
    std::string_view client_id,
    const std::vector<url::Origin>& origins,
    base::OnceCallback<void(base::Value)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (origins.size() > WebRtcDiagnostics::kMaxFilterOrigins) {
    return false;
  }

  for (const auto& origin : origins) {
    if (origin.opaque()) {
      return false;
    }
  }

  // A snapshot is only ever served to a client with an active session in this
  // profile, so that one extension cannot read what another caused to be
  // captured.
  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return false;
  }
  auto client = state->clients_.find(client_id);
  if (client == state->clients_.end()) {
    return false;
  }

  // The request may narrow the session's filter but never widen it. With an
  // unfiltered session the request's filter applies as-is; otherwise only the
  // origins present in both are readable.
  const std::vector<url::Origin>& session_origins = client->second;
  std::vector<url::Origin> effective;
  if (session_origins.empty()) {
    effective = origins;
  } else if (origins.empty()) {
    effective = session_origins;
  } else {
    for (const url::Origin& origin : session_origins) {
      if (MatchesFilter(origins, origin)) {
        effective.push_back(origin);
      }
    }
    // The two filters are disjoint, so nothing is readable. Fall through with
    // an empty result rather than treating it as unfiltered.
    if (effective.empty()) {
      base::DictValue empty_root;
      empty_root.Set(kGetUserMediaOutKey, base::ListValue());
      empty_root.Set(kPeerConnectionsOutKey, base::DictValue());
      empty_root.Set(kUserAgentOutKey,
                     GetContentClient()->browser()->GetUserAgent());
      empty_root.Set(kUserAgentDataOutKey, base::ListValue());
      std::move(callback).Run(base::Value(std::move(empty_root)));
      return true;
    }
  }

  base::DictValue root;

  // Add getUserMedia log.
  base::ListValue filtered_list;
  for (const auto& item : state->get_user_media_requests_) {
    const base::DictValue* item_dict = item.GetIfDict();
    if (item_dict && EntryMatchesFilter(*item_dict, kOriginKey, effective)) {
      filtered_list.Append(item.Clone());
    }
  }
  root.Set(kGetUserMediaOutKey, std::move(filtered_list));

  // Add peer connection data.
  base::DictValue filtered_dict;
  for (const auto& pc_value : state->peer_connection_data_) {
    const base::DictValue* pc_dict = pc_value.GetIfDict();
    if (!pc_dict) {
      continue;
    }
    std::optional<int> rid = pc_dict->FindInt(kRidKey);
    std::optional<int> lid = pc_dict->FindInt(kLidKey);
    if (!rid || !lid) {
      continue;
    }
    if (EntryMatchesFilter(*pc_dict, kUrlKey, effective)) {
      filtered_dict.Set(MakePeerConnectionId(*rid, *lid), pc_dict->Clone());
    }
  }
  root.Set(kPeerConnectionsOutKey, std::move(filtered_dict));

  // Add User Agent.
  root.Set(kUserAgentOutKey, GetContentClient()->browser()->GetUserAgent());

  // Add User Agent Data.
  const blink::UserAgentMetadata ua_metadata =
      GetContentClient()->browser()->GetUserAgentMetadata();
  base::ListValue ua_data_list;
  const auto& brands = !ua_metadata.brand_full_version_list.empty()
                           ? ua_metadata.brand_full_version_list
                           : ua_metadata.brand_version_list;
  for (const auto& brand_version : brands) {
    base::DictValue brand_dict;
    brand_dict.Set("brand", brand_version.brand);
    brand_dict.Set("version", brand_version.version);
    ua_data_list.Append(std::move(brand_dict));
  }
  root.Set(kUserAgentDataOutKey, std::move(ua_data_list));

  std::move(callback).Run(base::Value(std::move(root)));
  return true;
}

bool WebRtcDiagnosticsImpl::IsCapturingForClient(BrowserContext* context,
                                                 std::string_view client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PerContext* state = PerContext::GetIfExists(context);
  return state && state->clients_.find(client_id) != state->clients_.end();
}

std::vector<std::string> WebRtcDiagnosticsImpl::GetCapturingClients(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<std::string> clients;
  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return clients;
  }
  clients.reserve(state->clients_.size());
  for (const auto& pair : state->clients_) {
    clients.push_back(pair.first);
  }
  return clients;
}

void WebRtcDiagnosticsImpl::AddObserver(BrowserContext* context,
                                        WebRtcDiagnostics::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (PerContext* state = PerContext::GetOrCreate(this, context)) {
    state->observers_.AddObserver(observer);
  }
}

void WebRtcDiagnosticsImpl::RemoveObserver(
    BrowserContext* context,
    WebRtcDiagnostics::Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (PerContext* state = PerContext::GetIfExists(context)) {
    state->observers_.RemoveObserver(observer);
  }
}

void WebRtcDiagnosticsImpl::NotifySnapshotTruncated(PerContext* state) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!state->last_truncation_notification_.is_null() &&
      now - state->last_truncation_notification_ < base::Seconds(5)) {
    return;
  }
  state->last_truncation_notification_ = now;

  for (auto& observer : state->observers_) {
    observer.OnSnapshotTruncated(state->dropped_log_entries_,
                                 state->dropped_stats_entries_,
                                 state->dropped_media_entries_);
  }
}

std::optional<std::vector<url::Origin>>
WebRtcDiagnosticsImpl::GetFilterOriginsForClient(BrowserContext* context,
                                                 std::string_view client_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return std::nullopt;
  }
  auto it = state->clients_.find(client_id);
  if (it == state->clients_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<url::Origin> WebRtcDiagnosticsImpl::GetOriginForPeerConnection(
    BrowserContext* context,
    std::string_view pc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return std::nullopt;
  }
  auto it = state->pc_metadata_.find(pc_id);
  if (it != state->pc_metadata_.end()) {
    return it->second.origin;
  }
  return std::nullopt;
}

std::optional<ChildProcessId>
WebRtcDiagnosticsImpl::GetRenderProcessIdForPeerConnection(
    BrowserContext* context,
    std::string_view pc_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PerContext* state = PerContext::GetIfExists(context);
  if (!state) {
    return std::nullopt;
  }
  auto it = state->pc_metadata_.find(pc_id);
  if (it != state->pc_metadata_.end() &&
      !it->second.render_process_id.is_null()) {
    return it->second.render_process_id;
  }
  return std::nullopt;
}

void WebRtcDiagnosticsImpl::OnUpdate(const std::string& event_name,
                                     const base::Value* event_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!event_data) {
    return;
  }

  if (event_name == kUpdateAllPeerConnections) {
    if (!event_data->is_list()) {
      DVLOG(1) << "update-all-peer-connections received non-list data";
      return;
    }
    // This event carries the complete current state, so it is authoritative:
    // every context's peer connection cache is rebuilt from the entries that
    // belong to it, and a context with no entries in the replay ends up empty.
    // Rebuilding rather than merging is also the only path that prunes peer
    // connections that have since closed.
    for (PerContext* state : contexts_) {
      state->peer_connection_data_.clear();
      state->pc_metadata_.clear();
    }
    for (const auto& item : event_data->GetList()) {
      const base::DictValue* item_dict = item.GetIfDict();
      if (!item_dict) {
        continue;
      }
      PerContext* state = FindPerContextState(*item_dict);
      if (!state) {
        continue;
      }
      state->peer_connection_data_.Append(item.Clone());
      RebuildMetadataFor(state, *item_dict);
    }
    UpdateStatsTimer();
    return;
  }

  if (!event_data->is_dict()) {
    DLOG(WARNING) << event_name << " received non-dict data";
    return;
  }
  const base::DictValue& dict = event_data->GetDict();
  PerContext* state = FindPerContextState(dict);
  if (!state) {
    // The renderer is gone or the profile has no state for this feature, so
    // the entry cannot be attributed. Dropping it is the only safe option:
    // showing it to an arbitrary profile would be a cross-profile leak.
    return;
  }

  const int initial_dropped_log = state->dropped_log_entries_;
  const int initial_dropped_stats = state->dropped_stats_entries_;
  const int initial_dropped_media = state->dropped_media_entries_;

  if (event_name == kAddMedia) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> request_id = dict.FindInt(kRequestIdKey);

    // Upsert rather than append, so that replaying the WebRTCInternals backlog
    // into a context that has just started capturing cannot duplicate entries.
    base::Value* existing = nullptr;
    if (rid && request_id) {
      for (auto& item : state->get_user_media_requests_) {
        if (!item.is_dict()) {
          continue;
        }
        if (item.GetDict().FindInt(kRidKey) == rid &&
            item.GetDict().FindInt(kRequestIdKey) == request_id) {
          existing = &item;
          break;
        }
      }
    }

    if (existing) {
      *existing = event_data->Clone();
    } else {
      state->get_user_media_requests_.Append(event_data->Clone());
      if (state->get_user_media_requests_.size() > kMaxMediaEntries) {
        // O(n) shift on overflow. Acceptable at the current cap (1000); if the
        // cap is raised significantly, migrate to base::circular_deque.
        state->get_user_media_requests_.erase(
            state->get_user_media_requests_.begin());
        state->dropped_media_entries_++;
      }
    }
  } else if (event_name == kUpdateMedia) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> request_id = dict.FindInt(kRequestIdKey);

    if (rid && request_id) {
      for (auto& item : state->get_user_media_requests_) {
        if (!item.is_dict()) {
          continue;
        }
        if (item.GetDict().FindInt(kRidKey) != rid ||
            item.GetDict().FindInt(kRequestIdKey) != request_id) {
          continue;
        }
        for (std::string_view key :
             {kAudioKey, kVideoKey, kAudioTrackInfoKey, kVideoTrackInfoKey,
              kErrorKey, kErrorMessageKey}) {
          if (const std::string* value = dict.FindString(key)) {
            item.GetDict().Set(key, *value);
          }
        }
        break;
      }
    } else {
      DLOG(WARNING) << "update-media missing rid or request_id";
    }
  } else if (event_name == kAddPeerConnection) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> lid = dict.FindInt(kLidKey);

    if (rid && lid) {
      const std::string id = MakePeerConnectionId(*rid, *lid);

      // Upsert, for the same reason as add-media.
      bool found = false;
      for (auto& pc_value : state->peer_connection_data_) {
        if (!pc_value.is_dict()) {
          continue;
        }
        if (pc_value.GetDict().FindInt(kRidKey) == rid &&
            pc_value.GetDict().FindInt(kLidKey) == lid) {
          pc_value = event_data->Clone();
          found = true;
          break;
        }
      }
      if (!found) {
        state->peer_connection_data_.Append(event_data->Clone());
      }
      RebuildMetadataFor(state, dict);
      UpdateStatsTimer();

      for (auto& observer : state->observers_) {
        observer.OnPeerConnectionAdded(id, *event_data);
      }
    } else {
      DLOG(WARNING) << "add-peer-connection missing rid or lid";
    }
  } else if (event_name == kRemovePeerConnection) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> lid = dict.FindInt(kLidKey);
    if (rid && lid) {
      const std::string id = MakePeerConnectionId(*rid, *lid);

      for (auto& observer : state->observers_) {
        observer.OnPeerConnectionRemoved(id);
      }

      // Drop the closed connection's data as well as its metadata. Retaining
      // it would grow without bound over a long session and would keep SDP and
      // ICE candidates for calls that have already ended.
      state->peer_connection_data_.EraseIf([&rid, &lid](const base::Value& v) {
        return v.is_dict() && v.GetDict().FindInt(kRidKey) == rid &&
               v.GetDict().FindInt(kLidKey) == lid;
      });
      state->pc_metadata_.erase(id);
      UpdateStatsTimer();
    } else {
      DLOG(WARNING) << "remove-peer-connection missing rid or lid";
    }
  } else if (event_name == kUpdatePeerConnection) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> lid = dict.FindInt(kLidKey);
    const std::string* type = dict.FindString(kTypeKey);
    const std::string* value = dict.FindString(kValueKey);
    const std::string* time_str = dict.FindString(kTimeKey);
    std::optional<double> timestamp = dict.FindDouble(kTimestampKey);

    if (rid && lid && type && value) {
      for (auto& pc_value : state->peer_connection_data_) {
        if (!pc_value.is_dict()) {
          continue;
        }
        if (pc_value.GetDict().FindInt(kRidKey) != rid ||
            pc_value.GetDict().FindInt(kLidKey) != lid) {
          continue;
        }
        base::ListValue* log_list = pc_value.GetDict().FindList(kLogKey);
        if (!log_list) {
          pc_value.GetDict().Set(kLogKey, base::ListValue());
          log_list = pc_value.GetDict().FindList(kLogKey);
        }

        base::DictValue log_entry;
        log_entry.Set(kTypeKey, *type);
        log_entry.Set(kValueKey, *value);
        if (time_str) {
          log_entry.Set(kTimeKey, *time_str);
        }
        if (timestamp) {
          log_entry.Set(kTimestampKey, *timestamp);
        }

        log_list->Append(std::move(log_entry));

        if (log_list->size() > kMaxLogEntries) {
          // O(n) shift on overflow. Acceptable at the current cap (1000); if
          // the cap is raised significantly, migrate to base::circular_deque.
          log_list->erase(log_list->begin());
          state->dropped_log_entries_++;
        }
        break;
      }
    } else {
      DLOG(WARNING) << "update-peer-connection missing required parameters";
    }
  } else if (event_name == kAddStandardStats) {
    std::optional<int> rid = dict.FindInt(kRidKey);
    std::optional<int> lid = dict.FindInt(kLidKey);
    const base::ListValue* reports = dict.FindList(kReportsKey);
    std::optional<double> timestamp = dict.FindDouble(kTimestampKey);

    if (rid && lid && reports) {
      for (auto& pc_value : state->peer_connection_data_) {
        if (!pc_value.is_dict()) {
          continue;
        }
        if (pc_value.GetDict().FindInt(kRidKey) != rid ||
            pc_value.GetDict().FindInt(kLidKey) != lid) {
          continue;
        }
        base::ListValue* stats_list = pc_value.GetDict().FindList(kStatsKey);
        if (!stats_list) {
          pc_value.GetDict().Set(kStatsKey, base::ListValue());
          stats_list = pc_value.GetDict().FindList(kStatsKey);
        }

        base::DictValue stats_entry;
        if (timestamp) {
          stats_entry.Set(kTimestampKey, *timestamp);
        }
        stats_entry.Set(kReportsKey, reports->Clone());

        stats_list->Append(std::move(stats_entry));

        if (stats_list->size() > kMaxStatsEntries) {
          // O(n) shift on overflow. Acceptable at the current cap (1000); if
          // the cap is raised significantly, migrate to base::circular_deque.
          stats_list->erase(stats_list->begin());
          state->dropped_stats_entries_++;
        }
        break;
      }
    } else {
      DVLOG(1) << "add-standard-stats missing rid, lid, or reports";
    }
  }

  if (state->dropped_log_entries_ != initial_dropped_log ||
      state->dropped_stats_entries_ != initial_dropped_stats ||
      state->dropped_media_entries_ != initial_dropped_media) {
    NotifySnapshotTruncated(state);
  }
}

void WebRtcDiagnosticsImpl::ResetForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Copy first: clearing a context's state does not destroy the PerContext, but
  // callers may destroy contexts between tests.
  std::vector<PerContext*> contexts(contexts_.begin(), contexts_.end());
  for (PerContext* state : contexts) {
    state->clients_.clear();
    state->pc_metadata_.clear();
    state->get_user_media_requests_.clear();
    state->peer_connection_data_.clear();
    state->dropped_log_entries_ = 0;
    state->dropped_stats_entries_ = 0;
    state->dropped_media_entries_ = 0;
    state->last_truncation_notification_ = base::TimeTicks();
  }
  stats_timer_.Stop();
  if (registered_with_internals_) {
    if (WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance()) {
      webrtc_internals->RemoveObserver(this);
    }
    registered_with_internals_ = false;
  }
}

}  // namespace content
