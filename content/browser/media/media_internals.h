// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/scoped_multi_source_observation.h"
#include "base/synchronization/lock.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "content/browser/media/media_internals_audio_focus_helper.h"
#include "content/browser/media/media_internals_cdm_helper.h"
#include "content/common/content_export.h"
#include "content/common/media/media_log_records.mojom.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "media/audio/audio_logging.h"
#include "media/base/media_log.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video_capture_types.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {
struct MediaLogRecord;
}

namespace media_session {
namespace mojom {
enum class AudioFocusType;
}  // namespace mojom
}  // namespace media_session

namespace content {

// This class stores information about currently active media.
// TODO(crbug.com/40563083): Remove inheritance from media::AudioLogFactory once
// the creation of the AudioManager instance moves to the audio service.
class CONTENT_EXPORT MediaInternals : public media::AudioLogFactory,
                                      public RenderProcessHostCreationObserver,
                                      public RenderProcessHostObserver {
 public:
  // Called with the update string.
  using UpdateCallback = base::RepeatingCallback<void(const std::u16string&)>;

  static MediaInternals* GetInstance();

  MediaInternals(const MediaInternals&) = delete;
  MediaInternals& operator=(const MediaInternals&) = delete;

  ~MediaInternals() override;

  // RenderProcessHostCreationObserver implementation.
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Called when a MediaEvent occurs.
  void OnMediaEvents(int render_process_id,
                     const std::vector<media::MediaLogRecord>& events);

  // Add/remove update callbacks (see above). Must be called on the UI thread.
  // The callbacks must also be fired on UI thread.
  void AddUpdateCallback(UpdateCallback callback);
  void RemoveUpdateCallback(const UpdateCallback& callback);

  // Whether there are any update callbacks available. Can be called on any
  // thread.
  bool CanUpdate();

  // Replay all saved media events.
  void SendHistoricalMediaEvents();

  // Sends general audio information to each registered UpdateCallback.
  void SendGeneralAudioInformation();

  // Sends all audio cached data to each registered UpdateCallback.
  void SendAudioStreamData();

  // Sends all video capture capabilities cached data to each registered
  // UpdateCallback.
  void SendVideoCaptureDeviceCapabilities();

  // Sends all audio focus information to each registered UpdateCallback.
  void SendAudioFocusState();

  // Get information of registered CDMs and update the "CDMs" tab.
  void GetRegisteredCdms();

  // Called to inform of the capabilities enumerated for video devices.
  void UpdateVideoCaptureDeviceCapabilities(
      const std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                                   media::VideoCaptureFormats>>&
          descriptors_and_formats);

  // media::AudioLogFactory implementation.  Safe to call from any thread.
  std::unique_ptr<media::AudioLog> CreateAudioLog(AudioComponent component,
                                                  int component_id) override;

  // Creates a PendingRemote<media::mojom::AudioLog> strongly bound to a new
  // media::mojom::AudioLog instance. Safe to call from any thread.
  mojo::PendingRemote<media::mojom::AudioLog> CreateMojoAudioLog(
      AudioComponent component,
      int component_id,
      int render_process_id = -1,
      int render_frame_id = MSG_ROUTING_NONE);

  // Strongly bounds |receiver| to a new media::mojom::AudioLog instance. Safe
  // to call from any thread.
  void CreateMojoAudioLog(
      AudioComponent component,
      int component_id,
      mojo::PendingReceiver<media::mojom::AudioLog> receiver,
      int render_process_id = -1,
      int render_frame_id = MSG_ROUTING_NONE);

  static void CreateMediaLogRecords(
      int render_process_id,
      mojo::PendingReceiver<content::mojom::MediaInternalLogRecords> receiver);

 private:
  // Needs access to SendUpdate.
  friend class MediaInternalsAudioFocusHelper;
  friend class MediaInternalsCdmHelper;

  class AudioLogImpl;
  class MediaInternalLogRecordsImpl;
  // Inner class to handle reporting pipelinestatus to UMA
  class MediaInternalsUMAHandler;

  MediaInternals();

  // Sends |update| to each registered UpdateCallback.  Safe to call from any
  // thread, but will forward to the IO thread.
  void SendUpdate(const std::u16string& update);

  // Saves |event| so that it can be sent later in SendHistoricalMediaEvents().
  void SaveEvent(int process_id, const media::MediaLogRecord& event);

  // Erases saved events for |host|, if any.
  void EraseSavedEvents(RenderProcessHost* host);

  // Caches |value| under |cache_key| so that future UpdateAudioLog() calls
  // will include the current data.  Calls JavaScript |function|(|value|) for
  // each registered UpdateCallback (if any).
  enum AudioLogUpdateType {
    CREATE,             // Creates a new AudioLog cache entry.
    UPDATE_IF_EXISTS,   // Updates an existing AudioLog cache entry, does
                        // nothing if it doesn't exist.
    UPDATE_AND_DELETE,  // Deletes an existing AudioLog cache entry.
  };
  void UpdateAudioLog(AudioLogUpdateType type,
                      std::string_view cache_key,
                      std::string_view function,
                      const base::Value::Dict& value);

  std::unique_ptr<AudioLogImpl> CreateAudioLogImpl(AudioComponent component,
                                                   int component_id,
                                                   int render_process_id,
                                                   int render_frame_id);

  // Must only be accessed on the UI thread.
  std::vector<UpdateCallback> update_callbacks_;

  // Saved events by process ID for showing recent players in the UI.
  std::map<int, std::list<media::MediaLogRecord>> saved_events_by_process_;

  // Must only be accessed on the IO thread.
  base::Value::List video_capture_capabilities_cached_data_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};

  MediaInternalsAudioFocusHelper audio_focus_helper_;

  MediaInternalsCdmHelper cdm_helper_;

  // All variables below must be accessed under |lock_|.
  base::Lock lock_;
  bool can_update_ = false;
  base::Value::Dict audio_streams_cached_data_;
  int owner_ids_[base::to_underlying(
      media::AudioLogFactory::AudioComponent::kAudiocomponentMax)] = {};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
