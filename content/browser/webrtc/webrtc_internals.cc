// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_internals.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/browser/webrtc/webrtc_internals_connections_observer.h"
#include "content/browser/webrtc/webrtc_internals_ui_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/system_connector.h"
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
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/shell_dialogs/select_file_policy.h"

using base::ProcessId;
using std::string;

namespace content {

namespace {

const base::FilePath::CharType kEventLogFilename[] =
    FILE_PATH_LITERAL("event_log");

// This is intended to limit DoS attacks against the browser process consisting
// of many getUserMedia() calls. See https://crbug.com/804440.
const size_t kMaxGetUserMediaEntries = 1000;

// Makes sure that |dict| has a ListValue under path "log".
base::ListValue* EnsureLogList(base::DictionaryValue* dict) {
  base::ListValue* log = nullptr;
  if (!dict->GetList("log", &log))
    log = dict->SetList("log", std::make_unique<base::ListValue>());
  return log;
}

// Removes the log entry associated with a given record.
void FreeLogList(base::Value* value) {
  DCHECK(value->is_dict());
  auto* dict = static_cast<base::DictionaryValue*>(value);
  dict->Remove("log", nullptr);
}

}  // namespace

WebRTCInternals* WebRTCInternals::g_webrtc_internals = nullptr;

WebRTCInternals::PendingUpdate::PendingUpdate(
    const char* command,
    std::unique_ptr<base::Value> value)
    : command_(command), value_(std::move(value)) {}

WebRTCInternals::PendingUpdate::PendingUpdate(PendingUpdate&& other)
    : command_(other.command_),
      value_(std::move(other.value_)) {}

WebRTCInternals::PendingUpdate::~PendingUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

const char* WebRTCInternals::PendingUpdate::command() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return command_;
}

const base::Value* WebRTCInternals::PendingUpdate::value() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return value_.get();
}

WebRTCInternals::WebRTCInternals() : WebRTCInternals(500, true) {}

WebRTCInternals::WebRTCInternals(int aggregate_updates_ms,
                                 bool should_block_power_saving)
    : selection_type_(SelectionType::kAudioDebugRecordings),
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
      logger->EnableLocalLogging(local_logs_path,
                                 base::OnceCallback<void(bool)>());
    }
    // For clarity's sake, though these aren't supposed to be regarded now:
    event_log_recordings_ = true;
    event_log_recordings_file_path_.clear();
  }

  g_webrtc_internals = this;
}

WebRTCInternals::~WebRTCInternals() {
  DCHECK(g_webrtc_internals);
  g_webrtc_internals = nullptr;
}

WebRTCInternals* WebRTCInternals::CreateSingletonInstance() {
  DCHECK(!g_webrtc_internals);
  g_webrtc_internals = new WebRTCInternals;
  return g_webrtc_internals;
}

WebRTCInternals* WebRTCInternals::GetInstance() {
  return g_webrtc_internals;
}

void WebRTCInternals::OnAddPeerConnection(int render_process_id,
                                          ProcessId pid,
                                          int lid,
                                          const string& url,
                                          const string& rtc_configuration,
                                          const string& constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(tommi): Consider changing this design so that webrtc-internals has
  // minimal impact if chrome://webrtc-internals isn't open.

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("rid", render_process_id);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);
  dict->SetString("rtcConfiguration", rtc_configuration);
  dict->SetString("constraints", constraints);
  dict->SetString("url", url);
  dict->SetBoolean("isOpen", true);
  dict->SetBoolean("connected", false);

  if (observers_.might_have_observers())
    SendUpdate("addPeerConnection", dict->CreateDeepCopy());

  peer_connection_data_.Append(std::move(dict));

  if (render_process_id_set_.insert(render_process_id).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::OnRemovePeerConnection(ProcessId pid, int lid) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  size_t index;
  base::DictionaryValue* dict = FindRecord(pid, lid, &index);
  if (dict) {
    MaybeClosePeerConnection(dict);
    peer_connection_data_.Remove(index, nullptr);
  }

  if (observers_.might_have_observers()) {
    std::unique_ptr<base::DictionaryValue> id(new base::DictionaryValue());
    id->SetInteger("pid", static_cast<int>(pid));
    id->SetInteger("lid", lid);
    SendUpdate("removePeerConnection", std::move(id));
  }
}

