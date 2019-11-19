// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace media {
class AudioDebugRecordingSession;
}

namespace content {

class WebContents;
class WebRtcInternalsConnectionsObserver;
class WebRTCInternalsUIObserver;

// This is a singleton class running in the browser UI thread.
// It collects peer connection infomation from the renderers,
// forwards the data to WebRTCInternalsUIObserver and
// sends data collecting commands to the renderers.
class CONTENT_EXPORT WebRTCInternals : public RenderProcessHostObserver,
                                       public ui::SelectFileDialog::Listener {
 public:
  // * CreateSingletonInstance() ensures that no previous instantiation of the
  //   class was performed, then instantiates the class and returns the object.
  // * GetInstance() returns the object previously constructed using
  //   CreateSingletonInstance(). It may return null in tests.
  // * Creation is separated from access because WebRTCInternals may only be
  //   created from a context that allows blocking. If GetInstance were allowed
  //   to instantiate, as with a lazily constructed singleton, it would be
  //   difficult to guarantee that its construction is always first attempted
  //   from a context that allows it.
  static WebRTCInternals* CreateSingletonInstance();
  static WebRTCInternals* GetInstance();

  ~WebRTCInternals() override;

  // This method is called when a PeerConnection is created.
  // |render_process_id| is the id of the render process (not OS pid), which is
  // needed because we might not be able to get the OS process id when the
  // render process terminates and we want to clean up.
  // |pid| is the renderer process id, |lid| is the renderer local id used to
  // identify a PeerConnection, |url| is the url of the tab owning the
  // PeerConnection, |rtc_configuration| is the serialized RTCConfiguration,
  // |constraints| is the media constraints used to initialize the
  // PeerConnection.
  void OnAddPeerConnection(int render_process_id,
                           base::ProcessId pid,
                           int lid,
                           const std::string& url,
                           const std::string& rtc_configuration,
                           const std::string& constraints);

  // This method is called when PeerConnection is destroyed.
  // |pid| is the renderer process id, |lid| is the renderer local id.
  void OnRemovePeerConnection(base::ProcessId pid, int lid);

  // This method is called when a PeerConnection is updated.
  // |pid| is the renderer process id, |lid| is the renderer local id,
  // |type| is the update type, |value| is the detail of the update.
  void OnUpdatePeerConnection(base::ProcessId pid,
                              int lid,
                              const std::string& type,
                              const std::string& value);

  // These methods are called when results from
  // PeerConnectionInterface::GetStats (legacy or standard API) are available.
  // |pid| is the renderer process id, |lid| is the renderer local id, |value|
  // is the list of stats reports.
  void OnAddStandardStats(base::ProcessId pid, int lid, base::Value value);
  void OnAddLegacyStats(base::ProcessId pid, int lid, base::Value value);

  // This method is called when getUserMedia is called. |render_process_id| is
  // the id of the render process (not OS pid), which is needed because we might
  // not be able to get the OS process id when the render process terminates and
  // we want to clean up. |pid| is the renderer OS process id, |origin| is the
  // security origin of the getUserMedia call, |audio| is true if audio stream
  // is requested, |video| is true if the video stream is requested,
  // |audio_constraints| is the constraints for the audio, |video_constraints|
  // is the constraints for the video.
  void OnGetUserMedia(int render_process_id,
                      base::ProcessId pid,
                      const std::string& origin,
                      bool audio,
                      bool video,
                      const std::string& audio_constraints,
                      const std::string& video_constraints);

  // Methods for adding or removing WebRTCInternalsUIObserver.
  void AddObserver(WebRTCInternalsUIObserver* observer);
  void RemoveObserver(WebRTCInternalsUIObserver* observer);

  // Methods for adding or removing WebRtcInternalsConnectionsObserver.
  // |observer| is notified when there is a change in the count of active WebRTC
  // connections.
  void AddConnectionsObserver(WebRtcInternalsConnectionsObserver* observer);
  void RemoveConnectionsObserver(WebRtcInternalsConnectionsObserver* observer);

  // Sends all update data to |observer|.
  void UpdateObserver(WebRTCInternalsUIObserver* observer);

  // Enables or disables diagnostic audio recordings for debugging purposes.
  void EnableAudioDebugRecordings(content::WebContents* web_contents);
  void DisableAudioDebugRecordings();

  bool IsAudioDebugRecordingsEnabled() const;
  const base::FilePath& GetAudioDebugRecordingsFilePath() const;

  // Enables or disables diagnostic event log.
  void EnableLocalEventLogRecordings(content::WebContents* web_contents);
  void DisableLocalEventLogRecordings();

  bool IsEventLogRecordingsEnabled() const;
  bool CanToggleEventLogRecordings() const;

  int num_connected_connections() const { return num_connected_connections_; }

 protected:
  // Constructor/Destructor are protected to allow tests to derive from the
  // class and do per-instance testing without having to use the global
  // instance.
  // The default ctor sets |aggregate_updates_ms| to 500ms.
  WebRTCInternals();
  WebRTCInternals(int aggregate_updates_ms, bool should_block_power_saving);

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

 private:
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           CallWithAudioDebugRecordings);
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           CallWithAudioDebugRecordingsEnabledThenDisabled);
  FRIEND_TEST_ALL_PREFIXES(WebRtcAudioDebugRecordingsBrowserTest,
                           TwoCallsWithAudioDebugRecordings);
  FRIEND_TEST_ALL_PREFIXES(WebRtcInternalsTest,
                           AudioDebugRecordingsFileSelectionCanceled);

  static WebRTCInternals* g_webrtc_internals;

  void SendUpdate(const char* command,
                  std::unique_ptr<base::Value> value);

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* unused_params) override;
  void FileSelectionCanceled(void* params) override;

  // Called when a renderer exits (including crashes).
  void OnRendererExit(int render_process_id);

  // Enables diagnostic audio recordings on all render process hosts using
  // |audio_debug_recordings_file_path_|.
  void EnableAudioDebugRecordingsOnAllRenderProcessHosts();

  // Updates the number of open PeerConnections. Called when a PeerConnection
  // is stopped or removed.
  void MaybeClosePeerConnection(base::DictionaryValue* record);

  void MaybeMarkPeerConnectionAsConnected(base::DictionaryValue* record);
  void MaybeMarkPeerConnectionAsNotConnected(base::DictionaryValue* record);

  // Called whenever a PeerConnection is created or stopped in order to
  // request/cancel a wake lock on suspending the current application for power
  // saving.
  void UpdateWakeLock();

  device::mojom::WakeLock* GetWakeLock();

  // Called on a timer to deliver updates to javascript.
  // We throttle and bulk together updates to avoid DOS like scenarios where
  // a page uses a lot of peerconnection instances with many event
  // notifications.
  void ProcessPendingUpdates();

  base::DictionaryValue* FindRecord(base::ProcessId pid,
                                    int lid,
                                    size_t* index = nullptr);

  base::ObserverList<WebRTCInternalsUIObserver>::Unchecked observers_;

  base::ObserverList<WebRtcInternalsConnectionsObserver> connections_observers_;

  // |peer_connection_data_| is a list containing all the PeerConnection
  // updates.
  // Each item of the list represents the data for one PeerConnection, which
  // contains these fields:
  // "rid" -- the renderer id.
  // "pid" -- OS process id of the renderer that creates the PeerConnection.
  // "lid" -- local Id assigned to the PeerConnection.
  // "url" -- url of the web page that created the PeerConnection.
  // "servers" and "constraints" -- server configuration and media constraints
  // used to initialize the PeerConnection respectively.
  // "log" -- a ListValue contains all the updates for the PeerConnection. Each
  // list item is a DictionaryValue containing "time", which is the number of
  // milliseconds since epoch as a string, and "type" and "value", both of which
  // are strings representing the event.
  base::ListValue peer_connection_data_;

  // A list of getUserMedia requests. Each item is a DictionaryValue that
  // contains these fields:
  // "rid" -- the renderer id.
  // "pid" -- proceddId of the renderer.
  // "origin" -- the security origin of the request.
  // "audio" -- the serialized audio constraints if audio is requested.
  // "video" -- the serialized video constraints if video is requested.
  base::ListValue get_user_media_requests_;

  // For managing select file dialog.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  enum class SelectionType {
    kRtcEventLogs,
    kAudioDebugRecordings
  } selection_type_;

  // Diagnostic audio recording state.
  base::FilePath audio_debug_recordings_file_path_;
  std::unique_ptr<media::AudioDebugRecordingSession>
      audio_debug_recording_session_;

  // If non-empty, WebRTC (local) event logging should be enabled using this
  // path, and may not be turned off, except by restarting the browser.
  const base::FilePath command_line_derived_logging_path_;

  // Diagnostic event log recording state. These are meaningful only when
  // |command_line_derived_logging_path_| is empty.
  bool event_log_recordings_;
  base::FilePath event_log_recordings_file_path_;

  // While |num_connected_connections_| is greater than zero, request a wake
  // lock service. This prevents the application from being suspended while
  // remoting.
  int num_connected_connections_;
  const bool should_block_power_saving_;

  // Set of render process hosts that |this| is registered as an observer on.
  std::unordered_set<int> render_process_id_set_;

  // Used to bulk up updates that we send to javascript.
  // The class owns the value/dictionary and command name of an update.
  // For each update, a PendingUpdate is stored in the |pending_updates_| queue
  // and deleted as soon as the update has been delivered.
  // The class is moveble and not copyable to avoid copying while still allowing
  // us to use an stl container without needing scoped_ptr or similar.
  // The class is single threaded, so all operations must occur on the same
  // thread.
  class PendingUpdate {
   public:
    PendingUpdate(const char* command,
                  std::unique_ptr<base::Value> value);
    PendingUpdate(PendingUpdate&& other);
    ~PendingUpdate();

    const char* command() const;
    const base::Value* value() const;

   private:
    base::ThreadChecker thread_checker_;
    const char* command_;
    std::unique_ptr<base::Value> value_;
    DISALLOW_COPY_AND_ASSIGN(PendingUpdate);
  };

  base::queue<PendingUpdate> pending_updates_;
  const int aggregate_updates_ms_;

  // Weak factory for this object that we use for bulking up updates.
  base::WeakPtrFactory<WebRTCInternals> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_H_
