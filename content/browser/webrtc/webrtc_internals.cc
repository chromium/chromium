// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/webrtc/webrtc_internals_connections_observer.h"
#include "content/browser/webrtc/webrtc_internals_ui_observer.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webrtc_event_logger.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_debug_recording_session.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/cpp/debug_recording_session_factory.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

using base::ProcessId;
using std::string;

namespace content {

namespace {

const base::FilePath::CharType kEventLogFilename[] =
    FILE_PATH_LITERAL("event_log");

constexpr char kGetUserMedia[] = "getUserMedia";
constexpr char kGetDisplayMedia[] = "getDisplayMedia";

// This is intended to limit DoS attacks against the browser process consisting
// of many getUserMedia()/getDisplayMedia() calls. See https://crbug.com/804440.
const size_t kMaxMediaEntries = 1000;

// Makes sure that |dict| has a List under path "log".
base::Value::List& EnsureLogList(base::Value::Dict& dict) {
  base::Value::List* log = dict.FindList("log");
  if (log)
    return *log;
  return dict.Set("log", base::Value::List())->GetList();
}

// Removes the log entry associated with a given record.
void FreeLogList(base::Value* value) {
  DCHECK(value->is_dict());
  value->GetDict().Remove("log");
}

}  // namespace

WebRTCInternals* WebRTCInternals::g_webrtc_internals = nullptr;

WebRTCInternals::PendingUpdate::PendingUpdate(const std::string& event_name,
                                              base::Value event_data)
    : event_name_(event_name), event_data_(std::move(event_data)) {}

WebRTCInternals::PendingUpdate::PendingUpdate(PendingUpdate&& other)
    : event_name_(other.event_name_),
      event_data_(std::move(other.event_data_)) {}

WebRTCInternals::PendingUpdate::~PendingUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

const std::string& WebRTCInternals::PendingUpdate::event_name() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return event_name_;
}

const base::Value* WebRTCInternals::PendingUpdate::event_data() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return event_data_.is_none() ? nullptr : &event_data_;
}

WebRTCInternals::WebRTCInternals() : WebRTCInternals(500, true) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebRTCInternals::WebRTCInternals(int aggregate_updates_ms,
                                 bool should_block_power_saving)
    : peer_connection_data_(base::Value::List()),
      selection_type_(SelectionType::kAudioDebugRecordings),
      command_line_derived_logging_path_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
              switches::kWebRtcLocalEventLogging)),
      event_log_recordings_(false),
      num_connected_connections_(0),
      should_block_power_saving_(should_block_power_saving),
      aggregate_updates_ms_(aggregate_updates_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_internals);

  audio_debug_recordings_file_path_ =
      GetContentClient()->browser()->GetDefaultDownloadDirectory();
  event_log_recordings_file_path_ = audio_debug_recordings_file_path_;

  if (audio_debug_recordings_file_path_.empty()) {
    // In this case the default path (|audio_debug_recordings_file_path_|) will
    // be empty and the platform default path will be used in the file dialog
    // (with no default file name). See SelectFileDialog::SelectFile. On Android
    // where there's no dialog we'll fail to open the file.
    VLOG(1) << "Could not get the download directory.";
  } else {
    audio_debug_recordings_file_path_ =
        audio_debug_recordings_file_path_.Append(
            FILE_PATH_LITERAL("audio_debug"));
    event_log_recordings_file_path_ =
        event_log_recordings_file_path_.Append(kEventLogFilename);
  }

  // Allow command-line based setting of (local) WebRTC event logging.
  if (!command_line_derived_logging_path_.empty()) {
    const base::FilePath local_logs_path =
        command_line_derived_logging_path_.Append(kEventLogFilename);
    WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
    if (logger) {
      logger->EnableLocalLogging(local_logs_path);
    }
    // For clarity's sake, though these aren't supposed to be regarded now:
    event_log_recordings_ = true;
    event_log_recordings_file_path_.clear();
  }

  g_webrtc_internals = this;
}

WebRTCInternals::~WebRTCInternals() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_webrtc_internals);
  g_webrtc_internals = nullptr;

  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