void WebRTCInternals::OnUpdatePeerConnection(
    ProcessId pid, int lid, const string& type, const string& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::DictionaryValue* record = FindRecord(pid, lid);
  if (!record)
    return;

  if (type == "iceConnectionStateChange") {
    if (value == "connected" || value == "checking" || value == "completed") {
      MaybeMarkPeerConnectionAsConnected(record);
    } else if (value == "failed" || value == "disconnected" ||
               value == "closed" || value == "new") {
      MaybeMarkPeerConnectionAsNotConnected(record);
    }
  } else if (type == "stop") {
    MaybeClosePeerConnection(record);
  }

  // Don't update entries if there aren't any observers.
  if (!observers_.might_have_observers())
    return;

  auto log_entry = std::make_unique<base::DictionaryValue>();

  double epoch_time = base::Time::Now().ToJsTime();
  string time = base::NumberToString(epoch_time);
  log_entry->SetString("time", time);
  log_entry->SetString("type", type);
  log_entry->SetString("value", value);

  auto update = std::make_unique<base::DictionaryValue>();
  update->SetInteger("pid", static_cast<int>(pid));
  update->SetInteger("lid", lid);
  update->MergeDictionary(log_entry.get());

  SendUpdate("updatePeerConnection", std::move(update));

  // Append the update to the end of the log.
  EnsureLogList(record)->Append(std::move(log_entry));
}

void WebRTCInternals::OnAddStandardStats(base::ProcessId pid,
                                         int lid,
                                         base::Value value) {
  if (!observers_.might_have_observers())
    return;

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);

  dict->SetKey("reports", std::move(value));

  SendUpdate("addStandardStats", std::move(dict));
}

void WebRTCInternals::OnAddLegacyStats(base::ProcessId pid,
                                       int lid,
                                       base::Value value) {
  if (!observers_.might_have_observers())
    return;

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetInteger("lid", lid);

  dict->SetKey("reports", std::move(value));

  SendUpdate("addLegacyStats", std::move(dict));
}

void WebRTCInternals::OnGetUserMedia(int rid,
                                     base::ProcessId pid,
                                     const std::string& origin,
                                     bool audio,
                                     bool video,
                                     const std::string& audio_constraints,
                                     const std::string& video_constraints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (get_user_media_requests_.GetList().size() >= kMaxGetUserMediaEntries) {
    LOG(WARNING) << "Maximum number of tracked getUserMedia() requests reached "
                    "in webrtc-internals.";
    return;
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("rid", rid);
  dict->SetInteger("pid", static_cast<int>(pid));
  dict->SetString("origin", origin);
  dict->SetDouble("timestamp", base::Time::Now().ToJsTime());
  if (audio)
    dict->SetString("audio", audio_constraints);
  if (video)
    dict->SetString("video", video_constraints);

  if (observers_.might_have_observers())
    SendUpdate("addGetUserMedia", dict->CreateDeepCopy());

  get_user_media_requests_.Append(std::move(dict));

  if (render_process_id_set_.insert(rid).second) {
    RenderProcessHost* host = RenderProcessHost::FromID(rid);
    if (host)
      host->AddObserver(this);
  }
}

void WebRTCInternals::AddObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void WebRTCInternals::RemoveObserver(WebRTCInternalsUIObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
  if (observers_.might_have_observers())
    return;

  // Disables event log and audio debug recordings if enabled and the last
  // webrtc-internals page is going away.
  DisableAudioDebugRecordings();
  DisableLocalEventLogRecordings();

  // TODO(tommi): Consider removing all the peer_connection_data_.
  for (auto& dictionary : peer_connection_data_)
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
  if (peer_connection_data_.GetSize() > 0)
    observer->OnUpdate("updateAllPeerConnections", &peer_connection_data_);

  for (const auto& request : get_user_media_requests_) {
    observer->OnUpdate("addGetUserMedia", &request);
  }
}

void WebRTCInternals::EnableAudioDebugRecordings(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  EnableAudioDebugRecordingsOnAllRenderProcessHosts();
#else
  selection_type_ = SelectionType::kAudioDebugRecordings;
  DCHECK(!select_file_dialog_);
  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
      audio_debug_recordings_file_path_, nullptr, 0,
      base::FilePath::StringType(), web_contents->GetTopLevelNativeWindow(),
      nullptr);
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
  DCHECK(CanToggleEventLogRecordings());
#if defined(OS_ANDROID)
  WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->EnableLocalLogging(event_log_recordings_file_path_,
                               base::OnceCallback<void(bool)>());
  }