WebRTCInternals* WebRTCInternals::CreateSingletonInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_internals);
  g_webrtc_internals = new WebRTCInternals;
  return g_webrtc_internals;
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  // TODO(crbug.com/40837773): DCHECK calling from UI thread.
  // Currently, some unit tests call this from outside of the UI thread,
  // but that's not a real issue as these tests neglect setting
  // `g_webrtc_internals` to begin with, and therefore just ignore it.
  DCHECK(!g_webrtc_internals || BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_webrtc_internals;
}

void WebRTCInternals::OnPeerConnectionAdded(GlobalRenderFrameHostId frame_id,
                                            int lid,
                                            ProcessId pid,
                                            const string& url,
                                            const string& rtc_configuration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(tommi): Consider changing this design so that webrtc-internals has
  // minimal impact if chrome://webrtc-internals isn't open.

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("lid", lid);
  dict.Set("pid", static_cast<int>(pid));
  dict.Set("rtcConfiguration", rtc_configuration);
  dict.Set("url", url);
  dict.Set("isOpen", true);
  dict.Set("connected", false);

  if (!observers_.empty())
    SendUpdate("add-peer-connection", dict.Clone());

  peer_connection_data().Append(std::move(dict));

  if (render_process_id_set_.insert(frame_id.child_id).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(frame_id.child_id);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::OnPeerConnectionRemoved(GlobalRenderFrameHostId frame_id,
                                              int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = FindRecord(frame_id, lid);
  if (it != peer_connection_data().end()) {
    MaybeClosePeerConnection(*it);
    peer_connection_data().erase(it);
  }

  if (!observers_.empty()) {
    base::Value::Dict id;
    id.Set("rid", frame_id.child_id);
    id.Set("lid", lid);
    SendUpdate("remove-peer-connection", std::move(id));
  }
}

void WebRTCInternals::OnPeerConnectionUpdated(GlobalRenderFrameHostId frame_id,
                                              int lid,
                                              const string& type,
                                              const string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto it = FindRecord(frame_id, lid);
  if (it == peer_connection_data().end())
    return;

  if (type == "iceconnectionstatechange") {
    if (value == "connected" || value == "checking" || value == "completed") {
      MaybeMarkPeerConnectionAsConnected(*it);
    } else if (value == "failed" || value == "disconnected" ||
               value == "closed" || value == "new") {
      MaybeMarkPeerConnectionAsNotConnected(*it);
    }
  } else if (type == "close") {
    MaybeClosePeerConnection(*it);
  } else if (type == "setConfiguration") {
    // Update the configuration we have for this connection.
    it->GetDict().Set("rtcConfiguration", value);
  }

  // Don't update entries if there aren't any observers.
  if (observers_.empty())
    return;

  base::Value::Dict log_entry;

  double epoch_time = base::Time::Now().InMillisecondsFSinceUnixEpoch();
  string time = base::NumberToString(epoch_time);
  log_entry.Set("time", time);
  log_entry.Set("type", type);
  log_entry.Set("value", value);

  base::Value::Dict update;
  update.Set("rid", frame_id.child_id);
  update.Set("lid", lid);
  update.Merge(log_entry.Clone());

  SendUpdate("update-peer-connection", std::move(update));

  // Append the update to the end of the log.
  EnsureLogList(it->GetDict()).Append(std::move(log_entry));
}

void WebRTCInternals::OnAddStandardStats(GlobalRenderFrameHostId frame_id,
                                         int lid,
                                         base::Value::List value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (observers_.empty())
    return;

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("lid", lid);

  dict.Set("reports", std::move(value));

  SendUpdate("add-standard-stats", std::move(dict));
}

void WebRTCInternals::OnAddLegacyStats(GlobalRenderFrameHostId frame_id,
                                       int lid,
                                       base::Value::List value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (observers_.empty())
    return;

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("lid", lid);

  dict.Set("reports", std::move(value));

  SendUpdate("add-legacy-stats", std::move(dict));
}

void WebRTCInternals::OnGetMedia(const std::string& request_type,
                                 GlobalRenderFrameHostId frame_id,
                                 base::ProcessId pid,
                                 int request_id,
                                 bool audio,
                                 bool video,
                                 const std::string& audio_constraints,
                                 const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_user_media_requests_.size() >= kMaxMediaEntries) {
    LOG(WARNING) << "Maximum number of tracked getUserMedia/getDisplayMedia "
                    "requests reached in webrtc-internals.";
    return;
  }

  RenderFrameHost* rfh = RenderFrameHost::FromID(frame_id);
  // Frame may be gone (and does not exist in tests).
  std::string origin = rfh ? rfh->GetLastCommittedOrigin().Serialize() : "";

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("pid", static_cast<int>(pid));
  dict.Set("request_id", request_id);
  dict.Set("request_type", request_type);
  dict.Set("origin", origin);
  dict.Set("timestamp", base::Time::Now().InMillisecondsFSinceUnixEpoch());
  if (audio)
    dict.Set("audio", audio_constraints);
  if (video)
    dict.Set("video", video_constraints);

  if (!observers_.empty())
    SendUpdate("add-media", dict.Clone());

  get_user_media_requests_.Append(std::move(dict));

  if (render_process_id_set_.insert(frame_id.child_id).second) {
    RenderProcessHost* rph = RenderProcessHost::FromID(frame_id.child_id);
    if (rph)
      rph->AddObserver(this);
  }
}

void WebRTCInternals::OnGetMediaSuccess(const std::string& request_type,
                                        GlobalRenderFrameHostId frame_id,
                                        base::ProcessId pid,
                                        int request_id,
                                        const std::string& stream_id,
                                        const std::string& audio_track_info,
                                        const std::string& video_track_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_user_media_requests_.size() >= kMaxMediaEntries) {
    LOG(WARNING) << "Maximum number of tracked getUserMedia/getDisplayMedia "
                    "requests reached in webrtc-internals.";
    return;
  }

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("pid", static_cast<int>(pid));
  dict.Set("request_id", request_id);
  dict.Set("request_type", request_type);
  dict.Set("timestamp", base::Time::Now().InMillisecondsFSinceUnixEpoch());
  dict.Set("stream_id", stream_id);
  if (!audio_track_info.empty())
    dict.Set("audio_track_info", audio_track_info);
  if (!video_track_info.empty())
    dict.Set("video_track_info", video_track_info);

  if (!observers_.empty())
    SendUpdate("update-media", dict.Clone());

  get_user_media_requests_.Append(std::move(dict));

  if (render_process_id_set_.insert(frame_id.child_id).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(frame_id.child_id);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::OnGetMediaFailure(const std::string& request_type,
                                        GlobalRenderFrameHostId frame_id,
                                        base::ProcessId pid,
                                        int request_id,
                                        const std::string& error,
                                        const std::string& error_message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_user_media_requests_.size() >= kMaxMediaEntries) {
    LOG(WARNING) << "Maximum number of tracked /getDisplayMedia "
                    "requests reached in webrtc-internals.";
    return;
  }

  base::Value::Dict dict;
  dict.Set("rid", frame_id.child_id);
  dict.Set("pid", static_cast<int>(pid));
  dict.Set("request_id", request_id);
  dict.Set("request_type", request_type);
  dict.Set("timestamp", base::Time::Now().InMillisecondsFSinceUnixEpoch());
  dict.Set("error", error);
  dict.Set("error_message", error_message);

  if (!observers_.empty())
    SendUpdate("update-media", dict.Clone());

  get_user_media_requests_.Append(std::move(dict));

  if (render_process_id_set_.insert(frame_id.child_id).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(frame_id.child_id);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::OnGetUserMedia(GlobalRenderFrameHostId frame_id,
                                     base::ProcessId pid,
                                     int request_id,
                                     bool audio,
                                     bool video,
                                     const std::string& audio_constraints,
                                     const std::string& video_constraints) {
  OnGetMedia(kGetUserMedia, frame_id, pid, request_id, audio, video,
             audio_constraints, video_constraints);
}

void WebRTCInternals::OnGetUserMediaSuccess(
    GlobalRenderFrameHostId frame_id,
    base::ProcessId pid,
    int request_id,
    const std::string& stream_id,
    const std::string& audio_track_info,
    const std::string& video_track_info) {
  OnGetMediaSuccess(kGetUserMedia, frame_id, pid, request_id, stream_id,
                    audio_track_info, video_track_info);
}

void WebRTCInternals::OnGetUserMediaFailure(GlobalRenderFrameHostId frame_id,
                                            base::ProcessId pid,
                                            int request_id,
                                            const std::string& error,
                                            const std::string& error_message) {
  OnGetMediaFailure(kGetUserMedia, frame_id, pid, request_id, error,
                    error_message);
}

void WebRTCInternals::OnGetDisplayMedia(GlobalRenderFrameHostId frame_id,
                                        base::ProcessId pid,
                                        int request_id,
                                        bool audio,
                                        bool video,
                                        const std::string& audio_constraints,
                                        const std::string& video_constraints) {
  OnGetMedia(kGetDisplayMedia, frame_id, pid, request_id, audio, video,
             audio_constraints, video_constraints);
}

void WebRTCInternals::OnGetDisplayMediaSuccess(
    GlobalRenderFrameHostId frame_id,
    base::ProcessId pid,
    int request_id,
    const std::string& stream_id,
    const std::string& audio_track_info,
    const std::string& video_track_info) {
  OnGetMediaSuccess(kGetDisplayMedia, frame_id, pid, request_id, stream_id,
                    audio_track_info, video_track_info);
}

void WebRTCInternals::OnGetDisplayMediaFailure(
    GlobalRenderFrameHostId frame_id,
    base::ProcessId pid,
    int request_id,
    const std::string& error,
    const std::string& error_message) {
  OnGetMediaFailure(kGetDisplayMedia, frame_id, pid, request_id, error,
                    error_message);
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
  if (!observers_.empty())
    return;

  // Disables event log and audio debug recordings if enabled and the last
  // webrtc-internals page is going away.
  DisableAudioDebugRecordings();
  if (CanToggleEventLogRecordings()) {
    // Do not disable event log recording when the browser was started
    // with the flag to enable the recordings on the command line.
    DisableLocalEventLogRecordings();
  }

  // TODO(tommi): Consider removing all the peer_connection_data().
  for (auto& dictionary : peer_connection_data())
    FreeLogList(&dictionary);
}

void WebRTCInternals::AddConnectionsObserver(
    WebRtcInternalsConnectionsObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  connections_observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveConnectionsObserver(
    WebRtcInternalsConnectionsObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  connections_observers_.RemoveObserver(observer);
}

void WebRTCInternals::UpdateObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (peer_connection_data().size() > 0)
    observer->OnUpdate("update-all-peer-connections", &peer_connection_data_);

  for (const auto& request : get_user_media_requests_) {
    // If there is a stream_id key or an error key this is an update.
    if (request.GetDict().FindString("stream_id") ||
        request.GetDict().FindString("error")) {
      observer->OnUpdate("update-media", &request);
    } else {
      observer->OnUpdate("add-media", &request);
    }
  }
}

void WebRTCInternals::EnableAudioDebugRecordings(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_ANDROID)
  EnableAudioDebugRecordingsOnAllRenderProcessHosts();
#else
  if (select_file_dialog_) {
    return;
  }
  selection_type_ = SelectionType::kAudioDebugRecordings;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      audio_debug_recordings_file_path_, nullptr, 0,
      base::FilePath::StringType(), web_contents->GetTopLevelNativeWindow());
#endif
}

void WebRTCInternals::DisableAudioDebugRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!audio_debug_recording_session_)
    return;
  audio_debug_recording_session_.reset();

  // Tear down the dialog since the user has unchecked the audio debug
  // recordings box.
  select_file_dialog_ = nullptr;

  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->DisableAudioDebugRecordings();
  }
}

bool WebRTCInternals::IsAudioDebugRecordingsEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !!audio_debug_recording_session_;
}

const base::FilePath& WebRTCInternals::GetAudioDebugRecordingsFilePath() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return audio_debug_recordings_file_path_;
}

void WebRTCInternals::EnableLocalEventLogRecordings(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);
  DCHECK(CanToggleEventLogRecordings());

#if BUILDFLAG(IS_ANDROID)
  WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->EnableLocalLogging(event_log_recordings_file_path_);
  }
#else
  if (select_file_dialog_) {
    return;
  }

  selection_type_ = SelectionType::kRtcEventLogs;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      GetContentClient()->browser()->CreateSelectFilePolicy(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      event_log_recordings_file_path_, nullptr, 0, FILE_PATH_LITERAL(""),
      web_contents->GetTopLevelNativeWindow());
#endif
}

void WebRTCInternals::DisableLocalEventLogRecordings() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  event_log_recordings_ = false;
  // Tear down the dialog since the user has unchecked the event log checkbox.
  select_file_dialog_ = nullptr;
  DCHECK(CanToggleEventLogRecordings());
  WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->DisableLocalLogging();
  }
}

bool WebRTCInternals::IsEventLogRecordingsEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return event_log_recordings_;
}

bool WebRTCInternals::CanToggleEventLogRecordings() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return command_line_derived_logging_path_.empty();
}