#else
  DCHECK(web_contents);
  DCHECK(!select_file_dialog_);
  selection_type_ = SelectionType::kRtcEventLogs;
  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
      event_log_recordings_file_path_, nullptr, 0, FILE_PATH_LITERAL(""),
      web_contents->GetTopLevelNativeWindow(), nullptr);
#endif
}

void WebRTCInternals::DisableLocalEventLogRecordings() {
  event_log_recordings_ = false;
  // Tear down the dialog since the user has unchecked the event log checkbox.
  select_file_dialog_ = nullptr;
  DCHECK(CanToggleEventLogRecordings());
  WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
  if (logger) {
    logger->DisableLocalLogging(base::OnceCallback<void(bool)>());
  }
}

bool WebRTCInternals::IsEventLogRecordingsEnabled() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return event_log_recordings_;
}

bool WebRTCInternals::CanToggleEventLogRecordings() const {
  return command_line_derived_logging_path_.empty();
}

void WebRTCInternals::SendUpdate(const char* command,
                                 std::unique_ptr<base::Value> value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observers_.might_have_observers());

  bool queue_was_empty = pending_updates_.empty();
  pending_updates_.push(PendingUpdate(command, std::move(value)));

  if (queue_was_empty) {
    base::PostDelayedTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WebRTCInternals::ProcessPendingUpdates,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(aggregate_updates_ms_));
  }
}

void WebRTCInternals::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnRendererExit(host->GetID());
  render_process_id_set_.erase(host->GetID());
  host->RemoveObserver(this);
}

void WebRTCInternals::FileSelected(const base::FilePath& path,
                                   int /* unused_index */,
                                   void* /*unused_params */) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (selection_type_) {
    case SelectionType::kRtcEventLogs: {
      event_log_recordings_file_path_ = path;
      event_log_recordings_ = true;
      WebRtcEventLogger* const logger = WebRtcEventLogger::Get();
      if (logger) {
        logger->EnableLocalLogging(path, base::OnceCallback<void(bool)>());
      }
      break;
    }
    case SelectionType::kAudioDebugRecordings: {
      audio_debug_recordings_file_path_ = path;
      EnableAudioDebugRecordingsOnAllRenderProcessHosts();
      break;
    }
    default: { NOTREACHED(); }
  }
}

void WebRTCInternals::FileSelectionCanceled(void* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (selection_type_) {
    case SelectionType::kRtcEventLogs:
      SendUpdate("eventLogRecordingsFileSelectionCancelled", nullptr);
      break;
    case SelectionType::kAudioDebugRecordings:
      SendUpdate("audioDebugRecordingsFileSelectionCancelled", nullptr);
      break;
    default:
      NOTREACHED();
  }
  select_file_dialog_ = nullptr;
}