void WebRTCInternals::SendUpdate(const std::string& event_name,
                                 base::Value event_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!observers_.empty());

  bool queue_was_empty = pending_updates_.empty();
  pending_updates_.push(PendingUpdate(event_name, std::move(event_data)));

  if (queue_was_empty) {
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebRTCInternals::ProcessPendingUpdates,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(aggregate_updates_ms_));
  }
}

void WebRTCInternals::SendUpdate(const std::string& event_name,
                                 base::Value::Dict event_data) {
  SendUpdate(event_name, base::Value(std::move(event_data)));
}

void WebRTCInternals::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnRendererExit(host->GetID());
  render_process_id_set_.erase(host->GetID());
  host->RemoveObserver(this);
}

void WebRTCInternals::FileSelected(const ui::SelectedFileInfo& file,
                                   int /* unused_index */) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (selection_type_) {
    case SelectionType::kRtcEventLogs: {
      event_log_recordings_file_path_ = file.path();
      event_log_recordings_ = true;
      WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
      if (logger) {
        logger->EnableLocalLogging(file.path());
      }
      break;
    }
    case SelectionType::kAudioDebugRecordings: {
      audio_debug_recordings_file_path_ = file.path();
      EnableAudioDebugRecordingsOnAllRenderProcessHosts();
      break;
    }
    default: {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

void WebRTCInternals::FileSelectionCanceled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (selection_type_) {
    case SelectionType::kRtcEventLogs:
      SendUpdate("event-log-recordings-file-selection-cancelled",
                 base::Value());
      break;
    case SelectionType::kAudioDebugRecordings:
      SendUpdate("audio-debug-recordings-file-selection-cancelled",
                 base::Value());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  select_file_dialog_ = nullptr;
}

void WebRTCInternals::OnRendererExit(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterates from the end of the list to remove the PeerConnections created
  // by the exiting renderer.
  for (int i = peer_connection_data().size() - 1; i >= 0; --i) {
    DCHECK(peer_connection_data()[i].is_dict());

    std::optional<int> this_rid, this_lid;
    this_rid = peer_connection_data()[i].GetDict().FindInt("rid");
    this_lid = peer_connection_data()[i].GetDict().FindInt("lid");

    if (this_rid.value_or(0) == render_process_id) {
      if (!observers_.empty()) {
        base::Value::Dict update;
        update.Set("rid", this_rid.value_or(0));
        update.Set("lid", this_lid.value_or(0));
        SendUpdate("remove-peer-connection", std::move(update));
      }
      MaybeClosePeerConnection(peer_connection_data()[i]);
      peer_connection_data().erase(peer_connection_data().begin() + i);
    }
  }
  UpdateWakeLock();

  bool found_any = false;
  // Iterates from the end of the list to remove the getUserMedia requests
  // created by the exiting renderer.
  for (int i = get_user_media_requests_.size() - 1; i >= 0; --i) {
    DCHECK(get_user_media_requests_[i].is_dict());

    std::optional<int> this_rid =
        get_user_media_requests_[i].GetDict().FindInt("rid");

    if (this_rid.value_or(0) == render_process_id) {
      get_user_media_requests_.erase(get_user_media_requests_.begin() + i);
      found_any = true;
    }
  }

  if (found_any && !observers_.empty()) {
    base::Value::Dict update;
    update.Set("rid", render_process_id);
    SendUpdate("remove-media-for-renderer", std::move(update));
  }
}

void WebRTCInternals::EnableAudioDebugRecordingsOnAllRenderProcessHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!audio_debug_recording_session_);
  mojo::PendingRemote<audio::mojom::DebugRecording> debug_recording;
  content::GetAudioService().BindDebugRecording(
      debug_recording.InitWithNewPipeAndPassReceiver());
  audio_debug_recording_session_ = audio::CreateAudioDebugRecordingSession(
      audio_debug_recordings_file_path_, std::move(debug_recording));

  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->EnableAudioDebugRecordings(
        audio_debug_recordings_file_path_);
  }
}

void WebRTCInternals::MaybeClosePeerConnection(base::Value& record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::optional<bool> is_open = record.GetDict().FindBool("isOpen");
  DCHECK(is_open.has_value());
  if (!*is_open)
    return;

  record.GetDict().Set("isOpen", false);
  MaybeMarkPeerConnectionAsNotConnected(record);
}

void WebRTCInternals::MaybeMarkPeerConnectionAsConnected(base::Value& record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool was_connected = record.GetDict().FindBool("connected").value_or(true);
  if (!was_connected) {
    ++num_connected_connections_;
    record.GetDict().Set("connected", true);
    UpdateWakeLock();
    for (auto& observer : connections_observers_)
      observer.OnConnectionsCountChange(num_connected_connections_);
  }
}

void WebRTCInternals::MaybeMarkPeerConnectionAsNotConnected(
    base::Value& record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool was_connected = record.GetDict().FindBool("connected").value_or(false);
  if (was_connected) {
    record.GetDict().Set("connected", false);
    --num_connected_connections_;
    DCHECK_GE(num_connected_connections_, 0);
    UpdateWakeLock();
    for (auto& observer : connections_observers_)
      observer.OnConnectionsCountChange(num_connected_connections_);
  }
}

void WebRTCInternals::UpdateWakeLock() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!should_block_power_saving_)
    return;

  if (num_connected_connections_ == 0) {
    DVLOG(1)
        << ("Cancel the wake lock on application suspension since no "
            "PeerConnections are active anymore.");
    GetWakeLock()->CancelWakeLock();
  } else {
    DCHECK_GT(num_connected_connections_, 0);
    DVLOG(1) << ("Preventing the application from being suspended while one or "
                 "more PeerConnections are active.");
    GetWakeLock()->RequestWakeLock();
  }
}