void WebRTCInternals::OnRendererExit(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterates from the end of the list to remove the PeerConnections created
  // by the exitting renderer.
  for (int i = peer_connection_data_.GetSize() - 1; i >= 0; --i) {
    base::DictionaryValue* record = nullptr;
    peer_connection_data_.GetDictionary(i, &record);

    int this_rid = 0;
    record->GetInteger("rid", &this_rid);

    if (this_rid == render_process_id) {
      if (observers_.might_have_observers()) {
        int lid = 0, pid = 0;
        record->GetInteger("lid", &lid);
        record->GetInteger("pid", &pid);

        std::unique_ptr<base::DictionaryValue> update(
            new base::DictionaryValue());
        update->SetInteger("lid", lid);
        update->SetInteger("pid", pid);
        SendUpdate("removePeerConnection", std::move(update));
      }
      MaybeClosePeerConnection(record);
      peer_connection_data_.Remove(i, nullptr);
    }
  }
  UpdateWakeLock();

  bool found_any = false;
  // Iterates from the end of the list to remove the getUserMedia requests
  // created by the exiting renderer.
  for (int i = get_user_media_requests_.GetSize() - 1; i >= 0; --i) {
    base::DictionaryValue* record = nullptr;
    get_user_media_requests_.GetDictionary(i, &record);

    int this_rid = 0;
    record->GetInteger("rid", &this_rid);

    if (this_rid == render_process_id) {
      get_user_media_requests_.Remove(i, nullptr);
      found_any = true;
    }
  }

  if (found_any && observers_.might_have_observers()) {
    std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue());
    update->SetInteger("rid", render_process_id);
    SendUpdate("removeGetUserMediaForRenderer", std::move(update));
  }
}

void WebRTCInternals::EnableAudioDebugRecordingsOnAllRenderProcessHosts() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!audio_debug_recording_session_);
  audio_debug_recording_session_ = audio::CreateAudioDebugRecordingSession(
      audio_debug_recordings_file_path_, GetSystemConnector()->Clone());

  for (RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->EnableAudioDebugRecordings(
        audio_debug_recordings_file_path_);
  }
}

void WebRTCInternals::MaybeClosePeerConnection(base::DictionaryValue* record) {
  bool is_open;
  bool did_read = record->GetBoolean("isOpen", &is_open);
  DCHECK(did_read);
  if (!is_open)
    return;

  record->SetBoolean("isOpen", false);
  MaybeMarkPeerConnectionAsNotConnected(record);
}

void WebRTCInternals::MaybeMarkPeerConnectionAsConnected(
    base::DictionaryValue* record) {
  bool was_connected = true;
  record->GetBoolean("connected", &was_connected);
  if (!was_connected) {
    ++num_connected_connections_;
    record->SetBoolean("connected", true);
    UpdateWakeLock();
    for (auto& observer : connections_observers_)
      observer.OnConnectionsCountChange(num_connected_connections_);
  }
}

void WebRTCInternals::MaybeMarkPeerConnectionAsNotConnected(
    base::DictionaryValue* record) {
  bool was_connected = false;
  record->GetBoolean("connected", &was_connected);
  if (was_connected) {
    record->SetBoolean("connected", false);
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
  // Here is a lazy binding, and will not reconnect after connection error.
  if (!wake_lock_) {
    mojo::PendingReceiver<device::mojom::WakeLock> receiver =
        wake_lock_.BindNewPipeAndPassReceiver();
    // In some testing environments, the system Connector isn't initialized.
    service_manager::Connector* connector = GetSystemConnector();
    if (connector) {
      mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
      connector->Connect(device::mojom::kServiceName,
                         wake_lock_provider.BindNewPipeAndPassReceiver());
      wake_lock_provider->GetWakeLockWithoutContext(
          device::mojom::WakeLockType::kPreventAppSuspension,
          device::mojom::WakeLockReason::kOther,
          "WebRTC has active PeerConnections", std::move(receiver));
    }
  }
  return wake_lock_.get();
}

void WebRTCInternals::ProcessPendingUpdates() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!pending_updates_.empty()) {
    const auto& update = pending_updates_.front();
    for (auto& observer : observers_)
      observer.OnUpdate(update.command(), update.value());
    pending_updates_.pop();
  }
}

base::DictionaryValue* WebRTCInternals::FindRecord(
    ProcessId pid,
    int lid,
    size_t* index /*= nullptr*/) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::DictionaryValue* record = nullptr;
  for (size_t i = 0; i < peer_connection_data_.GetSize(); ++i) {
    peer_connection_data_.GetDictionary(i, &record);

    int this_pid = 0, this_lid = 0;
    record->GetInteger("pid", &this_pid);
    record->GetInteger("lid", &this_lid);

    if (this_pid == static_cast<int>(pid) && this_lid == lid) {
      if (index)
        *index = i;
      return record;
    }
  }
  return nullptr;
}
}  // namespace content