device::mojom::WakeLock* WebRTCInternals::GetWakeLock() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
    GetDeviceService().BindWakeLockProvider(
        wake_lock_provider.BindNewPipeAndPassReceiver());
    wake_lock_provider->GetWakeLockWithoutContext(
        device::mojom::WakeLockType::kPreventAppSuspension,
        device::mojom::WakeLockReason::kOther,
        "WebRTC has active PeerConnections",
        wake_lock_.BindNewPipeAndPassReceiver());
  }
  return wake_lock_.get();
}

void WebRTCInternals::ProcessPendingUpdates() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!pending_updates_.empty()) {
    const auto& update = pending_updates_.front();
    for (auto& observer : observers_)
      observer.OnUpdate(update.event_name(), update.event_data());
    pending_updates_.pop();
  }
}

base::Value::List::iterator WebRTCInternals::FindRecord(
    GlobalRenderFrameHostId frame_id,
    int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto it = peer_connection_data().begin();
       it != peer_connection_data().end(); ++it) {
    DCHECK(it->is_dict());

    int this_rid = it->GetDict().FindInt("rid").value_or(0);
    int this_lid = it->GetDict().FindInt("lid").value_or(0);

    if (this_rid == frame_id.child_id && this_lid == lid)
      return it;
  }
  return peer_connection_data().end();
}
}  // namespace content
